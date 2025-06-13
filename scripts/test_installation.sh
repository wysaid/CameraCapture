#!/bin/bash

# Test script to verify ccap installation
# Usage: ./scripts/test_installation.sh [install_dir]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INSTALL_DIR="${1:-$PROJECT_ROOT/install}"

# Convert to absolute path if it's a relative path
if [[ ! "$INSTALL_DIR" = /* ]]; then
    INSTALL_DIR="$PROJECT_ROOT/$INSTALL_DIR"
fi

echo "Testing ccap installation..."
echo "Install Directory: $INSTALL_DIR"

# Check if installation directory exists
if [ ! -d "$INSTALL_DIR" ]; then
    echo "Error: Installation directory does not exist: $INSTALL_DIR"
    exit 1
fi

# Create a temporary test directory
TEST_DIR="$PROJECT_ROOT/test_install"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

# Create a simple test program
cat >"$TEST_DIR/test_ccap.cpp" <<'EOF'
#include <ccap.h>
#include <iostream>

int main() {
    std::cout << "Testing ccap library..." << std::endl;
    
    // Test basic functionality - enumerate devices
    ccap::Provider cameraProvider;
    auto deviceNames = cameraProvider.findDeviceNames();
    std::cout << "Found " << deviceNames.size() << " camera device(s)" << std::endl;
    
    // Print device names
    if (!deviceNames.empty()) {
        std::cout << "Available camera devices:" << std::endl;
        for (size_t i = 0; i < deviceNames.size(); ++i) {
            std::cout << "  " << i << ": " << deviceNames[i] << std::endl;
        }
    }
    
    std::cout << "ccap library test completed successfully!" << std::endl;
    
    return 0;
}
EOF

# Create CMakeLists.txt for the test
cat >"$TEST_DIR/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.14)
project(test_ccap)

set(CMAKE_CXX_STANDARD 17)

# Find ccap using the installed package
set(CMAKE_PREFIX_PATH "$INSTALL_DIR")
find_package(ccap REQUIRED)

add_executable(test_ccap test_ccap.cpp)
target_link_libraries(test_ccap ccap::ccap)

# Additional platform-specific libraries for macOS
if(APPLE)
    target_link_libraries(test_ccap 
        "-framework Foundation"
        "-framework AVFoundation" 
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate"
    )
endif()
EOF

echo ""
echo "Building test program..."
cd "$TEST_DIR"

# Configure and build the test
cmake . -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

echo ""
echo "Running test program..."
./test_ccap

echo ""
echo "Verification completed successfully!"

# Cleanup
cd "$PROJECT_ROOT"
rm -rf "$TEST_DIR"

echo ""
echo "Installation verification passed!"
echo "The ccap library has been successfully installed and tested."
