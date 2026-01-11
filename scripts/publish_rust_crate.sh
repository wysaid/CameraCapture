#!/usr/bin/env bash

# Publish the Rust crate (bindings/rust) to crates.io.
#
# Features:
# - Environment checks (cargo/rustup, rustfmt/clippy)
# - Detect first publish vs update (crates.io existence)
# - Branch-aware version defaulting (non-main => pre-release '-test.*')
# - -y/--yes for non-interactive default choices
# - Build & test pipeline (static-link + build-source)
# - Package list + dry-run publish before real publish

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
RUST_DIR="${PROJECT_ROOT}/bindings/rust"
CARGO_TOML="${RUST_DIR}/Cargo.toml"

DEFAULT_YES=0
REQUESTED_VERSION=""
DO_PUBLISH=1
ALLOW_NON_SELF_CONTAINED=0
ALLOW_DEFAULT_STATIC_LINK=0
PUBLISH_MODE=""
AUTO_PREPARE=0
KEEP_NATIVE=0
PUBLISH_TEST_FLAG=0

usage() {
    cat <<'USAGE'
Usage:
    scripts/publish_rust_crate.sh [-y|--yes] [--mode <mode>] [--version <semver>] [--dry-run]
                                   [--publish] [--keep-native]
                                   [--allow-non-self-contained] [--allow-default-static-link]

Options:
  -y, --yes         Accept defaults (non-interactive).
  --publish         When combined with -y, enables one-step test version publish
                    (auto-appends -test.<timestamp>.<sha> suffix).
  --mode <mode>     Publish workflow preset. One of:
                                     - dry-run   : Run verification only (no publish). Auto-prepares crate for real-world packaging.
                                     - preview   : Publish a pre-release (recommended on non-main). Auto-prepares native sources.
                                     - stable    : Publish a stable release (recommended on main). Auto-prepares native sources.
                                     - expert    : Publish as-is (may be non-self-contained / static-link default). Requires overrides.
  --version <ver>   Target version to publish (e.g. 1.5.0-alpha.1).
                   If not on main and <ver> has no pre-release part, the script
                   will append a '-test.<timestamp>.<sha>' suffix by default.
  --dry-run         Only run 'cargo publish --dry-run' (no actual publish).

    --keep-native      Keep bindings/rust/native/ after the run if it was auto-created.

    --allow-non-self-contained
                                     Allow publishing even if bindings/rust/native/ is missing.
                                     (NOT recommended for stable releases; intended for preview/testing only.)

    --allow-default-static-link
                                     Allow publishing even if Cargo.toml default features include
                                     'static-link' (which is typically not crates.io-friendly).

Notes:
  - When '-y --publish' are used together, automatically publishes a test version
    with -test.<timestamp>.<sha> suffix without further prompts (preview mode).
  - This script publishes whatever package name is in bindings/rust/Cargo.toml.
  - crates.io disallows overwriting an existing version; each publish must use a
    new unique version.
USAGE
}

log() {
    printf "%s\n" "$*"
}

warn() {
    printf "WARN: %s\n" "$*" >&2
}

die() {
    printf "ERROR: %s\n" "$*" >&2
    exit 1
}

prompt_yn() {
    # prompt_yn "Question?" default(Y/N)
    local prompt="$1"
    local def="$2"
    local ans
    local hint

    # Display default as uppercase, alternative as lowercase, e.g. "Y/n" or "y/N".
    if [[ "${def}" == "Y" ]]; then
        hint="Y/n"
    else
        hint="y/N"
    fi

    if [[ "${DEFAULT_YES}" -eq 1 ]]; then
        [[ "${def}" == "Y" ]] && return 0 || return 1
    fi

    while true; do
        read -r -p "${prompt} [${hint}]: " ans || true
        ans="${ans:-$def}"
        case "${ans}" in
        Y | y) return 0 ;;
        N | n) return 1 ;;
        *) echo "Please answer Y or N." ;;
        esac
    done
}

