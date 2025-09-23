#!/bin/bash

# Android Build Script for CCAP Demo
# This script helps build the Android demo with proper environment setup

set -e

echo "🤖 CCAP Android Build Script"
echo "============================="

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to find Android SDK
find_android_sdk() {
    # Common locations for Android SDK
    local sdk_locations=(
        "$HOME/Library/Android/sdk"
        "$HOME/Android/Sdk"
        "/usr/local/android-sdk"
        "/opt/android-sdk"
        "$ANDROID_HOME"
        "$ANDROID_SDK_ROOT"
    )

    for location in "${sdk_locations[@]}"; do
        if [ -d "$location" ] && [ -f "$location/platform-tools/adb" ]; then
            echo "$location"
            return 0
        fi
    done
    return 1
}

# Function to find Android NDK
find_android_ndk() {
    # Try NDK within SDK first
    if [ -n "$ANDROID_SDK_ROOT" ] && [ -d "$ANDROID_SDK_ROOT/ndk" ]; then
        # Find the highest version NDK
        local ndk_dir=$(find "$ANDROID_SDK_ROOT/ndk" -maxdepth 1 -type d -name "[0-9]*" | sort -V | tail -n1)
        if [ -n "$ndk_dir" ] && [ -f "$ndk_dir/build/cmake/android.toolchain.cmake" ]; then
            echo "$ndk_dir"
            return 0
        fi
    fi

    # Common standalone locations
    local ndk_locations=(
        "$HOME/Library/Android/sdk/ndk-bundle"
        "$HOME/Android/Sdk/ndk-bundle"
        "/usr/local/android-ndk"
        "/opt/android-ndk"
        "$ANDROID_NDK_ROOT"
        "$ANDROID_NDK_HOME"
    )

    for location in "${ndk_locations[@]}"; do
        if [ -d "$location" ] && [ -f "$location/build/cmake/android.toolchain.cmake" ]; then
            echo "$location"
            return 0
        fi
    done
    return 1
}

# Check prerequisites
echo "🔍 Checking prerequisites..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: This script must be run from the CCAP root directory"
    echo "   Expected to find CMakeLists.txt in current directory"
    exit 1
fi

# Find Android SDK
if [ -z "$ANDROID_SDK_ROOT" ]; then
    echo "🔍 ANDROID_SDK_ROOT not set, searching for Android SDK..."
    if ANDROID_SDK_ROOT=$(find_android_sdk); then
        echo "✅ Found Android SDK at: $ANDROID_SDK_ROOT"
        export ANDROID_SDK_ROOT
    else
        echo "❌ Android SDK not found. Please install Android SDK and set ANDROID_SDK_ROOT"
        echo "   Download from: https://developer.android.com/studio"
        exit 1
    fi
else
    echo "✅ Using Android SDK: $ANDROID_SDK_ROOT"
fi

# Find Android NDK
if [ -z "$ANDROID_NDK_ROOT" ]; then
    echo "🔍 ANDROID_NDK_ROOT not set, searching for Android NDK..."
    if ANDROID_NDK_ROOT=$(find_android_ndk); then
        echo "✅ Found Android NDK at: $ANDROID_NDK_ROOT"
        export ANDROID_NDK_ROOT
    else
        echo "❌ Android NDK not found. Please install Android NDK and set ANDROID_NDK_ROOT"
        echo "   You can install it via Android Studio SDK Manager"
        exit 1
    fi
else
    echo "✅ Using Android NDK: $ANDROID_NDK_ROOT"
fi

# Check CMake
if ! command_exists cmake; then
    echo "❌ CMake not found. Please install CMake 3.18.1 or later"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | sed 's/cmake version //')
echo "✅ CMake version: $CMAKE_VERSION"

# Check if gradlew exists in demo project
DEMO_DIR="examples/android"
if [ ! -f "$DEMO_DIR/gradlew" ]; then
    echo "❌ Android demo project not found at $DEMO_DIR"
    exit 1
fi

echo "✅ Android demo project found"

# Build options
ABI_ARM64="arm64-v8a"
ABI_ARM32="armeabi-v7a"
BUILD_TYPE="Release"

