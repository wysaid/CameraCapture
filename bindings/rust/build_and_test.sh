#!/bin/bash

# Rust bindings build and test script for ccap

set -e

detect_cli_devices() {
    local cli_bin=""
    local original_dir
    original_dir="$(pwd)"

    # Prefer Debug build
    if [ -x "$PROJECT_ROOT/build/Debug/ccap" ]; then
        cli_bin="$PROJECT_ROOT/build/Debug/ccap"
    elif [ -x "$PROJECT_ROOT/build/Debug/ccap.exe" ]; then
        cli_bin="$PROJECT_ROOT/build/Debug/ccap.exe"
    elif [ -x "$PROJECT_ROOT/build/Release/ccap" ]; then
        cli_bin="$PROJECT_ROOT/build/Release/ccap"
    elif [ -x "$PROJECT_ROOT/build/Release/ccap.exe" ]; then
        cli_bin="$PROJECT_ROOT/build/Release/ccap.exe"
    else
        echo "ccap CLI not found. Building (Debug) with CCAP_BUILD_CLI=ON..."
        pushd "$PROJECT_ROOT" >/dev/null

        mkdir -p build/Debug
        pushd build/Debug >/dev/null

        # Reconfigure with CLI enabled (idempotent)
        cmake ../.. -DCMAKE_BUILD_TYPE=Debug -DCCAP_BUILD_CLI=ON
        cmake --build . --config Debug --target ccap-cli -- -j"$(nproc 2>/dev/null || echo 4)"

        popd >/dev/null
        popd >/dev/null

        cli_bin="$PROJECT_ROOT/build/Debug/ccap"
    fi

    cd "$original_dir"
    echo "$cli_bin"
}

parse_cli_device_count() {
    local output="$1"
    local count=0

    if [[ "$output" =~ Found[[:space:]]+([0-9]+)[[:space:]]+camera ]]; then
        count=${BASH_REMATCH[1]}
    elif echo "$output" | grep -qi "No camera devices found"; then
        count=0
    else
        count=-1
    fi

    echo "$count"
}

