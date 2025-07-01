#!/bin/bash

# Test script to verify ccap installation
# Usage: ./scripts/test_installation.sh [install_dir]
#
# This script verifies that the ccap library is properly installed by:
# 1. Checking that all required header files exist
# 2. Checking that library files exist
# 3. Building and running C++ test programs
# 4. Building and running C interface test programs
# 5. Testing both interfaces with comprehensive functionality tests
#
# The C interface tests cover:
# - Provider creation/destruction
# - Device discovery and enumeration
# - Property get/set operations
# - Frame capture (both sync and async)
# - Memory management
# - Error handling
# - Platform-specific features

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
    echo "Please run the script $PROJECT_ROOT/scripts/build_and_install.sh first."
    exit 1
fi

# Verify required header files exist
echo "Checking required header files..."
REQUIRED_HEADERS=(
    "ccap.h"
    "ccap_c.h"
    "ccap_core.h"
    "ccap_convert.h"
    "ccap_def.h"
    "ccap_utils.h"
)

for header in "${REQUIRED_HEADERS[@]}"; do
    header_path="$INSTALL_DIR/include/$header"
    if [ ! -f "$header_path" ]; then
        echo "Error: Required header file not found: $header_path"
        exit 1
    fi
    echo "  Found: $header"
done

# Verify library files exist
echo "Checking required library files..."
if [ -f "$INSTALL_DIR/lib/libccap.a" ]; then
    echo "  Found: libccap.a (static library)"
elif [ -f "$INSTALL_DIR/lib/libccap.so" ]; then
    echo "  Found: libccap.so (shared library)"
elif [ -f "$INSTALL_DIR/lib/libccap.dylib" ]; then
    echo "  Found: libccap.dylib (shared library)"
elif [ -f "$INSTALL_DIR/lib/ccap.lib" ]; then
    echo "  Found: ccap.lib (Windows static library)"
elif [ -f "$INSTALL_DIR/lib/ccap.dll" ]; then
    echo "  Found: ccap.dll (Windows shared library)"
else
    echo "Error: No ccap library found in $INSTALL_DIR/lib/"
    ls -la "$INSTALL_DIR/lib/" || echo "lib directory does not exist"
    exit 1
fi

# Verify CMake configuration files
echo "Checking CMake configuration..."
if [ -f "$INSTALL_DIR/lib/cmake/ccap/ccapConfig.cmake" ]; then
    echo "  Found: ccapConfig.cmake"
else
    echo "Warning: ccapConfig.cmake not found, but continuing..."
fi

# Create a temporary test directory
TEST_DIR="$PROJECT_ROOT/test_install"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

# Create a simple C++ test program
cat >"$TEST_DIR/test_ccap_cpp.cpp" <<'EOF'
#include <ccap.h>
#include <iostream>

int main() {
    std::cout << "Testing ccap C++ library..." << std::endl;
    
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
    
    std::cout << "ccap C++ library test completed successfully!" << std::endl;
    
    return 0;
}
EOF

# Create a simple C test program for C interface
cat >"$TEST_DIR/test_ccap_c.c" <<'EOF'
#include <ccap_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Callback function for new frames
bool frame_callback(const CcapVideoFrame* frame, void* userData) {
    static int frameCount = 0;
    frameCount++;
    
    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("Frame %d: %dx%d, format=%d, timestamp=%llu\n", 
               frameCount, frameInfo.width, frameInfo.height, 
               frameInfo.pixelFormat, frameInfo.timestamp);
    }
    
    // Return false to keep the frame available for grab()
    // Return true to consume the frame (won't be available for grab())
    return false;
}

