/**
 * @file ccap_convert_neon.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_CONVERT_NEON_H
#define CCAP_CONVERT_NEON_H

#include <cstdint>

// Forward declare ConvertFlag
enum class ConvertFlag;

// NEON support detection for ARM64 platforms
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

namespace ccap
{
bool hasNEON();

#if ENABLE_NEON_IMP

template <int inputChannels, int outputChannels, int swapRB>
void colorShuffle_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 to BGRA32, NEON accelerated
void nv12ToBgra32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// NV12 to RGBA32, NEON accelerated
void nv12ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// NV12 to BGR24, NEON accelerated
void nv12ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// NV12 to RGB24, NEON accelerated
void nv12ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// I420 to BGRA32, NEON accelerated
void i420ToBgra32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// I420 to RGBA32, NEON accelerated
void i420ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// I420 to BGR24, NEON accelerated
void i420ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// I420 to RGB24, NEON accelerated
void i420ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// YUYV to BGR24, NEON accelerated
void yuyvToBgr24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// YUYV to RGB24, NEON accelerated
void yuyvToRgb24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// YUYV to BGRA32, NEON accelerated
void yuyvToBgra32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// YUYV to RGBA32, NEON accelerated
void yuyvToRgba32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// UYVY to BGR24, NEON accelerated
void uyvyToBgr24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// UYVY to RGB24, NEON accelerated
void uyvyToRgb24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag);

// UYVY to BGRA32, NEON accelerated
void uyvyToBgra32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

// UYVY to RGBA32, NEON accelerated
void uyvyToRgba32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

#else

#define nv12ToBgr24_neon(...) assert(0 && "NEON not supported")
#define nv12ToRgb24_neon(...) assert(0 && "NEON not supported")
#define nv12ToBgra32_neon(...) assert(0 && "NEON not supported")
#define nv12ToRgba32_neon(...) assert(0 && "NEON not supported")
#define i420ToBgra32_neon(...) assert(0 && "NEON not supported")
#define i420ToRgba32_neon(...) assert(0 && "NEON not supported")
#define i420ToBgr24_neon(...) assert(0 && "NEON not supported")
#define i420ToRgb24_neon(...) assert(0 && "NEON not supported")
#define yuyvToBgr24_neon(...) assert(0 && "NEON not supported")
#define yuyvToRgb24_neon(...) assert(0 && "NEON not supported")
#define yuyvToBgra32_neon(...) assert(0 && "NEON not supported")
#define yuyvToRgba32_neon(...) assert(0 && "NEON not supported")
#define uyvyToBgr24_neon(...) assert(0 && "NEON not supported")
#define uyvyToRgb24_neon(...) assert(0 && "NEON not supported")
#define uyvyToBgra32_neon(...) assert(0 && "NEON not supported")
#define uyvyToRgba32_neon(...) assert(0 && "NEON not supported")

#endif

} // namespace ccap

#endif // CCAP_CONVERT_NEON_H
