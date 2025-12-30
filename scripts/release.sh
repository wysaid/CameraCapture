#!/bin/bash

# Release script for CameraCapture
# This script validates version consistency, checks git history, and creates release tags
# Usage: ./release.sh [-n|--dry-run] [-f|--force] [-d|--delete] [--beta|--alpha|--rc [number]]

set -e

# Color definitions for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ============================================================================
# Helper Functions
# ============================================================================

# Print a step header with consistent formatting
print_step() {
    local step_number="$1"
    local step_title="$2"
    echo -e "${BLUE}Step ${step_number}: ${step_title}...${NC}"
    echo ""
}

# Print a section header with consistent formatting
print_section() {
    local title="$1"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}          ${title}${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
    echo ""
}

# Extract version from ccap_config.h
extract_header_version() {
    if [ ! -f "include/ccap_config.h" ]; then
        echo -e "${RED}❌ Error: include/ccap_config.h not found${NC}"
        exit 1
    fi

    local major=$(grep "#define CCAP_VERSION_MAJOR" include/ccap_config.h | awk '{print $3}')
    local minor=$(grep "#define CCAP_VERSION_MINOR" include/ccap_config.h | awk '{print $3}')
    local patch=$(grep "#define CCAP_VERSION_PATCH" include/ccap_config.h | awk '{print $3}')
    local version_string=$(grep "#define CCAP_VERSION_STRING" include/ccap_config.h | sed 's/.*"\([^"]*\)".*/\1/')

    if [ -z "$major" ] || [ -z "$minor" ] || [ -z "$patch" ]; then
        echo -e "${RED}❌ Error: Could not parse version macros from include/ccap_config.h${NC}"
        exit 1
    fi

    local calculated_version="${major}.${minor}.${patch}"

    # Validate version consistency
    if [ "$calculated_version" != "$version_string" ]; then
        echo -e "${RED}❌ Error: Version mismatch in header file!${NC}"
        echo -e "  Calculated: $calculated_version, String: $version_string"
        exit 1
    fi

    echo "$calculated_version"
}

# Extract version from a file with simple pattern matching
extract_version_from_file() {
    local file="$1"
    local pattern="$2"
    local name="$3"

    if [ ! -f "$file" ]; then
        if [ "$4" = "optional" ]; then
            return 1
        fi
        echo -e "${RED}❌ Error: $file not found${NC}"
        exit 1
    fi

    local version
    if [ "$pattern" = "version =" ]; then
        version=$(grep "$pattern" "$file" | head -1 | sed 's/.*"\([^"]*\)".*/\1/')
    elif [ "$pattern" = "Current version:" ]; then
        version=$(grep "$pattern" "$file" | head -1 | sed 's/.*: \([^ ]*\).*/\1/')
    else
        version=$(grep "^  s.version" "$file" | sed 's/.*"\([^"]*\)".*/\1/')
    fi

    if [ -z "$version" ]; then
        echo -e "${RED}❌ Error: Could not extract version from $file${NC}"
        exit 1
    fi

    echo "$version"
}

# Check if a git tag exists locally
check_tag_exists_locally() {
    local tag="$1"
    git rev-parse "$tag" >/dev/null 2>&1
}

# Check if a git tag exists on remote
check_tag_exists_remotely() {
    local tag="$1"
    local remote="$2"
    git ls-remote --tags "$remote" | grep -q "refs/tags/${tag}$"
}

# Validate that repository has no uncommitted changes
validate_clean_working_directory() {
    if ! git diff-index --quiet HEAD --; then
        echo -e "${RED}❌ Error: Uncommitted changes detected!${NC}"
        echo -e "  Official releases require all changes to be committed.${NC}"
        echo -e "  The -f/--force flag does NOT override this requirement.${NC}"
        echo ""
        echo -e "${YELLOW}Uncommitted changes:${NC}"
        git status --short | sed 's/^/    /'
        exit 1
    fi
    echo -e "${GREEN}✅ All changes committed${NC}"
}

