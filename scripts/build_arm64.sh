#!/bin/bash

# 脚本用于在任何Mac设备上编译ARM64版本的ccap

# 获取脚本所在的真实目录路径
cd "$(dirname $0)"
SCRIPT_DIR="$(pwd)"
cd ..
PROJECT_ROOT="$(pwd)"

BUILD_TYPE=${1:-Debug}
echo "Building ARM64 version with build type: $BUILD_TYPE"
echo "Project root: $PROJECT_ROOT"

# 进入项目根目录
cd "$PROJECT_ROOT"

# 创建ARM64专用的构建目录
mkdir -p build/arm64-$BUILD_TYPE
cd build/arm64-$BUILD_TYPE

# 配置CMake以强制使用ARM64架构
cmake ../.. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCCAP_FORCE_ARM64=ON

# 编译
cmake --build . --config $BUILD_TYPE --parallel $(sysctl -n hw.ncpu)

echo "ARM64 build completed in $PROJECT_ROOT/build/arm64-$BUILD_TYPE"
echo "To test NEON detection, you can run the examples (note: they won't execute on Intel Macs)"
