/**
 * @file ccap_convert_apple.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#pragma once

#if !defined(CCAP_CONVERT_APPLE_H) && defined(__APPLE__)
#define CCAP_CONVERT_APPLE_H

#include <cstdint>

/// apple 的 vImage 不支持传入负的 stride 或者 height 进行上下翻转,
/// 所以参数定义为 uint32_t

namespace ccap
{
template <int inputChannels, int outputChannels, bool swapRB>
void colorShuffle_apple(const uint8_t* src, uint32_t srcStride,
                        uint8_t* dst, uint32_t dstStride,
                        uint32_t width, uint32_t height);

void verticalFlip_apple(const uint8_t* src, uint32_t srcStride,
                        uint8_t* dst, uint32_t dstStride,
                        uint32_t width, uint32_t height);

/**
 * @brief NV12 to BGRA8888.
 *
 * @tparam isFullRange true 表示 Full Range, false 表示 Video Range
 * @tparam isBT601 true 表示 BT.601, false 表示 BT.709
 */
template <bool isFullRange, bool isBT601>
void nv12ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcUV, int srcUVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height);

/**
 * @brief I420 to BGRA8888.
 *
 * @tparam isFullRange true 表示 Full Range, false 表示 Video Range
 * @tparam isBT601 true 表示 BT.601, false 表示 BT.709
 */
template <bool isFullRange, bool isBT601>
void i420ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcU, int srcUStride,
                        const uint8_t* srcV, int srcVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height);

} // namespace ccap

#endif