#!/bin/bash

# Exit on any error to ensure script robustness
set -e

# Check if version argument is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <new_version>"
    echo "Example: $0 1.2.0"
    exit 1
fi

NEW_VERSION=$1

# Extract major, minor, patch from version string
IFS='.' read -r -a VERSION_PARTS <<<"$NEW_VERSION"
MAJOR=${VERSION_PARTS[0]}
MINOR=${VERSION_PARTS[1]}
PATCH=${VERSION_PARTS[2]}

# Validate that all components are present and numeric
if [ -z "$MAJOR" ] || [ -z "$MINOR" ] || [ -z "$PATCH" ]; then
    echo "Error: Invalid version format. Expected X.Y.Z"
    exit 1
fi

if ! [[ "$MAJOR" =~ ^[0-9]+$ ]] || ! [[ "$MINOR" =~ ^[0-9]+$ ]] || ! [[ "$PATCH" =~ ^[0-9]+$ ]]; then
    echo "Error: Version components must be numeric (got: Major=$MAJOR, Minor=$MINOR, Patch=$PATCH)"
    exit 1
fi

echo "Updating version to $NEW_VERSION (Major: $MAJOR, Minor: $MINOR, Patch: $PATCH)..."

# Get the project root directory (assuming script is in scripts/ folder)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Cross-platform sed in-place editing function
# Handles differences between GNU sed (Linux) and BSD sed (macOS)
sed_inplace() {
    local expr=$1
    local file=$2
    if sed --version >/dev/null 2>&1; then
        # GNU sed (Linux)
        sed -i "$expr" "$file"
    else
        # BSD/macOS sed
        sed -i "" "$expr" "$file"
    fi
}

# Function to update file with error checking
update_file() {
    local expr=$1
    local file=$2
    local description=$3

    if [ ! -f "$file" ]; then
        echo "⚠️  $file not found, skipping $description"
        return 0
    fi

    if sed_inplace "$expr" "$file"; then
        echo "✅ Updated $description"
    else
        echo "❌ Failed to update $description"
        exit 1
    fi
}

echo "Updating version to $NEW_VERSION (Major: $MAJOR, Minor: $MINOR, Patch: $PATCH)..."

# 1. Update include/ccap_config.h (critical file - must exist)
CONFIG_FILE="$PROJECT_ROOT/include/ccap_config.h"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "❌ Critical error: $CONFIG_FILE not found!"
    exit 1
fi

update_file "s/#define CCAP_VERSION_MAJOR .*/#define CCAP_VERSION_MAJOR $MAJOR/" "$CONFIG_FILE" "CCAP_VERSION_MAJOR"
update_file "s/#define CCAP_VERSION_MINOR .*/#define CCAP_VERSION_MINOR $MINOR/" "$CONFIG_FILE" "CCAP_VERSION_MINOR"
update_file "s/#define CCAP_VERSION_PATCH .*/#define CCAP_VERSION_PATCH $PATCH/" "$CONFIG_FILE" "CCAP_VERSION_PATCH"
update_file "s/#define CCAP_VERSION_STRING .*/#define CCAP_VERSION_STRING \"$NEW_VERSION\"/" "$CONFIG_FILE" "CCAP_VERSION_STRING"

# 2. Update ccap.podspec (iOS/macOS support)
update_file "s/s.version      = \".*\"/s.version      = \"$NEW_VERSION\"/" "$PROJECT_ROOT/ccap.podspec" "ccap.podspec"

# 3. Update conanfile.py (optional Conan support)
update_file "s/version = \".*\"/version = \"$NEW_VERSION\"/" "$PROJECT_ROOT/conanfile.py" "conanfile.py (Conan support)"

# 4. Update BUILD_AND_INSTALL.md (documentation)
update_file "s/Current version: .*/Current version: $NEW_VERSION/" "$PROJECT_ROOT/BUILD_AND_INSTALL.md" "BUILD_AND_INSTALL.md"

echo "Version update complete!"