# Validate that local branch is up-to-date with remote
validate_branch_synced_with_remote() {
    local remote="$1"
    local branch="$2"

    echo ""
    echo -e "  Checking if local branch is up-to-date with remote..."
    git fetch "$remote" --quiet 2>/dev/null || true

    local local_head=$(git rev-parse HEAD)
    local remote_head=$(git rev-parse "$remote/$branch" 2>/dev/null || echo "")

    if [ -z "$remote_head" ]; then
        echo -e "${RED}❌ Error: Remote tracking branch not found!${NC}"
        echo -e "  Remote: ${RED}$remote/$branch${NC}"
        echo -e "  Official releases require changes to be pushed to remote.${NC}"
        echo -e "  The -f/--force flag does NOT override this requirement.${NC}"
        echo ""
        echo -e "${YELLOW}Push your changes to remote:${NC}"
        echo -e "    ${GREEN}git push $remote $branch${NC}"
        exit 1
    fi

    if [ "$local_head" != "$remote_head" ]; then
        echo -e "${RED}❌ Error: Local branch is not up-to-date with remote!${NC}"
        echo -e "  Official releases require all commits to be pushed to remote.${NC}"
        echo -e "  The -f/--force flag does NOT override this requirement.${NC}"
        echo ""
        echo -e "${YELLOW}Push your changes to remote:${NC}"
        echo -e "    ${GREEN}git push $remote $branch${NC}"
        exit 1
    fi
    echo -e "${GREEN}✅ Branch is up-to-date with remote${NC}"
}

# Perform strict validation for official releases
# This enforces main branch requirement, clean working directory, and remote sync
# These checks apply EVEN with the -f/--force flag
validate_official_release_requirements() {
    local is_main_branch="$1"
    local current_branch="$2"
    local github_remote="$3"

    echo ""
    echo -e "${BLUE}Performing strict validation for official release...${NC}"
    echo ""

    # Check if on main branch
    if [ "$is_main_branch" = false ]; then
        echo -e "${RED}❌ Error: Official releases can ONLY be made from the main branch!${NC}"
        echo -e "  Current branch: ${RED}$current_branch${NC}"
        echo -e "  Required branch: ${GREEN}main${NC} or ${GREEN}master${NC}"
        echo ""
        echo -e "${YELLOW}The -f/--force flag does NOT override this requirement.${NC}"
        echo -e "${YELLOW}Official releases must be made from the main branch for stability.${NC}"
        echo ""
        echo -e "  Switch to main branch:"
        echo -e "    ${GREEN}git checkout main${NC}"
        exit 1
    fi
    echo -e "${GREEN}✅ On main branch${NC}"

    # Check for uncommitted changes
    validate_clean_working_directory

    # Check if local branch is up-to-date with remote
    validate_branch_synced_with_remote "$github_remote" "$current_branch"

    echo ""
}

# Parse command line arguments
DRY_RUN=false
FORCE_BRANCH=false
DELETE_MODE=false
AUTO_YES=false
PRERELEASE_TYPE=""
PRERELEASE_NUMBER=""

