/**
 * @file ccap_convert.h
 * @author wysaid (this@wysaid.org)
 * @brief pixel convert functions for ccap.
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_CONVERT_H
#define CCAP_CONVERT_H

#include <algorithm>
#include <cstdint>

/**
 * @note All pixel conversion functions support writing with vertical flip when height is less than 0.
 * Since SIMD acceleration may be used, the caller must ensure that both src and dst are 32-byte aligned.
 *
 */

namespace ccap
{
/// @brief YUV 601 video-range to RGB (包含 video range 预处理)
inline void yuv2rgb601v(int y, int u, int v, int& r, int& g, int& b)
{
    y = y - 16;  // video range Y 预处理
    u = u - 128; // 中心化 U
    v = v - 128; // 中心化 V

    r = (298 * y + 409 * v + 128) >> 8;
    g = (298 * y - 100 * u - 208 * v + 128) >> 8;
    b = (298 * y + 516 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

/// @brief YUV 709 video-range to RGB (包含 video range 预处理)
inline void yuv2rgb709v(int y, int u, int v, int& r, int& g, int& b)
{
    y = y - 16;  // video range Y 预处理
    u = u - 128; // 中心化 U
    v = v - 128; // 中心化 V

    r = (298 * y + 459 * v + 128) >> 8;
    g = (298 * y - 55 * u - 136 * v + 128) >> 8;
    b = (298 * y + 541 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

/// @brief YUV 601 full-range to RGB (包含 full range 预处理)
inline void yuv2rgb601f(int y, int u, int v, int& r, int& g, int& b)
{
    // full range: Y 不需要减 16
    u = u - 128; // 中心化 U
    v = v - 128; // 中心化 V

    r = (256 * y + 351 * v + 128) >> 8;
    g = (256 * y - 86 * u - 179 * v + 128) >> 8;
    b = (256 * y + 443 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

/// @brief YUV 709 full-range to RGB (包含 full range 预处理)
inline void yuv2rgb709f(int y, int u, int v, int& r, int& g, int& b)
{
    // full range: Y 不需要减 16
    u = u - 128; // 中心化 U
    v = v - 128; // 中心化 V

    r = (256 * y + 403 * v + 128) >> 8;
    g = (256 * y - 48 * u - 120 * v + 128) >> 8;
    b = (256 * y + 475 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

enum class ConvertFlag
{
    BT601 = 0x1,                  ///< Use BT.601 color space
    BT709 = 0x2,                  ///< Use BT.709 color space
    FullRange = 0x10,             ///< Use full range color space
    VideoRange = 0x20,            ///< Use video range color space
    Default = BT601 | VideoRange, ///< Default conversion: BT.601 full range
};

inline bool operator&(ConvertFlag lhs, ConvertFlag rhs)
{
    return static_cast<bool>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline ConvertFlag operator|(ConvertFlag lhs, ConvertFlag rhs)
{
    return static_cast<ConvertFlag>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

// Check if AVX2 is available. If available, use AVX2 acceleration.
bool hasAVX2();

///////////// color shuffle /////////////

template <int inputChannels, int outputChannels>
void colorShuffle(const uint8_t* src, int srcStride,
                  uint8_t* dst, int dstStride,
                  int width, int height,
                  const uint8_t shuffle[]);

/// @brief A general 4-channel shuffle
inline void colorShuffle4To4(const uint8_t* src, int srcStride,
                             uint8_t* dst, int dstStride,
                             int width, int height,
                             const uint8_t shuffle[4])
{
    colorShuffle<4, 4>(src, srcStride, dst, dstStride, width, height, shuffle);
}

/// @brief A general channel shuffle, input is 3 channels, output is 4 channels
inline void colorShuffle3To4(const uint8_t* src, int srcStride,
                             uint8_t* dst, int dstStride,
                             int width, int height,
                             const uint8_t shuffle[4])
{
    colorShuffle<3, 4>(src, srcStride, dst, dstStride, width, height, shuffle);
}

/// @brief A general channel shuffle, input is 4 channels, output is 3 channels
inline void colorShuffle4To3(const uint8_t* src, int srcStride,
                             uint8_t* dst, int dstStride,
                             int width, int height,
                             const uint8_t shuffle[3])
{
    colorShuffle<4, 3>(src, srcStride, dst, dstStride, width, height, shuffle);
}

/// @brief A general channel shuffle, input is 3 channels, output is 3 channels
inline void colorShuffle3To3(const uint8_t* src, int srcStride,
                             uint8_t* dst, int dstStride,
                             int width, int height,
                             const uint8_t shuffle[3])
{
    colorShuffle<3, 3>(src, srcStride, dst, dstStride, width, height, shuffle);
}

constexpr auto rgbShuffle = colorShuffle3To3;
constexpr auto rgbaShuffle = colorShuffle4To4;

// swap R and B, G not change, remove A
inline void rgbaToBgr(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    constexpr uint8_t kShuffleMap[4] = { 2, 1, 0, 3 }; // RGBA -> BGR
    colorShuffle<4, 3>(src, srcStride, dst, dstStride, width, height, kShuffleMap);
}

constexpr auto bgraToRgb = rgbaToBgr; // function alias

/// remove last channel
inline void rgbaToRgb(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    constexpr uint8_t kShuffleMap[4] = { 0, 1, 2, 3 }; // RGBA -> RGB
    colorShuffle<4, 3>(src, srcStride, dst, dstStride, width, height, kShuffleMap);
}

constexpr auto bgra2bgr = rgbaToRgb;

/// swap R and B, then add A(0xff)
inline void rgbToBgra(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    constexpr uint8_t kShuffleMap[4] = { 2, 1, 0, 3 }; // RGB -> BGRA
    colorShuffle<3, 4>(src, srcStride, dst, dstStride, width, height, kShuffleMap);
}

constexpr auto bgrToRgba = rgbToBgra;

/// just add A(0xff)
inline void rgbToRgba(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    constexpr uint8_t kShuffleMap[4] = { 0, 1, 2, 3 }; // RGB -> RGBA
    colorShuffle<3, 4>(src, srcStride, dst, dstStride, width, height, kShuffleMap);
}

constexpr auto bgrToBgra = rgbToRgba;

//////////// yuv color to rgb color /////////////

void nv12ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcUV, int srcUVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

void nv12ToRgb24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcUV, int srcUVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

void nv12ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcUV, int srcUVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

void nv12ToRgba32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcUV, int srcUVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

void i420ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcU, int srcUStride,
                 const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

void i420ToRgb24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcU, int srcUStride,
                 const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height, ConvertFlag flag = ConvertFlag::Default);

void i420ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcU, int srcUStride,
                  const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

void i420ToRgba32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcU, int srcUStride,
                  const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height, ConvertFlag flag = ConvertFlag::Default);

} // namespace ccap

#endif // CCAP_CONVERT_H