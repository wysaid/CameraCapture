/**
 * @file ccap_convert_apple.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#pragma once

#if !defined(CCAP_CONVERT_APPLE_H) && defined(__APPLE__)
#define CCAP_CONVERT_APPLE_H

#include "ccap_convert.h"

#include <cstdint>

namespace ccap {

template <int inputChannels, int outputChannels, bool swapRB>
void colorShuffle_apple(const uint8_t* src, int srcStride,
                        uint8_t* dst, int dstStride,
                        int width, int height);

/// @brief Vertical flip the image. Never pass negative height, it's nonsense here.
void verticalFlip_apple(const uint8_t* src, int srcStride,
                        uint8_t* dst, int dstStride,
                        int height);

/**
 * @brief NV12 to BGRA8888.
 */
void nv12ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcUV, int srcUVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag);

void nv12ToRgba32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcUV, int srcUVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag);

void nv12ToBgr24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

void nv12ToRgb24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

/**
 * @brief I420 to BGRA8888.
 */
void i420ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcU, int srcUStride,
                        const uint8_t* srcV, int srcVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag);

void i420ToRgba32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcU, int srcUStride,
                        const uint8_t* srcV, int srcVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag);

void i420ToBgr24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

void i420ToRgb24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag);

} // namespace ccap

#endif