while [[ $# -gt 0 ]]; do
    case $1 in
    -n | --dry-run)
        DRY_RUN=true
        shift
        ;;
    -f | --force)
        FORCE_BRANCH=true
        shift
        ;;
    -d | --delete)
        DELETE_MODE=true
        shift
        ;;
    -y | --yes)
        AUTO_YES=true
        shift
        ;;
    --beta | --alpha | --rc)
        PRERELEASE_TYPE="${1#--}"
        shift
        # Check if next argument is a number
        if [[ $# -gt 0 && "$1" =~ ^[0-9]+$ ]]; then
            PRERELEASE_NUMBER="$1"
            shift
        else
            PRERELEASE_NUMBER="1"
        fi
        ;;
    *)
        echo "Unknown option: $1"
        echo "Usage: $0 [-n|--dry-run] [-f|--force] [-d|--delete] [-y|--yes] [--beta|--alpha|--rc [number]]"
        echo ""
        echo "Options:"
        echo "  -n, --dry-run         Run without making actual changes"
        echo "  -f, --force           Force tag creation on any branch (requires all changes committed)"
        echo "  -d, --delete          Delete existing tag(s) matching current version"
        echo "  -y, --yes             Auto-confirm with default options (official release)"
        echo "  --beta [number]       Create beta release (e.g., v1.0.0-beta.1)"
        echo "  --alpha [number]      Create alpha release (e.g., v1.0.0-alpha.1)"
        echo "  --rc [number]         Create release candidate (e.g., v1.0.0-rc.1)"
        echo ""
        echo "Examples:"
        echo "  $0                    # Create official release (interactive)"
        echo "  $0 -y                 # Create official release (auto-confirm)"
        echo "  $0 --dry-run          # Preview release without changes"
        echo "  $0 -f                 # Force release on current branch"
        echo "  $0 -d                 # Delete tag(s) matching current version"
        echo "  $0 --beta             # Create beta.1 release"
        echo "  $0 --beta 2           # Create beta.2 release"
        echo "  $0 --alpha 3          # Create alpha.3 release"
        echo "  $0 --rc 1             # Create rc.1 release"
        exit 1
        ;;
    esac
done

# Get the project root directory (assuming script is in scripts/ folder)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to project root
cd "$PROJECT_ROOT"

# Helper function to get the appropriate GitHub remote
# Priority: URL containing github.com > first available remote
get_github_remote() {
    # First, try to find a remote with github.com in the URL
    local github_remote=$(git remote -v | grep 'github\.com' | awk '{print $1}' | head -1)

    if [ -n "$github_remote" ]; then
        echo "$github_remote"
        return 0
    fi

    # Fallback: return first available remote
    local first_remote=$(git remote | head -1)
    if [ -n "$first_remote" ]; then
        echo "$first_remote"
        return 0
    fi

    # No remotes found
    return 1
}

# Get the GitHub remote to use for pushing
GITHUB_REMOTE=$(get_github_remote)
if [ -z "$GITHUB_REMOTE" ]; then
    echo -e "${RED}❌ Error: No git remote found!${NC}"
    exit 1
fi

# Helper function to check if a tag exists as a release on GitHub
check_github_release_exists() {
    local tag_name=$1
    local remote_url
    remote_url=$(git remote get-url "$GITHUB_REMOTE")

    # Extract owner and repo from GitHub URL
    # Support formats: git@github.com:owner/repo.git, https://github.com/owner/repo.git
    local owner_repo=""
    if [[ "$remote_url" =~ github\.com[:/]([^/]+)/([^/.]+) ]]; then
        local owner="${BASH_REMATCH[1]}"
        local repo="${BASH_REMATCH[2]}"
        owner_repo="${owner}/${repo}"
    else
        echo -e "${YELLOW}⚠️  Warning: Could not parse GitHub owner/repo from remote URL${NC}"
        return 2
    fi

    # Check if release exists using GitHub API
    local api_url="https://api.github.com/repos/${owner_repo}/releases/tags/${tag_name}"

    # Build curl command with optional authentication
    local curl_args=(-s --connect-timeout 5 --max-time 10 -o /dev/null -w "%{http_code}")
    if [ -n "${GITHUB_TOKEN:-}" ]; then
        curl_args+=(-H "Authorization: Bearer $GITHUB_TOKEN")
    fi

    local status_code
    status_code=$(curl "${curl_args[@]}" "$api_url")

    if [ "$status_code" = "200" ]; then
        return 0 # Release exists
    elif [ "$status_code" = "404" ]; then
        return 1 # Release does not exist
    elif [ "$status_code" = "403" ]; then
        echo -e "${YELLOW}⚠️  Warning: GitHub API rate limit exceeded (HTTP 403)${NC}"
        echo -e "${YELLOW}    Tip: Set GITHUB_TOKEN environment variable to increase rate limit${NC}"
        return 2 # Rate limit exceeded
    else
        echo -e "${YELLOW}⚠️  Warning: Could not check release status (HTTP $status_code)${NC}"
        return 2 # Unknown status
    fi
}

# ============================================================================
# DELETE MODE: Delete existing tags
# ============================================================================
if [ "$DELETE_MODE" = true ]; then
    print_section "Tag Deletion Mode"

    # Extract current version from header file
    BASE_VERSION=$(extract_header_version)

    echo -e "  Current base version: ${GREEN}$BASE_VERSION${NC}"
    echo ""

    # Fetch latest tags from remote
    echo -e "  Fetching tags from remote..."
    git fetch "$GITHUB_REMOTE" --tags --quiet 2>/dev/null || true

    # Find all tags matching current version (including pre-releases)
    MATCHING_TAGS=$(git tag -l "v${BASE_VERSION}*" | sort -V)

    if [ -z "$MATCHING_TAGS" ]; then
        echo -e "${YELLOW}⚠️  No tags found matching version $BASE_VERSION${NC}"
        exit 0
    fi

    echo -e "  Found matching tags:"
    echo ""

    # Display tags with numbering
    idx=1
    declare -A tag_map
    while IFS= read -r tag; do
        echo -e "    ${GREEN}[$idx]${NC} $tag"
        tag_map[$idx]=$tag
        idx=$((idx + 1))
    done <<<"$MATCHING_TAGS"

    echo ""
    echo -e "  ${YELLOW}Enter the number of the tag to delete (or 'q' to quit):${NC}"
    read -r selection

    if [ "$selection" = "q" ] || [ "$selection" = "Q" ]; then
        echo -e "${BLUE}Operation cancelled.${NC}"
        exit 0
    fi

    if ! [[ "$selection" =~ ^[0-9]+$ ]] || [ -z "${tag_map[$selection]}" ]; then
        echo -e "${RED}❌ Error: Invalid selection${NC}"
        exit 1
    fi

    TAG_TO_DELETE="${tag_map[$selection]}"

    echo ""
    echo -e "  Selected tag: ${RED}$TAG_TO_DELETE${NC}"
    echo ""

    # Check if tag exists as a release on GitHub
    echo -e "  Checking if tag has a GitHub Release..."
    if check_github_release_exists "$TAG_TO_DELETE"; then
        echo -e "${RED}❌ Error: This tag has an associated GitHub Release!${NC}"
        echo ""
        echo -e "${YELLOW}Please manually delete the release first:${NC}"
        echo -e "  1. Go to: https://github.com/wysaid/CameraCapture/releases"
        echo -e "  2. Find the release for ${RED}$TAG_TO_DELETE${NC}"
        echo -e "  3. Delete the release"
        echo -e "  4. Then run this script again to delete the tag"
        echo ""
        exit 1
    fi

    echo -e "${GREEN}✅ Tag does not have a GitHub Release (safe to delete)${NC}"
    echo ""

    # Confirm deletion
    echo -e "${YELLOW}Are you sure you want to delete tag ${RED}$TAG_TO_DELETE${NC}${YELLOW}? (yes/no):${NC}"
    read -r confirm

    if [ "$confirm" != "yes" ]; then
        echo -e "${BLUE}Operation cancelled.${NC}"
        exit 0
    fi

    echo ""
    echo -e "  Deleting tag locally..."
    if git tag -d "$TAG_TO_DELETE" 2>/dev/null; then
        echo -e "${GREEN}✅ Local tag deleted${NC}"
    else
        echo -e "${YELLOW}⚠️  Tag not found locally (may only exist on remote)${NC}"
    fi

    echo -e "  Deleting tag from remote..."
    if git push "$GITHUB_REMOTE" --delete "$TAG_TO_DELETE" 2>/dev/null; then
        echo -e "${GREEN}✅ Remote tag deleted${NC}"
    else
        echo -e "${YELLOW}⚠️  Could not delete remote tag (may not exist)${NC}"
    fi

    echo ""
    echo -e "${BLUE}Checking for local tags not present on remote...${NC}"

    # Fetch latest remote tags to ensure we have up-to-date information
    git fetch "$GITHUB_REMOTE" --tags --prune --quiet 2>/dev/null || true

    # Get all local tags
    LOCAL_TAGS=$(git tag)

    # Get all remote tags
    REMOTE_TAGS=$(git ls-remote --tags "$GITHUB_REMOTE" | awk '{print $2}' | sed 's|refs/tags/||' | sed 's|\^{}||' | sort -u)

    # Find tags that exist locally but not on remote
    ORPHAN_TAGS=""
    while IFS= read -r local_tag; do
        if [ -n "$local_tag" ]; then
            if ! echo "$REMOTE_TAGS" | grep -Fxq "$local_tag"; then
                ORPHAN_TAGS="${ORPHAN_TAGS}${local_tag}"$'\n'
            fi
        fi
    done <<<"$LOCAL_TAGS"

    # Remove trailing newline
    ORPHAN_TAGS=$(echo "$ORPHAN_TAGS" | sed '/^$/d')

    if [ -n "$ORPHAN_TAGS" ]; then
        echo -e "  ${YELLOW}Found local tags not present on remote:${NC}"
        echo ""
        echo "$ORPHAN_TAGS" | while IFS= read -r tag; do
            echo -e "    - ${YELLOW}$tag${NC}"
        done
        echo ""
        echo -e "  ${YELLOW}These tags could be accidentally pushed with 'git push --tags'.${NC}"
        echo -e "  ${YELLOW}Do you want to delete all these local-only tags? (yes/no):${NC}"
        read -r cleanup_confirm

        if [ "$cleanup_confirm" = "yes" ]; then
            echo ""
            echo -e "  Deleting local-only tags..."
            deleted_count=0
            failed_count=0

            while IFS= read -r tag; do
                if [ -n "$tag" ]; then
                    if git tag -d "$tag" 2>/dev/null; then
                        echo -e "    ${GREEN}✓${NC} Deleted: $tag"
                        deleted_count=$((deleted_count + 1))
                    else
                        echo -e "    ${RED}✗${NC} Failed to delete: $tag"
                        failed_count=$((failed_count + 1))
                    fi
                fi
            done <<<"$ORPHAN_TAGS"

            echo ""
            echo -e "${GREEN}✅ Cleanup completed: $deleted_count deleted, $failed_count failed${NC}"
        else
            echo -e "  ${BLUE}Skipped local-only tag cleanup${NC}"
        fi
    else
        echo -e "  ${GREEN}✅ No local-only tags found${NC}"
    fi

    echo ""
    echo -e "${GREEN}✅ Tag deletion completed!${NC}"
    echo ""
    exit 0
fi

# ============================================================================
# RELEASE MODE: Continue with normal release process
# ============================================================================

# Check current branch and auto-enable dry-run if not on main branch
CURRENT_BRANCH=$(git branch --show-current 2>/dev/null || git rev-parse --abbrev-ref HEAD)
MAIN_BRANCHES=("main" "master")
IS_MAIN_BRANCH=false

for branch in "${MAIN_BRANCHES[@]}"; do
    if [ "$CURRENT_BRANCH" = "$branch" ]; then
        IS_MAIN_BRANCH=true
        break
    fi
done

# Check if on main branch - REQUIRED for release unless -f is used
if [ "$IS_MAIN_BRANCH" = false ] && [ "$FORCE_BRANCH" = false ]; then
    echo -e "${RED}❌ Error: Release can only be executed from the main branch!${NC}"
    echo -e "  Current branch: ${RED}$CURRENT_BRANCH${NC}"
    echo -e "  Required branch: ${GREEN}main${NC} or ${GREEN}master${NC}"
    echo ""
    echo -e "${YELLOW}To force release on current branch, use:${NC}"
    echo -e "  ${GREEN}$0 -f${NC}"
    echo ""
    echo -e "${YELLOW}Or switch to the main branch:${NC}"
    echo -e "  ${GREEN}git checkout main${NC}"
    exit 1
fi

if [ "$IS_MAIN_BRANCH" = false ] && [ "$FORCE_BRANCH" = true ]; then
    echo -e "${YELLOW}⚠️  Warning: Force mode enabled - releasing from non-main branch!${NC}"
    echo -e "  Current branch: ${YELLOW}$CURRENT_BRANCH${NC}"
    echo ""
fi

print_section "CameraCapture Release Script"

echo -e "  Current branch: ${GREEN}$CURRENT_BRANCH${NC}"
echo -e "  Using remote: ${GREEN}$GITHUB_REMOTE${NC} ($(git remote get-url "$GITHUB_REMOTE"))"
if [ -n "$PRERELEASE_TYPE" ]; then
    echo -e "  Release type: ${YELLOW}Pre-release ($PRERELEASE_TYPE.$PRERELEASE_NUMBER)${NC}"
else
    echo -e "  Release type: ${GREEN}Official Release${NC}"
fi
echo ""

if [ "$DRY_RUN" = true ]; then
    echo -e "${YELLOW}⚠️  DRY-RUN MODE ENABLED (no actual changes will be made)${NC}"
    echo ""
fi

# ============================================================================
# Step 1: Extract and validate version from all sources
# ============================================================================
print_step "1" "Extracting version from all sources"

# Extract from ccap_config.h
HEADER_VERSION=$(extract_header_version)
echo -e "  Header (ccap_config.h): ${GREEN}$HEADER_VERSION${NC}"

# Extract from ccap.podspec
PODSPEC_VERSION=$(extract_version_from_file "ccap.podspec" "s.version" "podspec")
echo -e "  Podspec (ccap.podspec): ${GREEN}$PODSPEC_VERSION${NC}"

# Extract from conanfile.py (optional file)
if CONAN_VERSION=$(extract_version_from_file "conanfile.py" "version =" "conan" "optional"); then
    echo -e "  Conan (conanfile.py): ${GREEN}$CONAN_VERSION${NC}"
else
    echo -e "  Conan (conanfile.py): ${YELLOW}(not found, skipping)${NC}"
    CONAN_VERSION=""
fi

# Extract from BUILD_AND_INSTALL.md
DOC_VERSION=$(extract_version_from_file "BUILD_AND_INSTALL.md" "Current version:" "documentation")
echo -e "  Documentation (BUILD_AND_INSTALL.md): ${GREEN}$DOC_VERSION${NC}"

echo ""

# ============================================================================
# Step 2: Validate version consistency across all files
# ============================================================================
print_step "2" "Validating version consistency"

VERSIONS=("$HEADER_VERSION" "$PODSPEC_VERSION" "$DOC_VERSION")
# Only add conanfile version if it exists
if [ -n "$CONAN_VERSION" ]; then
    VERSIONS+=("$CONAN_VERSION")
fi

CONSISTENT=true
for version in "${VERSIONS[@]}"; do
    if [ "$version" != "$HEADER_VERSION" ]; then
        echo -e "${RED}❌ Version mismatch detected!${NC}"
        echo -e "  Expected: $HEADER_VERSION, Found: $version"
        CONSISTENT=false
    fi
done

if [ "$CONSISTENT" = false ]; then
    echo ""
    echo -e "${RED}❌ Version inconsistency detected across files!${NC}"
    echo -e "  Please run: ./scripts/update_version.sh $HEADER_VERSION"
    exit 1
fi

echo -e "${GREEN}✅ All versions are consistent: $HEADER_VERSION${NC}"
echo ""

CURRENT_VERSION="$HEADER_VERSION"

# Apply pre-release suffix if specified
if [ -n "$PRERELEASE_TYPE" ]; then
    CURRENT_VERSION="${CURRENT_VERSION}-${PRERELEASE_TYPE}.${PRERELEASE_NUMBER}"
    echo -e "${BLUE}Pre-release version: ${YELLOW}$CURRENT_VERSION${NC}"
    echo ""
fi

# ============================================================================
# Step 3: Check git repository status
# ============================================================================
print_step "3" "Checking git repository status"

# Check if we are in a git repository
if ! git rev-parse --git-dir >/dev/null 2>&1; then
    echo -e "${RED}❌ Error: Not in a git repository${NC}"
    exit 1
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${RED}❌ Error: Uncommitted changes detected!${NC}"
    echo -e "  Please commit all changes before releasing"
    git status --short
    exit 1
fi

# Check for untracked files (optional warning)
UNTRACKED_FILES=$(git ls-files --others --exclude-standard)
if [ -n "$UNTRACKED_FILES" ]; then
    echo -e "${YELLOW}⚠️  Warning: Untracked files detected:${NC}"
    echo "$UNTRACKED_FILES" | sed 's/^/    /'
    echo ""
fi

echo -e "${GREEN}✅ Git repository is clean (all changes committed)${NC}"
echo ""

# ============================================================================
# Step 4: Get all existing version tags from git history
# ============================================================================
print_step "4" "Checking existing version tags"

# Get all tags matching version pattern (v*.*.*, v*.*.*-alpha*, v*.*.*-beta*, v*.*.*-rc*)
VERSION_TAGS=$(git tag -l | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+' | sort -V)

if [ -z "$VERSION_TAGS" ]; then
    echo -e "${YELLOW}ℹ️  No existing version tags found${NC}"
    LATEST_TAG="0.0.0"
else
    echo -e "  Existing tags:"
    echo "$VERSION_TAGS" | sed 's/^/    - /'
    LATEST_TAG=$(echo "$VERSION_TAGS" | tail -1 | sed 's/^v//')
    echo ""
    echo -e "  Latest tag: ${GREEN}$LATEST_TAG${NC}"
fi

echo ""

# ============================================================================
# Step 5: Check if version already exists (both locally and remotely)
# ============================================================================
print_step "5" "Checking if version already exists"

RELEASE_TAG="v$CURRENT_VERSION"

# Check local tags
if check_tag_exists_locally "$RELEASE_TAG"; then
    echo -e "${RED}❌ Error: Release tag already exists locally!${NC}"
    echo -e "  Tag: $RELEASE_TAG"
    echo -e "  Please update the version number or use a different pre-release suffix"
    exit 1
fi

echo -e "${GREEN}✅ Tag does not exist locally${NC}"

# Fetch remote tags to ensure we have the latest
echo -e "  Fetching remote tags..."
if ! git fetch "$GITHUB_REMOTE" --tags --quiet 2>/dev/null; then
    echo -e "${YELLOW}⚠️  Warning: Could not fetch remote tags (continuing anyway)${NC}"
else
    echo -e "${GREEN}✅ Remote tags fetched${NC}"
fi

# Check remote tags
if check_tag_exists_remotely "$RELEASE_TAG" "$GITHUB_REMOTE"; then
    echo -e "${RED}❌ Error: Release tag already exists on remote!${NC}"
    echo -e "  Remote: $GITHUB_REMOTE"
    echo -e "  Tag: $RELEASE_TAG"
    echo -e "  Please update the version number or use a different pre-release suffix"
    exit 1
fi

echo -e "${GREEN}✅ Tag does not exist on remote${NC}"
echo ""

# ============================================================================
# Step 6: Compare current version with latest existing version
# ============================================================================
print_step "6" "Comparing version with latest existing version"

# Compare versions using semantic versioning rules
compare_versions() {
    local v1=$1
    local v2=$2

    # Strip any pre-release suffix for comparison (e.g., 1.2.3-beta.1 -> 1.2.3)
    v1=$(echo "$v1" | sed 's/-.*$//')
    v2=$(echo "$v2" | sed 's/-.*$//')

    # Convert versions to comparable format (e.g., 1.2.3 -> 001002003)
    local v1_parts=(${v1//./ })
    local v2_parts=(${v2//./ })

    local v1_num=$(printf "%03d%03d%03d" ${v1_parts[0]} ${v1_parts[1]} ${v1_parts[2]})
    local v2_num=$(printf "%03d%03d%03d" ${v2_parts[0]} ${v2_parts[1]} ${v2_parts[2]})

    if [ "$v1_num" -gt "$v2_num" ]; then
        echo "greater"
    elif [ "$v1_num" -eq "$v2_num" ]; then
        echo "equal"
    else
        echo "less"
    fi
}

if [ "$LATEST_TAG" != "0.0.0" ]; then
    COMPARE=$(compare_versions "$CURRENT_VERSION" "$LATEST_TAG")

    if [ "$COMPARE" = "less" ]; then
        echo -e "${RED}❌ Error: Version must be higher than or equal to existing releases!${NC}"
        echo -e "  Current: $CURRENT_VERSION, Latest: $LATEST_TAG"
        exit 1
    elif [ "$COMPARE" = "equal" ]; then
        # For equal base versions, check if it's a pre-release or if user will select one
        if [ -z "$PRERELEASE_TYPE" ]; then
            # If no prerelease type specified and not in auto mode, user can choose in Step 6.5
            if [ "$AUTO_YES" = true ]; then
                echo -e "${RED}❌ Error: Version is not higher than the latest release!${NC}"
                echo -e "  Current: $CURRENT_VERSION, Latest: $LATEST_TAG"
                echo -e "  Tip: Use -b/--beta, -a/--alpha, or -r/--rc to create a pre-release"
                exit 1
            else
                echo -e "${YELLOW}⚠️  Version equals latest release ($LATEST_TAG)${NC}"
                echo -e "  You can choose to create a pre-release in the next step"
            fi
        else
            echo -e "${GREEN}✅ Creating pre-release for version $HEADER_VERSION${NC}"
        fi
    else
        echo -e "  Latest version: $LATEST_TAG"
        echo -e "  Current version: $CURRENT_VERSION"
        echo -e "${GREEN}✅ Version is higher than the latest release${NC}"
    fi
else
    echo -e "${GREEN}✅ This is the first release (no previous versions found)${NC}"
fi

echo ""

# ============================================================================
# Step 6.5: Interactive release type selection (if not specified)
# ============================================================================
if [ -z "$PRERELEASE_TYPE" ] && [ "$AUTO_YES" = false ]; then
    echo -e "${BLUE}Step 6.5: Select release type...${NC}"
    echo ""
    echo -e "  ${YELLOW}What type of release do you want to create?${NC}"
    echo ""
    echo -e "    ${GREEN}[1]${NC} Official Release (v${HEADER_VERSION})"
    echo -e "    ${YELLOW}[2]${NC} Beta Release (v${HEADER_VERSION}-beta.N)"
    echo -e "    ${YELLOW}[3]${NC} Alpha Release (v${HEADER_VERSION}-alpha.N)"
    echo -e "    ${YELLOW}[4]${NC} Release Candidate (v${HEADER_VERSION}-rc.N)"
    echo ""
    echo -e "  ${BLUE}Enter your choice [1-4] (default: 1):${NC} "
    read -r release_choice

    # Default to 1 if empty
    if [ -z "$release_choice" ]; then
        release_choice=1
    fi

    case $release_choice in
    1)
        echo -e "  ${GREEN}✓ Selected: Official Release${NC}"
        # PRERELEASE_TYPE remains empty

        # For official releases, enforce strict requirements even with -f flag
        validate_official_release_requirements "$IS_MAIN_BRANCH" "$CURRENT_BRANCH" "$GITHUB_REMOTE"
        ;;
    2 | 3 | 4)
        if [ "$release_choice" = "2" ]; then
            PRERELEASE_TYPE="beta"
        elif [ "$release_choice" = "3" ]; then
            PRERELEASE_TYPE="alpha"
        else
            PRERELEASE_TYPE="rc"
        fi

        echo ""
        echo -e "  ${YELLOW}Enter the $PRERELEASE_TYPE number (default: 1):${NC} "
        read -r prerelease_num

        if [ -z "$prerelease_num" ]; then
            PRERELEASE_NUMBER=1
        elif [[ "$prerelease_num" =~ ^[0-9]+$ ]]; then
            PRERELEASE_NUMBER=$prerelease_num
        else
            echo -e "${RED}❌ Error: Invalid number${NC}"
            exit 1
        fi

        CURRENT_VERSION="${HEADER_VERSION}-${PRERELEASE_TYPE}.${PRERELEASE_NUMBER}"
        RELEASE_TAG="v$CURRENT_VERSION"

        echo -e "  ${GREEN}✓ Selected: $PRERELEASE_TYPE Release (v${CURRENT_VERSION})${NC}"

        # Re-check if this tag already exists
        if check_tag_exists_locally "$RELEASE_TAG"; then
            echo ""
            echo -e "${RED}❌ Error: This tag already exists locally!${NC}"
            echo -e "  Tag: $RELEASE_TAG"
            echo -e "  Please choose a different number or update the version"
            exit 1
        fi

        if check_tag_exists_remotely "$RELEASE_TAG" "$GITHUB_REMOTE"; then
            echo ""
            echo -e "${RED}❌ Error: This tag already exists on remote!${NC}"
            echo -e "  Tag: $RELEASE_TAG"
            echo -e "  Please choose a different number or update the version"
            exit 1
        fi
        ;;
    *)
        echo -e "${RED}❌ Error: Invalid choice${NC}"
        exit 1
        ;;
    esac

    echo ""
elif [ "$AUTO_YES" = true ] && [ -z "$PRERELEASE_TYPE" ]; then
    echo -e "${BLUE}Auto-confirm mode: Creating official release${NC}"

    # For official releases in auto mode, enforce strict requirements even with -f flag
    validate_official_release_requirements "$IS_MAIN_BRANCH" "$CURRENT_BRANCH" "$GITHUB_REMOTE"
fi

# ============================================================================
# Step 7: Create and push release tag
# ============================================================================
print_step "7" "Creating and pushing release tag"

echo -e "  Release tag: ${GREEN}$RELEASE_TAG${NC}"
echo -e "  Release version: ${GREEN}$CURRENT_VERSION${NC}"

if [ "$DRY_RUN" = true ]; then
    echo ""
    echo -e "${YELLOW}DRY-RUN: Skipping actual tag creation and push${NC}"
    echo ""
    echo -e "${GREEN}✅ Dry-run completed successfully!${NC}"
    echo ""
    echo -e "${BLUE}What would be executed:${NC}"
    echo -e "  ${BLUE}git tag -a $RELEASE_TAG -m \"Release $CURRENT_VERSION\"${NC}"
    echo -e "  ${BLUE}git push $GITHUB_REMOTE $RELEASE_TAG${NC}"
    exit 0
fi

# Create annotated tag
TAG_MESSAGE="Release $CURRENT_VERSION"
if [ -n "$PRERELEASE_TYPE" ]; then
    TAG_MESSAGE="Pre-release $CURRENT_VERSION"
fi

if ! git tag -a "$RELEASE_TAG" -m "$TAG_MESSAGE"; then
    echo -e "${RED}❌ Error: Failed to create tag $RELEASE_TAG${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Tag created: $RELEASE_TAG${NC}"

# Push tag to remote
if ! git push "$GITHUB_REMOTE" "$RELEASE_TAG"; then
    echo -e "${RED}❌ Error: Failed to push tag to remote${NC}"
    echo -e "  Cleaning up local tag..."
    git tag -d "$RELEASE_TAG"
    exit 1
fi

echo -e "${GREEN}✅ Tag pushed to remote: $GITHUB_REMOTE${NC}"
echo ""

# ============================================================================
# Success Summary
# ============================================================================
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✅ Release completed successfully!${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  Release version: ${GREEN}$CURRENT_VERSION${NC}"
echo -e "  Release tag: ${GREEN}$RELEASE_TAG${NC}"
echo ""
echo -e "${BLUE}Next steps:${NC}"
if [ "$DRY_RUN" = true ]; then
    echo -e "  ${YELLOW}This was a DRY-RUN. No actual changes were made.${NC}"
    echo ""
    echo -e "${BLUE}To perform an actual release:${NC}"
    if [ -n "$PRERELEASE_TYPE" ]; then
        echo -e "  ${GREEN}./scripts/release.sh --$PRERELEASE_TYPE $PRERELEASE_NUMBER${NC} (without --dry-run)"
    else
        echo -e "  ${GREEN}./scripts/release.sh${NC} (without --dry-run)"
    fi
else
    echo -e "  1. GitHub Actions will automatically build and create a release"
    echo -e "  2. Monitor the workflow at: https://github.com/wysaid/CameraCapture/actions"
    echo -e "  3. Verify the release at: https://github.com/wysaid/CameraCapture/releases/tag/$RELEASE_TAG"
    if [ -n "$PRERELEASE_TYPE" ]; then
        echo ""
        echo -e "${YELLOW}Note: This is a pre-release and will be marked as such on GitHub${NC}"
    fi
fi
echo ""
