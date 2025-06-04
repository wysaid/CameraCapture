#!/usr/bin/env bash

# 定位到项目根目录（脚本文件的上一级目录）
cd "$(dirname "$0")/.."

# 对 src、tests、examples 目录执行 clang-format
# 排除 examples/desktop/glfw 和 examples/desktop/glad 目录
find src tests examples \
    -type f \
    \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.h" -o -name "*.hpp" -o -name "*.hxx" \) \
    -not -path "examples/desktop/glfw/*" \
    -not -path "examples/desktop/glad/*" \
    -exec clang-format -i {} +

echo "代码格式化完成！"
