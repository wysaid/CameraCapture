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

#include <cstdint>

// pixel convert 里面的函数全都支持 height 小于0的时候, 进行上下翻转写入.
// 由于可能使用 AVX2 加速, 所以使用者需要保证 src 和 dst 的内存对齐.
// 这里的对齐要求是 32 字节对齐, 也就是 AVX2 的要求.

namespace ccap
{
// 检查一下 AVX2 是否可用, 可用的情况下， 使用 AVX2 加速.
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
void rgba2bgr(const uint8_t* src, int srcStride,
              uint8_t* dst, int dstStride,
              int width, int height);
constexpr auto bgra2rgb = rgba2bgr; // function alias

/// remove last channel
void rgba2rgb(const uint8_t* src, int srcStride,
              uint8_t* dst, int dstStride,
              int width, int height);
constexpr auto bgra2bgr = rgba2rgb;

/// swap R and B, then add A(0xff)
void rgb2bgra(const uint8_t* src, int srcStride,
              uint8_t* dst, int dstStride,
              int width, int height);
constexpr auto bgr2rgba = rgb2bgra;

/// just add A(0xff)
void rgb2rgba(const uint8_t* src, int srcStride,
              uint8_t* dst, int dstStride,
              int width, int height);

void nv12ToBGR24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcUV, int srcUVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height);

void nv12ToBGRA32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcUV, int srcUVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height);

void i420ToBGR24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcU, int srcUStride,
                 const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height);

void i420ToBGRA32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcU, int srcUStride,
                  const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height);

} // namespace ccap

#endif // CCAP_CONVERT_H