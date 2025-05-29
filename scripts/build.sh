#!/bin/bash

# 通用构建脚本，支持多种架构和配置

# 获取脚本所在的真实目录路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ARCH=${1:-native}      # native, arm64, x86_64, universal
BUILD_TYPE=${2:-Debug} # Debug, Release

echo "Building ccap for architecture: $ARCH, build type: $BUILD_TYPE"
echo "Project root: $PROJECT_ROOT"

# 进入项目根目录
cd "$PROJECT_ROOT"

case $ARCH in
"arm64")
    "$SCRIPT_DIR/build_arm64.sh" $BUILD_TYPE
    ;;
"x86_64")
    "$SCRIPT_DIR/build_x86_64.sh" $BUILD_TYPE
    ;;
"universal")
    "$SCRIPT_DIR/build_arm64.sh" $BUILD_TYPE
    "$SCRIPT_DIR/build_x86_64.sh" $BUILD_TYPE
    echo "Universal build completed. You can use lipo to combine the libraries if needed."
    ;;
"native")
    echo "Building for native architecture..."
    mkdir -p build/$BUILD_TYPE
    cd build/$BUILD_TYPE
    cmake ../.. -DCMAKE_BUILD_TYPE=$BUILD_TYPE
    cmake --build . --config $BUILD_TYPE --parallel $(sysctl -n hw.ncpu)
    ;;
*)
    echo "Unsupported architecture: $ARCH"
    echo "Supported architectures: native, arm64, x86_64, universal"
    exit 1
    ;;
esac

echo "Build completed!"
