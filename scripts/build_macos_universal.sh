#!/bin/bash

# Build script for macOS Universal Binary (x86_64 + arm64)
# Usage: ./scripts/build_macos_universal.sh [Debug|Release]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_TYPE="${1:-Release}"

echo "Building ccap Universal Binary for macOS..."
echo "Build Type: $BUILD_TYPE"
echo "Project Root: $PROJECT_ROOT"

# Create build directories
BUILD_DIR="$PROJECT_ROOT/build"
X86_64_DIR="$BUILD_DIR/x86_64"
ARM64_DIR="$BUILD_DIR/arm64"
UNIVERSAL_DIR="$BUILD_DIR/universal"

mkdir -p "$X86_64_DIR" "$ARM64_DIR" "$UNIVERSAL_DIR"

# Build for x86_64
echo "Building for x86_64..."
cd "$X86_64_DIR"
cmake ../.. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_INSTALL_PREFIX="$UNIVERSAL_DIR" \
    -DCCAP_BUILD_EXAMPLES=OFF \
    -DCCAP_BUILD_TESTS=OFF \
    -DCCAP_INSTALL=ON

cmake --build . --config "$BUILD_TYPE" --parallel $(sysctl -n hw.ncpu)

# Build for arm64
echo "Building for arm64..."
cd "$ARM64_DIR"
cmake ../.. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_INSTALL_PREFIX="$UNIVERSAL_DIR" \
    -DCCAP_BUILD_EXAMPLES=OFF \
    -DCCAP_BUILD_TESTS=OFF \
    -DCCAP_INSTALL=ON

cmake --build . --config "$BUILD_TYPE" --parallel $(sysctl -n hw.ncpu)

# Create universal binary
echo "Creating universal binary..."
X86_64_LIB="$X86_64_DIR/libccap.a"
ARM64_LIB="$ARM64_DIR/libccap.a"
UNIVERSAL_LIB="$UNIVERSAL_DIR/lib/libccap.a"

# Install headers from x86_64 build (they should be identical)
cd "$X86_64_DIR"
cmake --install . --config "$BUILD_TYPE"

# Create universal library
mkdir -p "$(dirname "$UNIVERSAL_LIB")"
lipo -create "$X86_64_LIB" "$ARM64_LIB" -output "$UNIVERSAL_LIB"

# Verify the universal binary
echo "Verifying universal binary:"
file "$UNIVERSAL_LIB"
lipo -info "$UNIVERSAL_LIB"

echo "Universal binary created successfully!"
echo "Installation directory: $UNIVERSAL_DIR"
echo "Library: $UNIVERSAL_LIB"
echo "Headers: $UNIVERSAL_DIR/include"