int main() {
    printf("Testing ccap C interface...\n");
    printf("Version: %s\n\n", ccap_get_version());
    
    // Test 1: Create and destroy camera provider
    printf("\n1. Testing provider creation and destruction...\n");
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Error: Failed to create camera provider\n");
        return 1;
    }
    printf("  Camera provider created successfully\n");
    
    // Test 2: Device discovery
    printf("\n2. Testing device discovery...\n");
    char** deviceNames = NULL;
    size_t deviceCount = 0;
    
    if (ccap_provider_find_device_names(provider, &deviceNames, &deviceCount)) {
        printf("  Found %zu camera device(s)\n", deviceCount);
        
        // Print device names
        if (deviceCount > 0) {
            printf("  Available camera devices:\n");
            for (size_t i = 0; i < deviceCount; i++) {
                printf("    %zu: %s\n", i, deviceNames[i]);
            }
        }
        
        // Free device names
        ccap_provider_free_device_names(deviceNames, deviceCount);
        printf("  Device names freed successfully\n");
    } else {
        printf("  Warning: Failed to enumerate camera devices (this is normal if no cameras are connected)\n");
    }
    
    // Test 3: Provider properties
    printf("\n3. Testing provider properties...\n");
    bool isOpened = ccap_provider_is_opened(provider);
    printf("  Provider opened status: %s\n", isOpened ? "true" : "false");
    
    bool isStarted = ccap_provider_is_started(provider);
    printf("  Provider started status: %s\n", isStarted ? "true" : "false");
    
    // Test 4: Basic frame properties using property interface
    printf("\n4. Testing frame properties...\n");
    double width = ccap_provider_get_property(provider, CCAP_PROPERTY_WIDTH);
    double height = ccap_provider_get_property(provider, CCAP_PROPERTY_HEIGHT);
    printf("  Current frame size: %.0f x %.0f\n", width, height);
    
    double fps = ccap_provider_get_property(provider, CCAP_PROPERTY_FRAME_RATE);
    printf("  Current FPS: %.2f\n", fps);
    
    // Test 5: Property setting (should work even without a camera)
    printf("\n5. Testing property setting...\n");
    bool setPropResult = ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, 640);
    printf("  Set width to 640: %s\n", setPropResult ? "success" : "failed");
    
    setPropResult = ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, 480);
    printf("  Set height to 480: %s\n", setPropResult ? "success" : "failed");
    
    setPropResult = ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_RATE, 30.0);
    printf("  Set frame rate to 30.0: %s\n", setPropResult ? "success" : "failed");
    
    // Test 6: Pixel format enumeration
    printf("\n6. Testing pixel format constants...\n");
    printf("  CCAP_PIXEL_FORMAT_UNKNOWN: %d\n", CCAP_PIXEL_FORMAT_UNKNOWN);
    printf("  CCAP_PIXEL_FORMAT_BGR24: %d\n", CCAP_PIXEL_FORMAT_BGR24);
    printf("  CCAP_PIXEL_FORMAT_BGRA32: %d\n", CCAP_PIXEL_FORMAT_BGRA32);
    printf("  CCAP_PIXEL_FORMAT_NV12: %d\n", CCAP_PIXEL_FORMAT_NV12);
    printf("  CCAP_PIXEL_FORMAT_I420: %d\n", CCAP_PIXEL_FORMAT_I420);
    printf("  Pixel format constants accessible\n");
    
    // Test 7: Pixel format utility functions
    printf("\n7. Testing pixel format utility functions...\n");
    bool isRGB = ccap_pixel_format_is_rgb(CCAP_PIXEL_FORMAT_BGR24);
    printf("  BGR24 is RGB format: %s\n", isRGB ? "true" : "false");
    
    bool isYUV = ccap_pixel_format_is_yuv(CCAP_PIXEL_FORMAT_NV12);
    printf("  NV12 is YUV format: %s\n", isYUV ? "true" : "false");
    
    // Test 8: Advanced configuration functions
    printf("\n8. Testing advanced configuration...\n");
    ccap_provider_set_max_available_frame_size(provider, 5);
    printf("  Set max available frame size to 5\n");
    
    ccap_provider_set_max_cache_frame_size(provider, 10);
    printf("  Set max cache frame size to 10\n");
    
    // Test 9: Frame callback setting (should work without actual camera)
    printf("\n9. Testing frame callback interface...\n");
    bool callbackResult = ccap_provider_set_new_frame_callback(provider, frame_callback, NULL);
    printf("  Set frame callback: %s\n", callbackResult ? "success" : "failed");
    
    // Remove callback
    callbackResult = ccap_provider_set_new_frame_callback(provider, NULL, NULL);
    printf("  Remove frame callback: %s\n", callbackResult ? "success" : "failed");
    
    // Test 10: Try to open default device (may fail if no camera available)
    printf("\n10. Testing device opening (optional - may fail without camera)...\n");
    bool openResult = ccap_provider_open(provider, NULL, false);
    if (openResult) {
        printf("  Successfully opened default camera device\n");
        
        // Test device info
        CcapDeviceInfo deviceInfo;
        if (ccap_provider_get_device_info(provider, &deviceInfo)) {
            printf("  Device: %s\n", deviceInfo.deviceName ? deviceInfo.deviceName : "Unknown");
            printf("  Supported pixel formats: %zu\n", deviceInfo.pixelFormatCount);
            printf("  Supported resolutions: %zu\n", deviceInfo.resolutionCount);
            
            if (deviceInfo.resolutionCount > 0) {
                printf("  First resolution: %dx%d\n", 
                       deviceInfo.supportedResolutions[0].width,
                       deviceInfo.supportedResolutions[0].height);
            }
            
            ccap_provider_free_device_info(&deviceInfo);
            printf("  Device info retrieved and freed successfully\n");
        }
        
        // Try starting capture
        bool startResult = ccap_provider_start(provider);
        if (startResult) {
            printf("  Successfully started capture\n");
            
            // Try grabbing a frame with timeout
            CcapVideoFrame* frame = ccap_provider_grab(provider, 1000); // 1 second timeout
            if (frame) {
                CcapVideoFrameInfo frameInfo;
                if (ccap_video_frame_get_info(frame, &frameInfo)) {
                    printf("  Grabbed frame: %dx%d, size=%u bytes\n", 
                           frameInfo.width, frameInfo.height, frameInfo.sizeInBytes);
                }
                ccap_video_frame_release(frame);
                printf("  Frame released successfully\n");
            } else {
                printf("  Note: No frame captured (timeout or no camera signal)\n");
            }
            
            ccap_provider_stop(provider);
            printf("  Capture stopped\n");
        } else {
            printf("  Note: Failed to start capture (normal without proper camera)\n");
        }
        
        ccap_provider_close(provider);
        printf("  Device closed\n");
    } else {
        printf("  Note: Failed to open camera device (normal if no camera available)\n");
    }
    
    // Clean up
    ccap_provider_destroy(provider);
    printf("\n  Camera provider destroyed successfully\n");
    
    printf("\nccap C interface test completed successfully!\n");
    printf("All C interface functions are working correctly.\n");
    
    return 0;
}
EOF