# Parse command line arguments
BUILD_ARM64=true
BUILD_ARM32=true
BUILD_DEMO=true
CLEAN_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
    --arm64-only)
        BUILD_ARM32=false
        shift
        ;;
    --arm32-only)
        BUILD_ARM64=false
        shift
        ;;
    --lib-only)
        BUILD_DEMO=false
        shift
        ;;
    --clean)
        CLEAN_BUILD=true
        shift
        ;;
    --debug)
        BUILD_TYPE="Debug"
        shift
        ;;
    --help)
        echo "Usage: $0 [OPTIONS]"
        echo "Options:"
        echo "  --arm64-only    Build only ARM64 library"
        echo "  --arm32-only    Build only ARM32 library"
        echo "  --lib-only      Build only native libraries (skip APK)"
        echo "  --clean         Clean build directories first"
        echo "  --debug         Build debug version"
        echo "  --help          Show this help"
        exit 0
        ;;
    *)
        echo "Unknown option: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
    esac
done

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo "🧹 Cleaning build directories..."
    rm -rf build/android build/android-v7a
    if [ -d "$DEMO_DIR" ]; then
        cd "$DEMO_DIR"
        ./gradlew clean 2>/dev/null || true
        cd - >/dev/null
    fi
    echo "✅ Clean completed"
fi

# Build native libraries
echo "🔨 Building CCAP native libraries..."

if [ "$BUILD_ARM64" = true ]; then
    echo "📱 Building ARM64 library..."
    mkdir -p build/android
    cd build/android

    # Copy the Android CMake configuration
    cp ../../cmake/android_build.cmake CMakeLists.txt

    cmake . \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI_ARM64" \
        -DANDROID_PLATFORM=android-21 \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    make -j$(nproc 2>/dev/null || echo 4)
    cd - >/dev/null
    echo "✅ ARM64 library built successfully"
fi

if [ "$BUILD_ARM32" = true ]; then
    echo "📱 Building ARM32 library..."
    mkdir -p build/android-v7a
    cd build/android-v7a

    # Copy the Android CMake configuration
    cp ../../cmake/android_build.cmake CMakeLists.txt

    cmake . \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI_ARM32" \
        -DANDROID_PLATFORM=android-21 \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    make -j$(nproc 2>/dev/null || echo 4)
    cd - >/dev/null
    echo "✅ ARM32 library built successfully"
fi

# Build Android demo
if [ "$BUILD_DEMO" = true ]; then
    echo "📱 Building Android demo APK..."
    cd "$DEMO_DIR"

    # Set executable permission for gradlew
    chmod +x gradlew

    # Build APK
    if [ "$BUILD_TYPE" = "Debug" ]; then
        ./gradlew assembleDebug
        APK_PATH="app/build/outputs/apk/debug/app-debug.apk"
    else
        ./gradlew assembleRelease
        APK_PATH="app/build/outputs/apk/release/app-release-unsigned.apk"
    fi

    cd - >/dev/null

    if [ -f "$DEMO_DIR/$APK_PATH" ]; then
        echo "✅ Android demo APK built successfully"
        echo "📦 APK location: $DEMO_DIR/$APK_PATH"

        # Check if device is connected
        if command_exists adb && [ "$(adb devices | wc -l)" -gt 1 ]; then
            echo ""
            echo "📱 Android device detected. To install the APK:"
            echo "   cd $DEMO_DIR && ./gradlew install$([ "$BUILD_TYPE" = "Debug" ] && echo "Debug" || echo "Release")"
        fi
    else
        echo "❌ Failed to build Android demo APK"
        exit 1
    fi
fi

echo ""
echo "🎉 Build completed successfully!"
echo "   ARM64 library: $([ "$BUILD_ARM64" = true ] && echo "✅" || echo "⏭️ ")"
echo "   ARM32 library: $([ "$BUILD_ARM32" = true ] && echo "✅" || echo "⏭️ ")"
echo "   Demo APK: $([ "$BUILD_DEMO" = true ] && echo "✅" || echo "⏭️ ")"
echo ""
echo "💡 Next steps:"
echo "   1. Install the APK on your Android device"
echo "   2. Grant camera permissions when prompted"
echo "   3. Test camera functionality in the demo app"