parse_rust_device_count() {
    local output="$1"
    local count=0

    if [[ "$output" =~ \#\#[[:space:]]+Found[[:space:]]+([0-9]+)[[:space:]]+video[[:space:]]+capture[[:space:]]+device ]]; then
        count=${BASH_REMATCH[1]}
    elif echo "$output" | grep -qi "Failed to find any video capture device"; then
        count=0
    else
        count=-1
    fi

    echo "$count"
}

echo "ccap Rust Bindings - Build and Test Script"
echo "==========================================="

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RUST_DIR="$SCRIPT_DIR"

echo "Project root: $PROJECT_ROOT"
echo "Rust bindings: $RUST_DIR"

# Build the C library first if needed
echo ""
echo "Step 1: Checking ccap C library..."
if [ ! -f "$PROJECT_ROOT/build/Debug/libccap.a" ] && [ ! -f "$PROJECT_ROOT/build/Release/libccap.a" ]; then
    echo "ccap C library not found. Building..."
    cd "$PROJECT_ROOT"

    mkdir -p build/Debug
    cd build/Debug

    if [ ! -f "CMakeCache.txt" ]; then
        cmake ../.. -DCMAKE_BUILD_TYPE=Debug
    fi

    cmake --build . --config Debug -- -j"$(nproc 2>/dev/null || echo 4)"

    cd "$RUST_DIR"
else
    echo "ccap C library found"
fi

# Build Rust bindings
echo ""
echo "Step 2: Building Rust bindings..."
cd "$RUST_DIR"

# Clean previous build
cargo clean

# Build with default features
echo "Building with default features..."
cargo build

# Build with async features
echo "Building with async features..."
cargo build --features async

# Run tests
echo ""
echo "Step 3: Running tests..."
cargo test

# Build examples
echo ""
echo "Step 4: Building examples..."
cargo build --examples
cargo build --features async --examples

echo ""
echo "Step 5: Testing basic functionality (camera discovery vs CLI)..."

# Run Rust discovery (print_camera) and capture output without aborting
set +e
RUST_DISCOVERY_OUTPUT=$(cargo run --example print_camera 2>&1)
RUST_DISCOVERY_STATUS=$?
set -e

RUST_DEVICE_COUNT=$(parse_rust_device_count "$RUST_DISCOVERY_OUTPUT")

CLI_BIN=$(detect_cli_devices)
if [ ! -x "$CLI_BIN" ]; then
    echo "❌ Failed to build or locate ccap CLI for reference checks." >&2
    exit 1
fi

set +e
CLI_DISCOVERY_OUTPUT=$("$CLI_BIN" --list-devices 2>&1)
CLI_DISCOVERY_STATUS=$?
set -e

CLI_DEVICE_COUNT=$(parse_cli_device_count "$CLI_DISCOVERY_OUTPUT")

echo "Rust discovery exit: $RUST_DISCOVERY_STATUS, devices: $RUST_DEVICE_COUNT"
echo "CLI  discovery exit: $CLI_DISCOVERY_STATUS, devices: $CLI_DEVICE_COUNT"

# Decision logic
if [ $CLI_DISCOVERY_STATUS -ne 0 ]; then
    echo "❌ CLI discovery failed. Output:" >&2
    echo "$CLI_DISCOVERY_OUTPUT" >&2
    exit 1
fi

if [ $CLI_DEVICE_COUNT -lt 0 ]; then
    echo "❌ Unable to parse CLI device count. Output:" >&2
    echo "$CLI_DISCOVERY_OUTPUT" >&2
    exit 1
fi

if [ $RUST_DISCOVERY_STATUS -ne 0 ] && [ $CLI_DEVICE_COUNT -gt 0 ]; then
    echo "❌ Rust discovery failed while CLI sees devices. Output:" >&2
    echo "$RUST_DISCOVERY_OUTPUT" >&2
    exit 1
fi

if [ $RUST_DEVICE_COUNT -lt 0 ]; then
    echo "⚠️  Rust discovery output could not be parsed. Output:" >&2
    echo "$RUST_DISCOVERY_OUTPUT" >&2
    if [ $CLI_DEVICE_COUNT -gt 0 ]; then
        echo "❌ CLI sees devices but Rust discovery is inconclusive." >&2
        exit 1
    fi
fi

if [ $CLI_DEVICE_COUNT -eq 0 ] && [ $RUST_DEVICE_COUNT -eq 0 ]; then
    echo "ℹ️  No cameras detected by CLI or Rust. Skipping capture tests (expected in headless environments)."
elif [ $CLI_DEVICE_COUNT -ne $RUST_DEVICE_COUNT ]; then
    echo "❌ Device count mismatch (Rust: $RUST_DEVICE_COUNT, CLI: $CLI_DEVICE_COUNT)." >&2
    echo "Rust output:" >&2
    echo "$RUST_DISCOVERY_OUTPUT" >&2
    echo "CLI output:" >&2
    echo "$CLI_DISCOVERY_OUTPUT" >&2
    exit 1
else
    echo "✅ Camera discovery consistent (devices: $CLI_DEVICE_COUNT)."
fi

echo ""
echo "✅ All Rust binding builds completed successfully!"
echo ""
echo "Usage examples:"
echo "  cargo run --example print_camera"
echo "  cargo run --example minimal_example"
echo "  cargo run --example capture_grab"
echo "  cargo run --example capture_callback"
echo "  cargo run --features async --example capture_callback"
echo ""
echo "To use in your project, add to Cargo.toml:"
echo '  ccap = { package = "ccap-rs", path = "'$RUST_DIR'" }'
echo '  # or with async support:'
echo '  ccap = { package = "ccap-rs", path = "'$RUST_DIR'", features = ["async"] }'
