#!/bin/bash

# Build and install script for ccap
# Usage: ./scripts/build_and_install.sh [Debug|Release] [install_dir]

set -e

function isWsl() {
    [[ -d "/mnt/c" ]] || command -v wslpath &>/dev/null
}

function isWindows() {
    [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]] || isWsl || [[ -n "$WINDIR" ]]
}

function detectCores() {
    if [[ -n "$NUMBER_OF_PROCESSORS" ]]; then
        echo $NUMBER_OF_PROCESSORS
    else
        getconf _NPROCESSORS_ONLN || nproc || echo 1
    fi
}

if isWsl; then
    # Check if this is a Windows mount point or native WSL Linux
    if [[ "$(pwd)" == /mnt/* ]]; then
        # Windows mount point - switch to Git Bash for Windows builds
        echo "You're using WSL with Windows mount point. Switching to Git Bash for Windows build!" >&2
        GIT_BASH_PATH_WIN=$(/mnt/c/Windows/system32/cmd.exe /C "where bash.exe" | grep -i Git | head -n 1 | tr -d '\n\r')
        GIT_BASH_PATH_WSL=$(wslpath -u "$GIT_BASH_PATH_WIN")
        echo "== GIT_BASH_PATH_WIN=$GIT_BASH_PATH_WIN"
        echo "== GIT_BASH_PATH_WSL=$GIT_BASH_PATH_WSL"
        if [[ -f "$GIT_BASH_PATH_WSL" ]]; then
            THIS_BASE_NAME=$(basename "$0")
            "$GIT_BASH_PATH_WSL" "$THIS_BASE_NAME" $@
            exit $?
        else
            echo "Git Bash not found, please install Git Bash!" >&2
            exit 1
        fi
    else
        # Native WSL Linux environment - continue with Linux build
        echo "Using native WSL Linux environment for Linux build"
    fi
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_TYPE="${1:-Release}"
INSTALL_DIR="${2:-$PROJECT_ROOT/install}"

echo "Building and installing ccap..."
echo "Install Directory: $INSTALL_DIR"
echo "Project Root: $PROJECT_ROOT"

# Function to build and install a specific configuration
build_and_install_config() {
    local config=$1
    echo ""
    echo "========================================="
    echo "Building $config configuration..."
    echo "========================================="

    # Create build directory
    BUILD_DIR="$PROJECT_ROOT/build/$config"
    mkdir -p "$BUILD_DIR"

    cd "$BUILD_DIR"

    # Configure
    echo "Configuring $config..."
    cmake ../.. \
        -DCMAKE_BUILD_TYPE="$config" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCCAP_BUILD_EXAMPLES=OFF \
        -DCCAP_BUILD_TESTS=OFF \
        -DCCAP_INSTALL=ON

    # Build
    echo "Building $config..."
    cmake --build . --config "$config" --parallel $(detectCores 2>/dev/null)

    # Install
    echo "Installing $config..."
    cmake --install . --config "$config"

    echo "$config build and install completed!"
}

# Check if we're on Windows (including Git Bash, MSYS, Cygwin)
if isWindows; then
    echo "Windows environment detected - building both Debug and Release versions"
    echo "Debug libraries will have 'd' suffix (e.g., ccapd.lib)"

    # Build Debug version
    build_and_install_config "Debug"

    # Build Release version
    build_and_install_config "Release"

    echo ""
    echo "========================================="
    echo "All builds completed successfully!"
    echo "========================================="
    echo "Installation directory: $INSTALL_DIR"
    echo ""
    echo "Libraries installed:"
    if [[ -f "$INSTALL_DIR/lib/ccap.lib" ]]; then
        echo "  - ccap.lib (Release)"
    fi
    if [[ -f "$INSTALL_DIR/lib/ccapd.lib" ]]; then
        echo "  - ccapd.lib (Debug)"
    fi

else
    # Non-Windows: use the specified build type (default: Release)
    echo "Build Type: $BUILD_TYPE"
    build_and_install_config "$BUILD_TYPE"

    echo "Build and install completed successfully!"
    echo "Installation directory: $INSTALL_DIR"
fi

echo ""
echo "Installed files:"
find "$INSTALL_DIR" -type f | head -20
