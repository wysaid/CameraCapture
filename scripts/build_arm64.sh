#!/bin/bash

# Script for building ARM64 version of ccap on any Mac device

# Get the real directory path of the script
cd "$(dirname $0)"
SCRIPT_DIR="$(pwd)"
cd ..
PROJECT_ROOT="$(pwd)"

BUILD_TYPE=${1:-Debug}
echo "Building ARM64 version with build type: $BUILD_TYPE"
echo "Project root: $PROJECT_ROOT"

# Enter the project root directory
cd "$PROJECT_ROOT"

# Create a dedicated build directory for ARM64
mkdir -p build/arm64-$BUILD_TYPE
cd build/arm64-$BUILD_TYPE

# Configure CMake to force ARM64 architecture
cmake ../.. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCCAP_FORCE_ARM64=ON

# Build
cmake --build . --config $BUILD_TYPE --parallel $(sysctl -n hw.ncpu)

echo "ARM64 build completed in $PROJECT_ROOT/build/arm64-$BUILD_TYPE"
echo "To test NEON detection, you can run the examples (note: they won't execute on Intel Macs)"
