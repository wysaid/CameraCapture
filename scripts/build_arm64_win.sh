#!/bin/bash
# Windows ARM64 build script for ccap
# This script builds ccap for Windows ARM64 with NEON support

echo "Building ccap for Windows ARM64..."

# Check if we're in a Windows environment
if ! command -v cmd.exe &>/dev/null; then
    echo "Warning: This script is designed for Windows environment with git-bash"
fi

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Go to the parent directory (project root)
cd "$SCRIPT_DIR/.."

# Create build directory
if [ ! -d "build-arm64" ]; then
    mkdir -p build-arm64
fi
cd build-arm64

echo "Configuring with Visual Studio for ARM64..."
echo "Using toolset: host=x64 to avoid ARM64 configuration issues"

# Try multiple cmake configurations in order of preference
CMAKE_SUCCESS=false

# First try: Visual Studio 17 2022 with host=x64 toolset
echo "Attempting configuration with Visual Studio 17 2022 and host=x64 toolset..."
if cmake .. -G "Visual Studio 17 2022" -A ARM64 -T host=x64 -DCCAP_FORCE_ARM64=ON -DCCAP_BUILD_EXAMPLES=ON; then
    CMAKE_SUCCESS=true
    echo "Configuration successful with host=x64 toolset"
else
    echo "Configuration failed with host=x64 toolset, trying alternative..."

    # Second try: Visual Studio 17 2022 with v143 toolset
    echo "Attempting configuration with v143 toolset..."
    if cmake .. -G "Visual Studio 17 2022" -A ARM64 -T v143 -DCCAP_FORCE_ARM64=ON -DCCAP_BUILD_EXAMPLES=ON; then
        CMAKE_SUCCESS=true
        echo "Configuration successful with v143 toolset"
    else
        echo "Configuration failed with v143 toolset, trying without explicit toolset..."

        # Third try: Visual Studio 17 2022 without explicit toolset
        echo "Attempting configuration without explicit toolset..."
        if cmake .. -G "Visual Studio 17 2022" -A ARM64 -DCCAP_FORCE_ARM64=ON -DCCAP_BUILD_EXAMPLES=ON; then
            CMAKE_SUCCESS=true
            echo "Configuration successful without explicit toolset"
        fi
    fi
fi

if [ "$CMAKE_SUCCESS" = false ]; then
    echo "All configuration attempts failed!"
    echo ""
    echo "Possible solutions:"
    echo "1. Install 'MSVC v143 - VS 2022 C++ ARM64 build tools' in Visual Studio Installer"
    echo "2. Install 'Windows 11 SDK' in Visual Studio Installer"
    echo "3. Make sure Visual Studio 2022 is properly installed with C++ ARM64 support"
    echo ""
    echo "To install ARM64 build tools:"
    echo "- Open Visual Studio Installer"
    echo "- Modify Visual Studio 2022"
    echo "- Go to Individual components"
    echo "- Search for 'ARM64' and install all ARM64-related components"
    echo "- Also install 'Windows 11 SDK (10.0.22621.0)' or later"
    read -p "Press any key to continue..."
    exit 1
fi

# Build the project
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo "Build failed!"
    read -p "Press any key to continue..."
    exit 1
fi

echo "Build completed successfully!"
echo "ARM64 binaries are in build-arm64/Release/"

read -p "Press any key to continue..."
