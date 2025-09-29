/**
 * @file ccap_convert.h
 * @author wysaid (this@wysaid.org)
 * @brief pixel convert functions for ccap.
 * @date 2025-05
 *
 * @note For C language, use ccap_convert_c.h instead of this header.
 *
 */

#ifndef __cplusplus
#error "ccap_convert.h is for C++ only. For C language, please use ccap_convert_c.h instead."
#endif

#pragma once
#ifndef CCAP_CONVERT_H
#define CCAP_CONVERT_H

#include "ccap_def.h"
#include <algorithm>
#include <cstdint>
#include <memory>

/**
 * @note All pixel conversion functions support writing with vertical flip when height is less than 0.
 * Since SIMD acceleration may be used, the caller must ensure that both src and dst are 32-byte aligned.
 *
 */

namespace ccap {
// Check if AVX2 is available. If available, use AVX2 acceleration.
CCAP_EXPORT bool hasAVX2(); // Check if AVX2 is supported by the CPU (hardware related)

CCAP_EXPORT bool canUseAVX2(); // Check if AVX2 is enabled, useful for testing

/**
 * @brief Enable or disable AVX2 implementation.
 * @param enable true to enable AVX2, false to disable.
 * @return true if AVX2 is available and enabled, false otherwise.
 */
CCAP_EXPORT bool enableAVX2(bool enable); // Disable AVX2 implementation, useful for testing

/// Check if Apple Accelerate is available. If available, use Apple Accelerate acceleration.
CCAP_EXPORT bool hasAppleAccelerate();
/// Check if Apple Accelerate is enabled, useful for testing
CCAP_EXPORT bool canUseAppleAccelerate();
/**
 * @brief Enable or disable Apple Accelerate implementation.
 *
 * @param enable true to enable Apple Accelerate, false to disable.
 * @return true if Apple Accelerate is available and enabled, false otherwise.
 */
CCAP_EXPORT bool enableAppleAccelerate(bool enable);

/// Check if NEON is available. If available, use NEON acceleration.
CCAP_EXPORT bool hasNEON();
/// Check if NEON is enabled, useful for testing
CCAP_EXPORT bool canUseNEON();
/**
 * @brief Enable or disable NEON implementation.
 *
 * @param enable true to enable NEON, false to disable.
 * @return true if NEON is available and enabled, false otherwise.
 */
CCAP_EXPORT bool enableNEON(bool enable);

enum class ConvertBackend : uint32_t {
    AUTO,            ///< Automatically choose the best available backend
    CPU,             ///< CPU implementation
    AVX2,            ///< AVX2 implementation
    AppleAccelerate, ///< Apple Accelerate implementation
    NEON,            ///< NEON implementation
};

/**
 * @brief Check the current conversion backend that will be used.
 *  If Apple Accelerate is available and enabled, returns AppleAccelerate.
 *  If AVX2 is available and enabled, returns AVX2.
 *  If NEON is available and enabled, returns NEON.
 *  Otherwise returns CPU.
 *
 * @return ConvertBackend
 */
ConvertBackend getConvertBackend();

/**
 * @brief Set the Convert Backend.
 *
 * @param backend
 * @return true if the backend was set successfully.
 * @return false if the backend is not supported or the operation failed.
 * Note: When setting ConvertBackend::AVX2, Apple Accelerate will be automatically disabled.
 * Note: When setting ConvertBackend::NEON, Apple Accelerate and AVX2 will be automatically disabled.
 */
bool setConvertBackend(ConvertBackend backend);

/// @brief YUV 601 video-range to RGB (includes video range preprocessing)
inline void yuv2rgb601v(int y, int u, int v, int& r, int& g, int& b) {
    y = y - 16;  // video range Y preprocessing
    u = u - 128; // center U
    v = v - 128; // center V

    r = (298 * y + 409 * v + 128) >> 8;
    g = (298 * y - 100 * u - 208 * v + 128) >> 8;
    b = (298 * y + 516 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

/// @brief YUV 709 video-range to RGB (includes video range preprocessing)
inline void yuv2rgb709v(int y, int u, int v, int& r, int& g, int& b) {
    y = y - 16;  // video range Y preprocessing
    u = u - 128; // center U
    v = v - 128; // center V

    r = (298 * y + 459 * v + 128) >> 8;
    g = (298 * y - 55 * u - 136 * v + 128) >> 8;
    b = (298 * y + 541 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

/// @brief YUV 601 full-range to RGB (includes full range preprocessing)
inline void yuv2rgb601f(int y, int u, int v, int& r, int& g, int& b) {
    // full range: Y does not need to subtract 16
    u = u - 128; // center U
    v = v - 128; // center V

    r = (256 * y + 351 * v + 128) >> 8;
    g = (256 * y - 86 * u - 179 * v + 128) >> 8;
    b = (256 * y + 443 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

/// @brief YUV 709 full-range to RGB (includes full range preprocessing)
inline void yuv2rgb709f(int y, int u, int v, int& r, int& g, int& b) {
    // full range: Y does not need to subtract 16
    u = u - 128; // center U
    v = v - 128; // center V

    r = (256 * y + 403 * v + 128) >> 8;
    g = (256 * y - 48 * u - 120 * v + 128) >> 8;
    b = (256 * y + 475 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

enum class ConvertFlag {
    BT601 = 0x1,                  ///< Use BT.601 color space
    BT709 = 0x2,                  ///< Use BT.709 color space
    FullRange = 0x10,             ///< Use full range color space
    VideoRange = 0x20,            ///< Use video range color space
    Default = BT601 | VideoRange, ///< Default conversion: BT.601 full range
};

inline bool operator&(ConvertFlag lhs, ConvertFlag rhs) { return static_cast<bool>(static_cast<int>(lhs) & static_cast<int>(rhs)); }

inline ConvertFlag operator|(ConvertFlag lhs, ConvertFlag rhs) {
    return static_cast<ConvertFlag>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

// 定义YUV到RGB转换函数指针类型
typedef void (*YuvToRgbFunc)(int y, int u, int v, int& r, int& g, int& b);

inline YuvToRgbFunc getYuvToRgbFunc(bool is601, bool isFullRange) {
    if (is601) {
        if (isFullRange)
            return yuv2rgb601f;
        else
            return yuv2rgb601v;
    } else {
        if (isFullRange)
            return yuv2rgb709f;
        else
            return yuv2rgb709v;
    }
}

///////////// color shuffle /////////////

// swapRB indicates whether to swap Red and Blue channels
template <int inputChannels, int outputChannels, int swapRB>
void colorShuffle(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

inline void rgbaToBgra(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height) {
    colorShuffle<4, 4, true>(src, srcStride, dst, dstStride, width, height);
}

constexpr auto bgraToRgba = rgbaToBgra; // function alias

// swap R and B, G not change, remove A
inline void rgbaToBgr(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height) {
    colorShuffle<4, 3, true>(src, srcStride, dst, dstStride, width, height);
}

constexpr auto bgraToRgb = rgbaToBgr;

/// remove last channel
inline void rgbaToRgb(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height) {
    colorShuffle<4, 3, false>(src, srcStride, dst, dstStride, width, height);
}

constexpr auto bgra2bgr = rgbaToRgb;

/// swap R and B, then add A(0xff)
inline void rgbToBgra(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height) {
    colorShuffle<3, 4, true>(src, srcStride, dst, dstStride, width, height);
}

constexpr auto bgrToRgba = rgbToBgra;

/// just add A(0xff)
inline void rgbToRgba(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height) {
    colorShuffle<3, 4, false>(src, srcStride, dst, dstStride, width, height);
}

constexpr auto bgrToBgra = rgbToRgba;

inline void rgbToBgr(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height) {
    colorShuffle<3, 3, true>(src, srcStride, dst, dstStride, width, height);
}

constexpr auto bgrToRgb = rgbToBgr;

//////////// yuv color to rgb color /////////////

CCAP_EXPORT void nv12ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcUV, int srcUVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void nv12ToRgb24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcUV, int srcUVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void nv12ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcUV, int srcUVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void nv12ToRgba32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcUV, int srcUVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void i420ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcU, int srcUStride,
                 const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void i420ToRgb24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcU, int srcUStride,
                 const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void i420ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcU, int srcUStride,
                  const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void i420ToRgba32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcU, int srcUStride,
                  const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

// YUYV (YUV 4:2:2 packed) conversion functions
CCAP_EXPORT void yuyvToBgr24(const uint8_t* src, int srcStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void yuyvToRgb24(const uint8_t* src, int srcStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void yuyvToBgra32(const uint8_t* src, int srcStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void yuyvToRgba32(const uint8_t* src, int srcStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

// UYVY (YUV 4:2:2 packed) conversion functions
CCAP_EXPORT void uyvyToBgr24(const uint8_t* src, int srcStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void uyvyToRgb24(const uint8_t* src, int srcStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void uyvyToBgra32(const uint8_t* src, int srcStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

CCAP_EXPORT void uyvyToRgba32(const uint8_t* src, int srcStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

class Allocator;
/// @brief Used to store some intermediate results, avoiding repeated memory allocation.
/// If no shared memory allocator is set externally, use the default allocator.
/// @return A shared pointer to the current shared memory allocator. (Will not be nullptr)
CCAP_EXPORT std::shared_ptr<ccap::Allocator> getSharedAllocator();
/// @brief Release the shared memory allocator.
CCAP_EXPORT void resetSharedAllocator();
} // namespace ccap

#endif // CCAP_CONVERT_H