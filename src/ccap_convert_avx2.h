/**
 * @file ccap_convert_avx2.h.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_CONVERT_AVX2_H
#define CCAP_CONVERT_AVX2_H

#include "ccap_convert.h"

#include <cstdint>

#if __APPLE__
#include <TargetConditionals.h>
#endif

#ifndef ENABLE_AVX2_IMP
#if ((defined(_MSC_VER) || defined(_WIN32)) && !defined(__arm__) && !defined(__aarch64__) && !defined(_M_ARM) && !defined(_M_ARM64)) || \
    (defined(__APPLE__) && defined(__x86_64__) &&                                                                                       \
     !((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE))) || \
    (defined(__linux__) && defined(__x86_64__))
#define ENABLE_AVX2_IMP 1
#else
#define ENABLE_AVX2_IMP 0
#endif
#endif

namespace ccap {
bool hasAVX2();

bool canUseAVX2(); // 检查 AVX2 是否可用（硬件支持且未被禁用）

bool enableAVX2(bool enable); // Disable AVX2 implementation, useful for testing

// 获取详细的 AVX2 支持信息（用于调试）
const char* getAVX2SupportInfo();

#if ENABLE_AVX2_IMP

template <int inputChannels, int outputChannels, int swapRB>
void colorShuffle_avx2(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 to BGRA32, AVX2 accelerated
void nv12ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// NV12 to RGBA32, AVX2 accelerated
void nv12ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// NV12 to BGR24, AVX2 accelerated
void nv12ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// NV12 to RGB24, AVX2 accelerated
void nv12ToRgb24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// I420 to BGRA32, AVX2 accelerated
void i420ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// I420 to RGBA32, AVX2 accelerated
void i420ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// I420 to BGR24, AVX2 accelerated
void i420ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// I420 to RGB24, AVX2 accelerated
void i420ToRgb24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);
#else

#define nv12ToBgr24_avx2(...) assert(0 && "AVX2 not supported")
#define nv12ToRgb24_avx2(...) assert(0 && "AVX2 not supported")
#define nv12ToBgra32_avx2(...) assert(0 && "AVX2 not supported")
#define nv12ToRgba32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToBgra32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToRgba32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToBgr24_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToRgb24_avx2(...) assert(0 && "AVX2 not supported")

#endif

} // namespace ccap

#endif // CCAP_CONVERT_AVX2_H