read_default_features() {
    # Print the default feature list as a comma-separated string, or empty.
    python3 - "$CARGO_TOML" <<'PY'
import re, sys, pathlib

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding='utf-8')

m = re.search(r'(?ms)^\[features\]\s*(.*?)(?=^\[|\Z)', text)
if not m:
    print("")
    raise SystemExit(0)

feat = m.group(1)
dm = re.search(r'(?m)^\s*default\s*=\s*\[(.*?)\]\s*$', feat)
if not dm:
    print("")
    raise SystemExit(0)

items = []
for raw in dm.group(1).split(','):
    s = raw.strip().strip('"').strip("'")
    if s:
        items.append(s)

print(','.join(items))
PY
}

csv_has_item() {
    # csv_has_item "a,b,c" "b"
    local csv="$1"
    local item="$2"
    [[ ",${csv}," == *",${item},"* ]]
}

set_default_features() {
    # set_default_features "build-source" "static-link"
    # Replaces [features].default list with provided items.
    python3 - "$CARGO_TOML" "$@" <<'PY'
import re, sys, pathlib

path = pathlib.Path(sys.argv[1])
items = [x for x in sys.argv[2:] if x]
text = path.read_text(encoding='utf-8')

m = re.search(r'(?ms)^\[features\]\s*(.*?)(?=^\[|\Z)', text)
if not m:
    raise SystemExit("[features] section not found in Cargo.toml")

feat_body = m.group(1)
new_default = 'default = [' + ', '.join([f'"{x}"' for x in items]) + ']'

if re.search(r'(?m)^\s*default\s*=\s*\[.*?\]\s*$', feat_body):
    feat_body2, n = re.subn(r'(?m)^\s*default\s*=\s*\[.*?\]\s*$', new_default, feat_body, count=1)
    if n != 1:
        raise SystemExit("Failed to update [features].default")
else:
    # Insert at top of [features] section
    feat_body2 = new_default + "\n" + feat_body

new_text = text[:m.start()] + "[features]\n" + feat_body2 + text[m.end():]
path.write_text(new_text, encoding='utf-8')
PY
}

vendor_native_sources() {
    # vendor_native_sources <dest_native_dir>
    # Copies <repo>/include and <repo>/src into bindings/rust/native/{include,src}
    local dest="$1"
    local src_root="${PROJECT_ROOT}"

    [[ -d "${src_root}/include" ]] || die "Cannot vendor: missing ${src_root}/include"
    [[ -d "${src_root}/src" ]] || die "Cannot vendor: missing ${src_root}/src"

    mkdir -p "${dest}"
    rm -rf "${dest}/include" "${dest}/src"
    mkdir -p "${dest}/include" "${dest}/src"

    # Use tar for robust, fast directory copy without requiring rsync.
    (cd "${src_root}" && tar -cf - include src) | (cd "${dest}" && tar -xf -)
}

select_publish_mode() {
    # Returns selected mode string.
    if [[ "${DEFAULT_YES}" -eq 1 ]]; then
        # Safer default in non-interactive mode.
        printf "%s" "dry-run"
        return 0
    fi

    echo "Select publish workflow:" >&2
    echo "  1) dry-run  - verify only (no publish), auto-prepare crate packaging" >&2
    echo "  2) preview  - publish a pre-release, auto-prepare native + friendly defaults" >&2
    echo "  3) stable   - publish a stable release, auto-prepare native + friendly defaults" >&2
    echo "  4) expert   - publish as-is (requires override flags), not recommended" >&2

    local choice
    choice="$(prompt_text "Enter choice" "1")"
    case "${choice}" in
    1 | dry-run) printf "%s" "dry-run" ;;
    2 | preview) printf "%s" "preview" ;;
    3 | stable) printf "%s" "stable" ;;
    4 | expert) printf "%s" "expert" ;;
    *)
        warn "Unknown choice '${choice}', defaulting to dry-run."
        printf "%s" "dry-run"
        ;;
    esac
}