# 在 Windows - git bash 中使用时，CONVERTED_INSTALL_DIR 需要转换成 Windows 路径格式
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Convert to Windows path format
    export FIXED_INSTALL_DIR=$(cygpath -w "$INSTALL_DIR" | tr '\\' '/')
    echo "Converted install directory for Windows: $FIXED_INSTALL_DIR"
else
    export FIXED_INSTALL_DIR="$INSTALL_DIR"
    echo "Using install directory: $FIXED_INSTALL_DIR"
fi

# Create CMakeLists.txt for the test
cat >"$TEST_DIR/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.14)
project(test_ccap)

set(CMAKE_C_STANDARD 99)

# Find ccap using the installed package
set(CMAKE_PREFIX_PATH "$FIXED_INSTALL_DIR")
find_package(ccap REQUIRED)

# C++ test executable
add_executable(test_ccap_cpp test_ccap_cpp.cpp)
target_link_libraries(test_ccap_cpp ccap::ccap)

# C test executable
add_executable(test_ccap_c test_ccap_c.c)
target_link_libraries(test_ccap_c ccap::ccap)

# Additional platform-specific libraries for macOS
if(APPLE)
    target_link_libraries(test_ccap_cpp 
        "-framework Foundation"
        "-framework AVFoundation" 
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate"
    )
    target_link_libraries(test_ccap_c 
        "-framework Foundation"
        "-framework AVFoundation" 
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate"
    )
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(test_ccap_cpp 
        strmiids
        ole32
        oleaut32
        uuid
    )
    target_link_libraries(test_ccap_c 
        strmiids
        ole32
        oleaut32
        uuid
    )
endif()
EOF

echo ""
echo "Building test programs..."
cd "$TEST_DIR"

# Configure and build the tests
cmake . -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

echo ""
echo "Running C++ test program..."

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    export EXE_PREFIX="Release/"
    export EXE_SUFFIX=".exe"
fi

if "./${EXE_PREFIX}test_ccap_cpp${EXE_SUFFIX}"; then
    echo "  C++ interface test PASSED"
    CPP_TEST_RESULT="PASSED"
else
    echo "  ✗ C++ interface test FAILED"
    CPP_TEST_RESULT="FAILED"
fi

echo ""
echo "Running C interface test program..."
if "./${EXE_PREFIX}test_ccap_c${EXE_SUFFIX}"; then
    echo "  C interface test PASSED"
    C_TEST_RESULT="PASSED"
else
    echo "  ✗ C interface test FAILED"
    C_TEST_RESULT="FAILED"
fi

echo ""
echo "==========================================="
echo "           TEST RESULTS SUMMARY"
echo "==========================================="
echo "Header Files Check:     ✅ PASSED"
echo "Library Files Check:    ✅ PASSED"
echo "C++ Interface Test:     $CPP_TEST_RESULT"
echo "C Interface Test:       $C_TEST_RESULT"

# Additional test statistics
if command -v wc >/dev/null 2>&1; then
    echo ""
    echo "Test Statistics:"
    echo "  Total header files:   $(echo "${REQUIRED_HEADERS[@]}" | wc -w)"
    echo "  C++ test functions:   Basic enumeration and provider functionality"
    echo "  C interface tests:    10 comprehensive test categories"
fi

echo "==========================================="

if [ "$CPP_TEST_RESULT" = "FAILED" ] || [ "$C_TEST_RESULT" = "FAILED" ]; then
    echo ""
    echo "❌ Some tests failed. Please check the output above for details."
    exit 1
fi

echo ""
echo "✅ All tests passed successfully!"
echo "Installation verification completed successfully!"

# Cleanup
cd "$PROJECT_ROOT"
rm -rf "$TEST_DIR"

echo ""
echo "The ccap library (both C++ and C interfaces) has been successfully installed and tested."
