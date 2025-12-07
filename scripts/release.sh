#!/bin/bash

# Release script for CameraCapture
# This script validates version consistency, checks git history, and creates release tags
# Usage: ./release.sh [-n|--dry-run]

set -e

# Color definitions for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
DRY_RUN=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--dry-run)
            DRY_RUN=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-n|--dry-run]"
            exit 1
            ;;
    esac
done

# Get the project root directory (assuming script is in scripts/ folder)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
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

echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}          CameraCapture Release Script${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  Using remote: ${GREEN}$GITHUB_REMOTE${NC} ($(git remote get-url "$GITHUB_REMOTE"))"
echo ""

if [ "$DRY_RUN" = true ]; then
    echo -e "${YELLOW}⚠️  DRY-RUN MODE ENABLED (no actual changes will be made)${NC}"
    echo ""
fi

# ============================================================================
# Step 1: Extract and validate version from all sources
# ============================================================================
echo -e "${BLUE}Step 1: Extracting version from all sources...${NC}"
echo ""

# Extract from ccap_config.h
if [ ! -f "include/ccap_config.h" ]; then
    echo -e "${RED}❌ Error: include/ccap_config.h not found${NC}"
    exit 1
fi

HEADER_MAJOR=$(grep "#define CCAP_VERSION_MAJOR" include/ccap_config.h | awk '{print $3}')
HEADER_MINOR=$(grep "#define CCAP_VERSION_MINOR" include/ccap_config.h | awk '{print $3}')
HEADER_PATCH=$(grep "#define CCAP_VERSION_PATCH" include/ccap_config.h | awk '{print $3}')
HEADER_VERSION_STRING=$(grep "#define CCAP_VERSION_STRING" include/ccap_config.h | sed 's/.*"\([^"]*\)".*/\1/')

if [ -z "$HEADER_MAJOR" ] || [ -z "$HEADER_MINOR" ] || [ -z "$HEADER_PATCH" ]; then
    echo -e "${RED}❌ Error: Could not parse version macros from include/ccap_config.h${NC}"
    exit 1
fi

HEADER_VERSION="${HEADER_MAJOR}.${HEADER_MINOR}.${HEADER_PATCH}"
echo -e "  Header (ccap_config.h): ${GREEN}$HEADER_VERSION${NC}"

# Validate header version consistency
if [ "$HEADER_VERSION" != "$HEADER_VERSION_STRING" ]; then
    echo -e "${RED}❌ Error: Version mismatch in header file!${NC}"
    echo -e "  Calculated: $HEADER_VERSION, String: $HEADER_VERSION_STRING"
    exit 1
fi

# Extract from ccap.podspec
if [ ! -f "ccap.podspec" ]; then
    echo -e "${RED}❌ Error: ccap.podspec not found${NC}"
    exit 1
fi

PODSPEC_VERSION=$(grep '^  s.version' ccap.podspec | sed 's/.*"\([^"]*\)".*/\1/')
if [ -z "$PODSPEC_VERSION" ]; then
    echo -e "${RED}❌ Error: Could not extract version from ccap.podspec${NC}"
    exit 1
fi
echo -e "  Podspec (ccap.podspec): ${GREEN}$PODSPEC_VERSION${NC}"

# Extract from conanfile.py (optional file)
if [ -f "conanfile.py" ]; then
    CONAN_VERSION=$(grep 'version = ' conanfile.py | head -1 | sed 's/.*"\([^"]*\)".*/\1/')
    if [ -z "$CONAN_VERSION" ]; then
        echo -e "${RED}❌ Error: Could not extract version from conanfile.py${NC}"
        exit 1
    fi
    echo -e "  Conan (conanfile.py): ${GREEN}$CONAN_VERSION${NC}"
else
    echo -e "  Conan (conanfile.py): ${YELLOW}(not found, skipping)${NC}"
    CONAN_VERSION=""
fi

# Extract from BUILD_AND_INSTALL.md
if [ ! -f "BUILD_AND_INSTALL.md" ]; then
    echo -e "${RED}❌ Error: BUILD_AND_INSTALL.md not found${NC}"
    exit 1
fi

DOC_VERSION=$(grep 'Current version:' BUILD_AND_INSTALL.md | head -1 | sed 's/.*: \([^ ]*\).*/\1/')
if [ -z "$DOC_VERSION" ]; then
    echo -e "${RED}❌ Error: Could not extract version from BUILD_AND_INSTALL.md${NC}"
    exit 1
fi
echo -e "  Documentation (BUILD_AND_INSTALL.md): ${GREEN}$DOC_VERSION${NC}"

echo ""

# ============================================================================
# Step 2: Validate version consistency across all files
# ============================================================================
echo -e "${BLUE}Step 2: Validating version consistency...${NC}"
echo ""

VERSIONS=("$HEADER_VERSION" "$PODSPEC_VERSION" "$DOC_VERSION")
# Only add conanfile version if it exists
if [ -n "$CONAN_VERSION" ]; then
    VERSIONS+=("$CONAN_VERSION")
fi

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

# ============================================================================
# Step 3: Check git repository status
# ============================================================================
echo -e "${BLUE}Step 3: Checking git repository status...${NC}"
echo ""

# Check if we are in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
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

echo -e "${GREEN}✅ Git repository is clean${NC}"
echo ""

# ============================================================================
# Step 4: Get all existing version tags from git history
# ============================================================================
echo -e "${BLUE}Step 4: Checking existing version tags...${NC}"
echo ""

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
# Step 5: Check if current version already exists
# ============================================================================
echo -e "${BLUE}Step 5: Checking if version already exists in git history...${NC}"
echo ""

RELEASE_TAG="v$CURRENT_VERSION"

if git rev-parse "$RELEASE_TAG" >/dev/null 2>&1; then
    echo -e "${RED}❌ Error: Release tag already exists!${NC}"
    echo -e "  Tag: $RELEASE_TAG"
    echo -e "  Please update the version number in include/ccap_config.h"
    exit 1
fi

echo -e "${GREEN}✅ Version $CURRENT_VERSION does not exist in git history${NC}"
echo ""

# ============================================================================
# Step 6: Compare current version with latest existing version
# ============================================================================
echo -e "${BLUE}Step 6: Comparing version with latest existing version...${NC}"
echo ""

# Compare versions using semantic versioning rules
compare_versions() {
    local v1=$1
    local v2=$2
    
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
    
    if [ "$COMPARE" = "equal" ]; then
        echo -e "${RED}❌ Error: Version is not higher than the latest release!${NC}"
        echo -e "  Current: $CURRENT_VERSION, Latest: $LATEST_TAG"
        exit 1
    elif [ "$COMPARE" = "less" ]; then
        echo -e "${RED}❌ Error: Version must be higher than existing releases!${NC}"
        echo -e "  Current: $CURRENT_VERSION, Latest: $LATEST_TAG"
        exit 1
    fi
    
    echo -e "  Latest version: $LATEST_TAG"
    echo -e "  Current version: $CURRENT_VERSION"
    echo -e "${GREEN}✅ Version is higher than the latest release${NC}"
else
    echo -e "${GREEN}✅ This is the first release (no previous versions found)${NC}"
fi

echo ""

# ============================================================================
# Step 7: Create and push release tag
# ============================================================================
echo -e "${BLUE}Step 7: Creating and pushing release tag...${NC}"
echo ""

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
if ! git tag -a "$RELEASE_TAG" -m "Release $CURRENT_VERSION"; then
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

echo -e "${GREEN}✅ Tag pushed to remote${NC}"
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
echo -e "  1. GitHub Actions will automatically build and create a release"
echo -e "  2. Monitor the workflow at: https://github.com/wysaid/CameraCapture/actions"
echo -e "  3. Verify the release at: https://github.com/wysaid/CameraCapture/releases/tag/$RELEASE_TAG"
echo ""
