#!/bin/bash

# 脚本用于构建x86_64版本的ccap

# 获取脚本所在的真实目录路径
cd "$(dirname $0)"
SCRIPT_DIR="$(pwd)"
cd ..
PROJECT_ROOT="$(pwd)"

BUILD_TYPE=${1:-Debug}
echo "Building x86_64 version with build type: $BUILD_TYPE"
echo "Project root: $PROJECT_ROOT"

# 进入项目根目录
cd "$PROJECT_ROOT"

# 创建x86_64专用的构建目录
mkdir -p build/x86_64-$BUILD_TYPE
cd build/x86_64-$BUILD_TYPE

# 配置CMake以使用x86_64架构
cmake ../.. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_OSX_ARCHITECTURES=x86_64

# 编译
cmake --build . --config $BUILD_TYPE --parallel $(sysctl -n hw.ncpu)

echo "x86_64 build completed in build/x86_64-$BUILD_TYPE"
