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