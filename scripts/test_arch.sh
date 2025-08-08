#!/bin/bash

# 测试脚本 - 检测当前环境的架构支持情况

# 获取脚本所在的真实目录路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== CCAP Architecture Detection Test ==="
echo "Project root: $PROJECT_ROOT"
echo

# 检测当前系统架构
echo "Current system architecture:"
uname -m
echo

# 检测编译器目标架构
echo "Compiler default target:"
clang -dumpmachine
echo

# 创建临时测试文件
cat >/tmp/arch_test.cpp <<'EOF'
#include <iostream>

int main() {
    std::cout << "Architecture detection test:" << std::endl;
    
#ifdef __aarch64__
    std::cout << "  __aarch64__ is defined" << std::endl;
#else
    std::cout << "  __aarch64__ is NOT defined" << std::endl;
#endif

#ifdef _M_ARM64
    std::cout << "  _M_ARM64 is defined" << std::endl;
#else
    std::cout << "  _M_ARM64 is NOT defined" << std::endl;
#endif

#ifdef __APPLE__
    std::cout << "  __APPLE__ is defined" << std::endl;
#else
    std::cout << "  __APPLE__ is NOT defined" << std::endl;
#endif

#ifdef __x86_64__
    std::cout << "  __x86_64__ is defined" << std::endl;
#else
    std::cout << "  __x86_64__ is NOT defined" << std::endl;
#endif

    return 0;
}
EOF

echo "Testing native compilation:"
clang++ -o /tmp/arch_test_native /tmp/arch_test.cpp
/tmp/arch_test_native
echo

echo "Testing ARM64 compilation:"
if clang++ -target arm64-apple-macos11 -o /tmp/arch_test_arm64 /tmp/arch_test.cpp 2>/dev/null; then
    echo "  ARM64 compilation successful"
    if [[ $(uname -m) == "arm64" ]]; then
        echo "  Running ARM64 binary:"
        /tmp/arch_test_arm64
    else
        echo "  Cannot run ARM64 binary on this architecture"
    fi
else
    echo "  ARM64 compilation failed"
fi
echo

echo "Testing x86_64 compilation:"
if clang++ -target x86_64-apple-macos10.13 -o /tmp/arch_test_x86_64 /tmp/arch_test.cpp 2>/dev/null; then
    echo "  x86_64 compilation successful"
    if [[ $(uname -m) == "x86_64" ]]; then
        echo "  Running x86_64 binary:"
        /tmp/arch_test_x86_64
    else
        echo "  Cannot run x86_64 binary on this architecture"
    fi
else
    echo "  x86_64 compilation failed"
fi

# 清理临时文件
rm -f /tmp/arch_test.cpp /tmp/arch_test_native /tmp/arch_test_arm64 /tmp/arch_test_x86_64

echo
echo "=== Test completed ==="