prompt_text() {
    # prompt_text "Prompt" "default"
    local prompt="$1"
    local def="$2"
    local ans

    if [[ "${DEFAULT_YES}" -eq 1 ]]; then
        printf "%s" "${def}"
        return 0
    fi

    read -r -p "${prompt} [default: ${def}]: " ans || true
    ans="${ans:-$def}"
    printf "%s" "${ans}"
}

require_cmd() {
    local cmd="$1"
    command -v "${cmd}" >/dev/null 2>&1
}

install_rustup() {
    require_cmd curl || die "curl not found; please install curl first to auto-install Rust toolchain"

    log "Installing Rust toolchain via rustup..."
    curl https://sh.rustup.rs -sSf | sh -s -- -y
    # shellcheck disable=SC1090
    source "${HOME}/.cargo/env"
}

ensure_rust_toolchain() {
    if ! require_cmd cargo; then
        warn "cargo not found."
        if prompt_yn "Install Rust toolchain now (rustup)?" "Y"; then
            install_rustup
        else
            die "cargo is required. Please install Rust (rustup) and re-run this script."
        fi
    fi

    if require_cmd rustup; then
        # Ensure rustfmt/clippy if possible
        if prompt_yn "Ensure rustfmt & clippy components are installed?" "Y"; then
            rustup component add rustfmt clippy >/dev/null 2>&1 || true
        fi
    else
        warn "rustup not found; skipping automatic rustfmt/clippy installation."
    fi
}

read_package_field() {
    # read_package_field <fieldname>
    python3 - "$CARGO_TOML" "$1" <<'PY'
import re, sys, pathlib

path = pathlib.Path(sys.argv[1])
field = sys.argv[2]
text = path.read_text(encoding='utf-8')

# Find the [package] section
m = re.search(r'(?ms)^\[package\]\s*(.*?)\n\[', text)
if not m:
    # fallback: [package] till EOF
    m = re.search(r'(?ms)^\[package\]\s*(.*)\Z', text)
if not m:
    print("")
    sys.exit(0)

pkg = m.group(1)
fm = re.search(rf'(?m)^\s*{re.escape(field)}\s*=\s*"([^"]+)"\s*$', pkg)
print(fm.group(1) if fm else "")
PY
}

set_package_version() {
    local new_ver="$1"
    python3 - "$CARGO_TOML" "$new_ver" <<'PY'
import re, sys, pathlib

path = pathlib.Path(sys.argv[1])
new_ver = sys.argv[2]
text = path.read_text(encoding='utf-8')

def repl(m):
    body = m.group(1)
    # Replace only the first version = "..." within [package]
    body2, n = re.subn(r'(?m)^(\s*version\s*=\s*")[^"]+("\s*)$', rf"\g<1>{new_ver}\2", body, count=1)
    if n != 1:
        raise SystemExit("Failed to update [package].version in Cargo.toml")
    return "[package]\n" + body2

m = re.search(r'(?ms)^\[package\]\s*(.*?)(?=^\[|\Z)', text)
if not m:
    raise SystemExit("[package] section not found in Cargo.toml")

new_text = text[:m.start()] + repl(m) + text[m.end():]
path.write_text(new_text, encoding='utf-8')
PY
}

cratesio_has_exact_crate() {
    local crate="$1"
    # cargo search output begins with: <name> = "<ver>"  ...
    local out
    out="$(cargo search "^${crate}$" --limit 10 2>/dev/null || true)"
    echo "${out}" | grep -E "^${crate} = \"" >/dev/null 2>&1
}

cratesio_latest_version() {
    local crate="$1"
    local out
    out="$(cargo search "^${crate}$" --limit 10 2>/dev/null || true)"
    # Extract the first exact match version if present
    echo "${out}" | sed -n -E "s/^${crate} = \"([^\"]+)\".*$/\1/p" | head -n 1
}

