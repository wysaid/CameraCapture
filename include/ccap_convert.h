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
/// @brief YUV 601 video-range to RGB
inline void yuv2rgb601v(int y, int u, int v, int& r, int& g, int& b)
{
    r = (298 * y + 409 * v + 128) >> 8;
    g = (298 * y - 100 * u - 208 * v + 128) >> 8;
    b = (298 * y + 516 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

// Check if AVX2 is available. If available, use AVX2 acceleration.
bool hasAVX2();

void rgbShuffle(const uint8_t* src, int srcStride,
                uint8_t* dst, int dstStride,
                int width, int height,
                const uint8_t shuffle[3]);

void rgbaShuffle(const uint8_t* src, int srcStride,
                 uint8_t* dst, int dstStride,
                 int width, int height,
                 const uint8_t shuffle[4]);

// swap R and B, G not change, remove A
void rgbaToBgr(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height);
constexpr auto bgraToRgb = rgbaToBgr; // function alias

/// remove last channel
void rgbaToRgb(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height);
constexpr auto bgra2bgr = rgbaToRgb;

/// swap R and B, then add A(0xff)
void rgbToBgra(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height);
constexpr auto bgrToRgba = rgbToBgra;

/// just add A(0xff)
void rgbToRgba(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height);

constexpr auto bgrToBgra = rgbToRgba;

void nv12ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcUV, int srcUVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height);

void nv12ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcUV, int srcUVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height);

void i420ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcU, int srcUStride,
                 const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height);

void i420ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcU, int srcUStride,
                  const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height);

} // namespace ccap

#endif // CCAP_CONVERT_H