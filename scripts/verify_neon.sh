#!/bin/bash

# 创建一个简单的NEON检测验证程序

# 获取脚本所在的真实目录路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== NEON Support Verification ==="
echo "Project root: $PROJECT_ROOT"
echo

# 创建测试程序
cat > /tmp/neon_test.cpp << 'EOF'
#include <iostream>

// 复制ccap的NEON检测逻辑
#if (defined(__aarch64__) || defined(_M_ARM64)) && \
    (defined(__APPLE__) || defined(_WIN32) || defined(__ANDROID__) || defined(__linux__))
#define ENABLE_NEON_IMP 1
#else
#define ENABLE_NEON_IMP 0
#endif

#if ENABLE_NEON_IMP
#include <arm_neon.h>

// NEON capability detection
#if defined(__APPLE__)
// On Apple platforms, NEON is always available on ARM64
inline bool hasNEON_() { return true; }
#elif defined(_WIN32)
// On Windows ARM64, NEON is standard
#include <intrin.h>
inline bool hasNEON_() 
{
    // Windows ARM64 always has NEON support
    return true;
}
#elif defined(__ANDROID__) || defined(__linux__)
// On Android/Linux, check through /proc/cpuinfo or assume available for ARM64
#include <sys/auxv.h>
inline bool hasNEON_()
{
#ifdef __aarch64__
    // NEON is mandatory in ARMv8-A (AArch64)
    return true;
#else
    // For ARMv7, check HWCAP_NEON
    return (getauxval(AT_HWCAP) & HWCAP_NEON) != 0;
#endif
}
#else
inline bool hasNEON_() { return false; }
#endif

#endif // ENABLE_NEON_IMP

int main() {
    std::cout << "=== NEON Detection Report ===" << std::endl;
    
    std::cout << "Compile-time architecture detection:" << std::endl;
#ifdef __aarch64__
    std::cout << "  __aarch64__ is defined (ARM64)" << std::endl;
#else
    std::cout << "  __aarch64__ is NOT defined" << std::endl;
#endif

#ifdef _M_ARM64
    std::cout << "  _M_ARM64 is defined (Windows ARM64)" << std::endl;
#else
    std::cout << "  _M_ARM64 is NOT defined" << std::endl;
#endif

#ifdef __APPLE__
    std::cout << "  __APPLE__ is defined" << std::endl;
#else
    std::cout << "  __APPLE__ is NOT defined" << std::endl;
#endif

    std::cout << "NEON implementation status:" << std::endl;
#if ENABLE_NEON_IMP
    std::cout << "  ENABLE_NEON_IMP = 1 (NEON implementation enabled)" << std::endl;
    std::cout << "  Runtime NEON support: " << (hasNEON_() ? "YES" : "NO") << std::endl;
#else
    std::cout << "  ENABLE_NEON_IMP = 0 (NEON implementation disabled)" << std::endl;
    std::cout << "  Runtime NEON support: N/A (not compiled)" << std::endl;
#endif

    return 0;
}
EOF

echo "Testing native compilation (current architecture):"
if clang++ -o /tmp/neon_test_native /tmp/neon_test.cpp 2>/dev/null; then
    echo "  Compilation successful"
    echo "  Running native binary:"
    /tmp/neon_test_native
else
    echo "  Compilation failed"
fi
echo

echo "Testing ARM64 compilation:"
if clang++ -target arm64-apple-macos11 -o /tmp/neon_test_arm64 /tmp/neon_test.cpp 2>/dev/null; then
    echo "  ARM64 compilation successful"
    if [[ $(uname -m) == "arm64" ]]; then
        echo "  Running ARM64 binary:"
        /tmp/neon_test_arm64
    else
        echo "  Cannot run ARM64 binary on x86_64 Mac"
        echo "  But compilation shows NEON would be detected correctly on ARM64"
    fi
else
    echo "  ARM64 compilation failed"
fi
echo

echo "Testing x86_64 compilation:"
if clang++ -target x86_64-apple-macos10.13 -o /tmp/neon_test_x86_64 /tmp/neon_test.cpp 2>/dev/null; then
    echo "  x86_64 compilation successful"
    if [[ $(uname -m) == "x86_64" ]]; then
        echo "  Running x86_64 binary:"
        /tmp/neon_test_x86_64
    else
        echo "  Cannot run x86_64 binary on ARM64 Mac"
    fi
else
    echo "  x86_64 compilation failed"
fi

# 清理临时文件
rm -f /tmp/neon_test.cpp /tmp/neon_test_native /tmp/neon_test_arm64 /tmp/neon_test_x86_64

echo
echo "=== Verification completed ==="