git_branch() {
    git -C "${PROJECT_ROOT}" rev-parse --abbrev-ref HEAD 2>/dev/null || echo ""
}

git_short_sha() {
    git -C "${PROJECT_ROOT}" rev-parse --short HEAD 2>/dev/null || echo "nogit"
}

timestamp_utc() {
    date -u +%Y%m%d%H%M%S
}

strip_prerelease() {
    # strip_prerelease 1.2.3-alpha.1 => 1.2.3
    echo "$1" | sed -E 's/^([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

bump_patch() {
    local base="$1"
    local major minor patch
    IFS='.' read -r major minor patch <<<"${base}"
    [[ -n "${major}" && -n "${minor}" && -n "${patch}" ]] || return 1
    patch=$((patch + 1))
    echo "${major}.${minor}.${patch}"
}

ensure_ccap_static_lib_for_static_link() {
    # Only needed for default (static-link) mode.
    local debug_lib="${PROJECT_ROOT}/build/Debug/libccap.a"
    local release_lib="${PROJECT_ROOT}/build/Release/libccap.a"

    if [[ -f "${debug_lib}" || -f "${release_lib}" ]]; then
        return 0
    fi

    warn "Pre-built libccap.a not found (expected for static-link dev mode)."
    if ! prompt_yn "Build C++ library now (CMake Debug) so static-link tests can run?" "Y"; then
        warn "Skipping static-link tests due to missing libccap.a."
        return 1
    fi

    require_cmd cmake || die "cmake not found; please install cmake or build libccap.a manually"

    log "Configuring & building C++ library (Debug)..."
    mkdir -p "${PROJECT_ROOT}/build/Debug"
    cmake -S "${PROJECT_ROOT}" -B "${PROJECT_ROOT}/build/Debug" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCCAP_BUILD_EXAMPLES=OFF \
        -DCCAP_BUILD_TESTS=OFF \
        -DCCAP_BUILD_CLI=OFF >/dev/null
    cmake --build "${PROJECT_ROOT}/build/Debug" --config Debug --parallel "$(nproc 2>/dev/null || echo 4)"

    [[ -f "${debug_lib}" ]] || warn "libccap.a still not found after build; static-link tests may fail."
    return 0
}

ensure_logged_in_hint() {
    local cred1="${HOME}/.cargo/credentials.toml"
    local cred2="${HOME}/.cargo/credentials"

    if [[ ! -f "${cred1}" && ! -f "${cred2}" ]]; then
        warn "Cargo does not appear to be logged in (no credentials file found)."
        warn "If publish fails with an authentication error, run: cargo login <TOKEN>"
    fi
}

main() {
    local native_autocreated=0

    while [[ $# -gt 0 ]]; do
        case "$1" in
        -y | --yes)
            DEFAULT_YES=1
            shift
            ;;
        --publish)
            PUBLISH_TEST_FLAG=1
            shift
            ;;
        --mode)
            PUBLISH_MODE="${2:-}"
            [[ -n "${PUBLISH_MODE}" ]] || die "--mode requires a value"
            shift 2
            ;;
        --version)
            REQUESTED_VERSION="${2:-}"
            [[ -n "${REQUESTED_VERSION}" ]] || die "--version requires a value"
            shift 2
            ;;
        --dry-run)
            DO_PUBLISH=0
            shift
            ;;
        --keep-native)
            KEEP_NATIVE=1
            shift
            ;;
        --allow-non-self-contained)
            ALLOW_NON_SELF_CONTAINED=1
            shift
            ;;
        --allow-default-static-link)
            ALLOW_DEFAULT_STATIC_LINK=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            die "Unknown argument: $1 (use --help)"
            ;;
        esac
    done

    [[ -f "${CARGO_TOML}" ]] || die "Cargo.toml not found at ${CARGO_TOML}"

    ensure_rust_toolchain

    require_cmd git || warn "git not found; branch-aware defaults may be unavailable."

    local branch
    branch="$(git_branch)"
    local sha
    sha="$(git_short_sha)"
    local on_main=0
    if [[ "${branch}" == "main" ]]; then
        on_main=1
    fi

    local pkg_name
    pkg_name="$(read_package_field name)"
    [[ -n "${pkg_name}" ]] || die "Failed to read [package].name from Cargo.toml"

    local current_version
    current_version="$(read_package_field version)"
    [[ -n "${current_version}" ]] || die "Failed to read [package].version from Cargo.toml"

    log "Publishing crate from: ${RUST_DIR}"
    log "Package name      : ${pkg_name}"
    log "Current version   : ${current_version}"

    # Handle -y --publish combination: auto-publish test version
    if [[ "${DEFAULT_YES}" -eq 1 && "${PUBLISH_TEST_FLAG}" -eq 1 ]]; then
        log "Auto-publish mode detected (-y --publish): publishing test version"
        PUBLISH_MODE="preview"
        DO_PUBLISH=1
        AUTO_PREPARE=1

        # Force test version if not already specified
        if [[ -z "${REQUESTED_VERSION}" ]]; then
            local base
            base="$(strip_prerelease "${current_version}")"
            REQUESTED_VERSION="${base}-test.$(timestamp_utc).${sha}"
            log "Auto-generated test version: ${REQUESTED_VERSION}"
        elif [[ "${REQUESTED_VERSION}" != *-test.* ]]; then
            # Ensure test suffix exists
            REQUESTED_VERSION="${REQUESTED_VERSION}-test.$(timestamp_utc).${sha}"
            log "Appended test suffix: ${REQUESTED_VERSION}"
        fi
    fi

    if [[ -z "${PUBLISH_MODE}" ]]; then
        PUBLISH_MODE="$(select_publish_mode)"
    fi

    case "${PUBLISH_MODE}" in
    dry-run)
        DO_PUBLISH=0
        AUTO_PREPARE=1
        ;;
    preview)
        AUTO_PREPARE=1
        ;;
    stable)
        AUTO_PREPARE=1
        ;;
    expert)
        # No auto changes; requires explicit overrides.
        AUTO_PREPARE=0
        ;;
    *)
        warn "Unknown --mode '${PUBLISH_MODE}', defaulting to dry-run."
        DO_PUBLISH=0
        AUTO_PREPARE=1
        PUBLISH_MODE="dry-run"
        ;;
    esac

    if [[ "${PUBLISH_MODE}" == "stable" && "${on_main}" -ne 1 ]]; then
        warn "Mode 'stable' selected but you are not on main."
        if ! prompt_yn "Switch to preview mode automatically?" "Y"; then
            die "Refusing stable publish from non-main."
        fi
        PUBLISH_MODE="preview"
    fi

    # Correctness guard: default features should be crates.io-friendly.
    # If default includes static-link, typical users will fail without a pre-built libccap.
    local default_features
    default_features="$(read_default_features)"
    if [[ -n "${default_features}" ]] && csv_has_item "${default_features}" "static-link"; then
        warn "Cargo.toml default features include 'static-link' (${default_features})."
        warn "This is usually NOT crates.io-friendly: users won't have pre-built libccap.a by default."
        warn "Recommendation: publish with default = ['build-source'] (and keep 'static-link' for repo dev)."

        if [[ "${ALLOW_DEFAULT_STATIC_LINK}" -ne 1 ]]; then
            if [[ "${AUTO_PREPARE}" -eq 1 ]]; then
                if prompt_yn "Auto-fix Cargo.toml defaults to ['build-source'] for crates.io?" "Y"; then
                    set_default_features "build-source"
                    default_features="$(read_default_features)"
                    log "Updated Cargo.toml default features: ${default_features}"
                else
                    if [[ "${DO_PUBLISH}" -eq 1 ]]; then
                        die "Refusing to publish: default features include 'static-link'. Re-run with --allow-default-static-link to override (not recommended)."
                    else
                        warn "Continuing because this is a dry-run; publish would be refused unless overridden."
                    fi
                fi
            else
                if [[ "${DO_PUBLISH}" -eq 1 ]]; then
                    die "Refusing to publish: default features include 'static-link'. Re-run with --allow-default-static-link to override (not recommended)."
                else
                    warn "Continuing because this is a dry-run; publish would be refused unless overridden."
                fi
            fi
        fi
    fi

    if [[ -n "${branch}" ]]; then
        log "Git branch        : ${branch} (${sha})"
        if [[ "${on_main}" -ne 1 ]]; then
            warn "You are not on 'main'. It is recommended to publish only a pre-release (test/alpha/beta/rc)."
        fi
    fi

    # Determine whether this is first publish.
    local exists=0
    if cratesio_has_exact_crate "${pkg_name}"; then
        exists=1
        local remote_ver
        remote_ver="$(cratesio_latest_version "${pkg_name}")"
        log "crates.io status  : exists (latest: ${remote_ver})"
    else
        log "crates.io status  : not found (first publish)"
    fi

    if [[ "${exists}" -eq 0 ]]; then
        if ! prompt_yn "Crate '${pkg_name}' not found on crates.io. Publish as a NEW crate?" "Y"; then
            die "Aborted by user."
        fi
    fi

    # Propose a target version.
    local base
    base="$(strip_prerelease "${current_version}")"
    local default_target
    if [[ "${on_main}" -eq 1 ]]; then
        default_target="$(bump_patch "${base}" || echo "${base}")"
    else
        default_target="${base}-test.$(timestamp_utc).${sha}"
    fi

    local target_version
    if [[ -n "${REQUESTED_VERSION}" ]]; then
        target_version="${REQUESTED_VERSION}"
    else
        target_version="$(prompt_text "Enter version to publish" "${default_target}")"
    fi

    # If not on main, enforce pre-release by default.
    if [[ "${on_main}" -ne 1 ]]; then
        if [[ "${target_version}" != *-* ]]; then
            warn "Version '${target_version}' has no pre-release part and you are not on main."
            if prompt_yn "Append test suffix automatically?" "Y"; then
                target_version="${target_version}-test.$(timestamp_utc).${sha}"
            else
                die "Refusing to publish a stable version from a non-main branch. Use a pre-release (e.g. 1.5.0-test.1)."
            fi
        fi
    fi

    if [[ "${target_version}" == "${current_version}" ]]; then
        warn "Target version equals current version. crates.io will reject republishing the same version."
        if ! prompt_yn "Continue anyway?" "N"; then
            die "Please choose a new version."
        fi
    fi

    log "Version change    : ${current_version} -> ${target_version}"
    if ! prompt_yn "Update Cargo.toml to '${target_version}' and proceed?" "Y"; then
        die "Aborted by user."
    fi

    set_package_version "${target_version}"

    # Update lockfile if present (best-effort).
    if [[ -f "${RUST_DIR}/Cargo.lock" ]]; then
        (cd "${RUST_DIR}" && cargo update -p "${pkg_name}" >/dev/null 2>&1) || true
    fi

    ensure_logged_in_hint

    log "Running Rust checks & tests..."
    pushd "${RUST_DIR}" >/dev/null

    if [[ ! -d "${RUST_DIR}/native" ]]; then
        warn "bindings/rust/native/ not found."
        warn "This makes the published crate NOT self-contained for typical crates.io users."
        warn "Recommendation: vendor include/ + src/ into bindings/rust/native/ before publishing."

        if [[ "${AUTO_PREPARE}" -eq 1 ]]; then
            if prompt_yn "Auto-vendor native sources into bindings/rust/native now?" "Y"; then
                vendor_native_sources "${RUST_DIR}/native"
                native_autocreated=1
                log "Vendored native sources into bindings/rust/native/."
            else
                if [[ "${ALLOW_NON_SELF_CONTAINED}" -ne 1 ]]; then
                    if [[ "${DO_PUBLISH}" -eq 1 ]]; then
                        die "Refusing to publish: missing bindings/rust/native/. Re-run with --allow-non-self-contained for preview/testing only."
                    else
                        warn "Continuing because this is a dry-run; publish would be refused unless overridden."
                    fi
                fi
            fi
        else
            if [[ "${ALLOW_NON_SELF_CONTAINED}" -ne 1 ]]; then
                if [[ "${DO_PUBLISH}" -eq 1 ]]; then
                    die "Refusing to publish: missing bindings/rust/native/. Re-run with --allow-non-self-contained for preview/testing only."
                else
                    warn "Continuing because this is a dry-run; publish would be refused unless overridden."
                fi
            fi
        fi
    fi

    if require_cmd rustfmt; then
        cargo fmt --all
    else
        warn "rustfmt not available; skipping format check."
    fi

    if require_cmd cargo-clippy; then
        # Clippy on the crate's default feature set.
        cargo clippy --all-targets -- -D warnings
    else
        warn "clippy not available; skipping clippy."
    fi

    # Static-link (dev) mode: optional, may need pre-built libccap.a.
    # NOTE: When default features are set to build-source for crates.io friendliness,
    # running `--features static-link` without `--no-default-features` would enable
    # BOTH build-source and static-link, which does NOT actually validate the
    # pre-built-library (static-link) workflow.
    local did_static=0
    local run_static=0
    if [[ "${PUBLISH_MODE}" == "expert" ]]; then
        run_static=1
    else
        if prompt_yn "Also run static-link (dev) tests? (requires pre-built libccap.a)" "N"; then
            run_static=1
        fi
    fi

    if [[ "${run_static}" -eq 1 ]]; then
        if ensure_ccap_static_lib_for_static_link; then
            did_static=1
            cargo test --verbose --no-default-features --features static-link
        else
            did_static=0
        fi
    fi

    # Build-source (dist) mode: must work for crates.io.
    cargo test --verbose --no-default-features --features build-source

    # Build release artifacts as a sanity check.
    if [[ "${did_static}" -eq 1 ]]; then
        cargo build --release --no-default-features --features static-link
    fi
    cargo build --release --no-default-features --features build-source

    log "Packaging check (cargo package)..."
    cargo package --list >/dev/null
    # Validate crates.io build in distribution mode.
    cargo publish --dry-run --no-default-features --features build-source

    if [[ "${DO_PUBLISH}" -eq 0 ]]; then
        log "Dry-run completed (no publish requested)."
        if [[ "${native_autocreated}" -eq 1 && "${KEEP_NATIVE}" -ne 1 ]]; then
            rm -rf "${RUST_DIR}/native"
            log "Removed auto-created bindings/rust/native/ (use --keep-native to keep it)."
        fi
        popd >/dev/null
        return 0
    fi

    # Skip confirmation prompt for auto-publish mode
    if [[ "${PUBLISH_TEST_FLAG}" -ne 1 ]]; then
        if ! prompt_yn "Proceed to publish '${pkg_name}' version '${target_version}' to crates.io?" "N"; then
            die "Aborted by user after dry-run."
        fi
    else
        log "Auto-publishing test version to crates.io..."
    fi

    if cargo publish --no-default-features --features build-source; then
        log "✅ Publish succeeded: ${pkg_name} ${target_version}"
        log "Next: verify on crates.io and docs.rs."
    else
        warn "Publish failed. Common causes:"
        warn "- Not logged in: run 'cargo login <TOKEN>'"
        warn "- Version already exists on crates.io (must be unique)"
        warn "- Missing system deps for bindgen (libclang) on your machine"
        exit 1
    fi

    if [[ "${native_autocreated}" -eq 1 && "${KEEP_NATIVE}" -ne 1 ]]; then
        rm -rf "${RUST_DIR}/native"
        log "Removed auto-created bindings/rust/native/ (use --keep-native to keep it)."
    fi

    popd >/dev/null
}

main "$@"
