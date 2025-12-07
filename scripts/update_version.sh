#!/bin/bash

# Check if version argument is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <new_version>"
    echo "Example: $0 1.2.0"
    exit 1
fi

NEW_VERSION=$1

# Extract major, minor, patch from version string
IFS='.' read -r -a VERSION_PARTS <<< "$NEW_VERSION"
MAJOR=${VERSION_PARTS[0]}
MINOR=${VERSION_PARTS[1]}
PATCH=${VERSION_PARTS[2]}

if [ -z "$MAJOR" ] || [ -z "$MINOR" ] || [ -z "$PATCH" ]; then
    echo "Error: Invalid version format. Expected X.Y.Z"
    exit 1
fi

echo "Updating version to $NEW_VERSION (Major: $MAJOR, Minor: $MINOR, Patch: $PATCH)..."

# Get the project root directory (assuming script is in scripts/ folder)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 1. Update include/ccap_config.h
sed -i "" "s/#define CCAP_VERSION_MAJOR .*/#define CCAP_VERSION_MAJOR $MAJOR/" "$PROJECT_ROOT/include/ccap_config.h"
sed -i "" "s/#define CCAP_VERSION_MINOR .*/#define CCAP_VERSION_MINOR $MINOR/" "$PROJECT_ROOT/include/ccap_config.h"
sed -i "" "s/#define CCAP_VERSION_PATCH .*/#define CCAP_VERSION_PATCH $PATCH/" "$PROJECT_ROOT/include/ccap_config.h"
sed -i "" "s/#define CCAP_VERSION_STRING .*/#define CCAP_VERSION_STRING \"$NEW_VERSION\"/" "$PROJECT_ROOT/include/ccap_config.h"
echo "Updated include/ccap_config.h"

# 2. Update ccap.podspec
sed -i "" "s/s.version      = \".*\"/s.version      = \"$NEW_VERSION\"/" "$PROJECT_ROOT/ccap.podspec"
echo "Updated ccap.podspec"

# 3. Update conanfile.py
sed -i "" "s/version = \".*\"/version = \"$NEW_VERSION\"/" "$PROJECT_ROOT/conanfile.py"
echo "Updated conanfile.py"

# 4. Update BUILD_AND_INSTALL.md
sed -i "" "s/Current version: .*/Current version: $NEW_VERSION/" "$PROJECT_ROOT/BUILD_AND_INSTALL.md"
echo "Updated BUILD_AND_INSTALL.md"

echo "Version update complete!"
