/**
 * @file ccap_convert_avx2.h.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_CONVERT_AVX2_H
#define CCAP_CONVERT_AVX2_H

#include <cstdint>

#if (defined(_MSC_VER) || defined(_WIN32)) && !defined(__arm__) && !defined(__aarch64__) && !defined(_M_ARM) && !defined(_M_ARM64)
#define ENABLE_AVX2_IMP 1
#else
#define ENABLE_AVX2_IMP 0
#endif

#if ENABLE_AVX2_IMP
/// macOS 上直接使用 Accelerate.framework, 暂时不需要单独实现
#include <immintrin.h> // AVX2

#if defined(_MSC_VER)
#include <intrin.h>
inline bool hasAVX2_()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    bool avx = (cpuInfo[2] & (1 << 28)) != 0;
    if (!(osxsave && avx))
        return false;
    // 检查 XGETBV，确认 OS 支持 YMM
    unsigned long long xcrFeatureMask = _xgetbv(0);
    if ((xcrFeatureMask & 0x6) != 0x6)
        return false;
    // 检查 AVX2
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 5)) != 0;
}
#elif defined(__GNUC__) && defined(_WIN32)
#include <cpuid.h>
inline bool hasAVX2_()
{
    unsigned int eax, ebx, ecx, edx;
    // 1. 检查 AVX 和 OSXSAVE
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        return false;
    bool osxsave = (ecx & (1 << 27)) != 0;
    bool avx = (ecx & (1 << 28)) != 0;
    if (!(osxsave && avx))
        return false;
    // 2. 检查 XGETBV
    unsigned int xcr0_lo = 0, xcr0_hi = 0;
#if defined(_XCR_XFEATURE_ENABLED_MASK)
    asm volatile("xgetbv"
                 : "=a"(xcr0_lo), "=d"(xcr0_hi)
                 : "c"(0));
#else
    asm volatile("xgetbv"
                 : "=a"(xcr0_lo), "=d"(xcr0_hi)
                 : "c"(0));
#endif
    if ((xcr0_lo & 0x6) != 0x6)
        return false;
    // 3. 检查 AVX2
    if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx))
        return false;
    return (ebx & (1 << 5)) != 0;
}
#else
inline bool hasAVX2_() { return false; }
#endif

#endif

namespace ccap
{
bool hasAVX2();

#if ENABLE_AVX2_IMP
// NV12 转 BGRA32，AVX2加速
void nv12ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 转 RGBA32，AVX2加速
void nv12ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 转 BGR24，AVX2加速
void nv12ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// NV12 转 RGB24，AVX2加速
void nv12ToRgb24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// I420 转 BGRA32，AVX2加速
void i420ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// I420 转 RGBA32，AVX2加速
void i420ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// I420 转 BGR24，AVX2加速
void i420ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// I420 转 RGB24，AVX2加速
void i420ToRgb24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);
#else

#define nv12ToBgr24_avx2(...) assert(0 && "AVX2 not supported")
#define nv12ToBgra32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToBgra32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToBgr24_avx2(...) assert(0 && "AVX2 not supported")

#endif

} // namespace ccap

#endif // CCAP_CONVERT_AVX2_H