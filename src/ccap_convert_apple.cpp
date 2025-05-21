/**
 * @file ccap_convert_apple.cpp
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#if __APPLE__

#include "ccap_convert_apple.h"

#include "ccap_utils.h"

#include <Accelerate/Accelerate.h>

namespace ccap
{
template <int inputChannels, int outputChannels, bool swapRB>
void colorShuffle_apple(const uint8_t* src, uint32_t srcStride,
                        uint8_t* dst, uint32_t dstStride,
                        uint32_t width, uint32_t height)
{
    static_assert((inputChannels == 3 || inputChannels == 4) &&
                      (outputChannels == 3 || outputChannels == 4),
                  "inputChannels and outputChannels must be 3 or 4");

    vImage_Buffer srcBuffer = { (void*)src, height, width, srcStride };
    vImage_Buffer dstBuffer = { dst, height, width, dstStride };

    if constexpr (inputChannels == outputChannels)
    { /// Conversion with the same number of channels, only RGB <-> BGR, RGBA <-> BGRA, swapRB must be true.

        static_assert(swapRB, "swapRB must be true when inputChannels == outputChannels");

        // RGBA8888 <-> BGRA8888: Channel reordering required
        // vImagePermuteChannels_ARGB8888 needs 4 indices to implement arbitrary channel arrangement
        // RGBA8888: [R, G, B, A], BGRA8888: [B, G, R, A]
        constexpr uint8_t permuteMap[4] = { 2, 1, 0, 3 };

        if constexpr (inputChannels == 4) // RGBA -> BGRA
        {
            vImagePermuteChannels_ARGB8888(&srcBuffer, &dstBuffer, permuteMap, kvImageNoFlags);
        }
        else
        {
            vImagePermuteChannels_RGB888(&srcBuffer, &dstBuffer, permuteMap, kvImageNoFlags);
        }
    }
    else // Different number of channels, only 4 channels <-> 3 channels
    {
        if constexpr (inputChannels == 4)
        { // 4 channels -> 3 channels
            if constexpr (swapRB)
            { // Possible cases: RGBA->BGR, BGRA->RGB

                vImageConvert_RGBA8888toBGR888(&srcBuffer, &dstBuffer, kvImageNoFlags);
            }
            else
            { // Possible cases: RGBA->RGB, BGRA->BGR
                vImageConvert_RGBA8888toRGB888(&srcBuffer, &dstBuffer, kvImageNoFlags);
            }
        }
        else
        { /// 3 channels -> 4 channels
            if constexpr (swapRB)
            { // Possible cases: BGR->RGBA, RGB->BGRA
                vImageConvert_RGB888toBGRA8888(&srcBuffer, nullptr, 0xff, &dstBuffer, false, kvImageNoFlags);
            }
            else
            { // Possible cases: BGR->BGRA, RGB->RGBA
                vImageConvert_RGB888toRGBA8888(&srcBuffer, nullptr, 0xff, &dstBuffer, false, kvImageNoFlags);
            }
        }
    }
}

void verticalFlip_apple(const uint8_t* src, uint32_t srcStride,
                        uint8_t* dst, uint32_t dstStride,
                        uint32_t width, uint32_t height)
{
    vImage_Buffer srcBuffer = { (void*)src, height, width, srcStride };
    vImage_Buffer dstBuffer = { dst, height, width, dstStride };
    vImageVerticalReflect_Planar8(&srcBuffer, &dstBuffer, kvImageNoFlags);
}

////////////////// NV12 to BGRA8888 //////////////////////

void nv12ToBgra32_apple_imp(const uint8_t* srcY, int srcYStride,
                            const uint8_t* srcUV, int srcUVStride,
                            uint8_t* dst, int dstStride,
                            int width, int height,
                            const vImage_YpCbCrToARGBMatrix* matrix,
                            const vImage_YpCbCrPixelRange* range)
{
    // 构造 vImage_Buffer
    vImage_Buffer yBuffer = { (void*)srcY, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)srcYStride };
    vImage_Buffer uvBuffer = { (void*)srcUV, (vImagePixelCount)(height / 2), (vImagePixelCount)(width / 2), (size_t)srcUVStride };
    vImage_Buffer dstBuffer = { dst, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)dstStride };

    // 生成转换描述
    vImage_YpCbCrToARGB info;
    vImage_Error err = vImageConvert_YpCbCrToARGB_GenerateConversion(
        matrix,
        range,
        &info,
        kvImage420Yp8_CbCr8, // NV12 格式
        kvImageARGB8888,     // 输出格式
        kvImageNoFlags);
    if (err != kvImageNoError)
    {
        CCAP_LOG_E("vImageConvert_YpCbCrToARGB_GenerateConversion failed: %zu", err);
        return;
    }

    // 执行 NV12 到 BGRA8888 的转换
    err = vImageConvert_420Yp8_CbCr8ToARGB8888(
        &yBuffer,
        &uvBuffer,
        &dstBuffer,
        &info,
        nullptr, // 没有预乘 alpha
        255,     // alpha 通道全不透明
        kvImageNoFlags);

    if (err != kvImageNoError)
    {
        CCAP_LOG_E("vImageConvert_420Yp8_CbCr8ToARGB8888 failed: %zu", err);
        return;
    }
}

template <bool isFullRange, bool isBT601>
void nv12ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcUV, int srcUVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height)
{
    // 构造 vImage_Buffer
    vImage_Buffer yBuffer = { (void*)srcY, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)srcYStride };
    vImage_Buffer uvBuffer = { (void*)srcUV, (vImagePixelCount)(height / 2), (vImagePixelCount)(width / 2), (size_t)srcUVStride };
    vImage_Buffer dstBuffer = { dst, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)dstStride };

    // @refitem <https://developer.apple.com/documentation/accelerate/vimage_ypcbcrpixelrange?language=objc>

    // Video Range
    constexpr vImage_YpCbCrPixelRange videoRange = { 16,   // Yp_bias
                                                     128,  // CbCr_bias
                                                     265,  // YpRangeMax
                                                     240,  // CbCrRangeMax
                                                     235,  // YpMax
                                                     16,   // YpMin
                                                     240,  // CbCrMax
                                                     16 }; // CbCrMin

    // Full Range
    constexpr vImage_YpCbCrPixelRange fullRange = { 0,   // Yp_bias
                                                    128, // CbCr_bias
                                                    255, // YpRangeMax
                                                    255, // CbCrRangeMax
                                                    255, // YpMax
                                                    1,   // YpMin
                                                    255, // CbCrMax
                                                    0 }; // CbCrMin

    const auto* range = isFullRange ? &fullRange : &videoRange;

    // 选择色彩空间矩阵（通常 NV12 是 BT.601，视频范围）
    const vImage_YpCbCrToARGBMatrix* matrix = isBT601 ? kvImage_YpCbCrToARGBMatrix_ITU_R_601_4 :
                                                        kvImage_YpCbCrToARGBMatrix_ITU_R_709_2;
    nv12ToBgra32_apple_imp(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, matrix, range);
}

template void nv12ToBgra32_apple<true, true>(const uint8_t* srcY, int srcYStride,
                                             const uint8_t* srcUV, int srcUVStride,
                                             uint8_t* dst, int dstStride,
                                             int width, int height);

template void nv12ToBgra32_apple<true, false>(const uint8_t* srcY, int srcYStride,
                                              const uint8_t* srcUV, int srcUVStride,
                                              uint8_t* dst, int dstStride,
                                              int width, int height);

template void nv12ToBgra32_apple<false, true>(const uint8_t* srcY, int srcYStride,
                                              const uint8_t* srcUV, int srcUVStride,
                                              uint8_t* dst, int dstStride,
                                              int width, int height);

template void nv12ToBgra32_apple<false, false>(const uint8_t* srcY, int srcYStride,
                                               const uint8_t* srcUV, int srcUVStride,
                                               uint8_t* dst, int dstStride,
                                               int width, int height);

//////////////////////  I420 to BGRA8888 //////////////////////

void i420ToBgra32_apple_imp(const uint8_t* srcY, int srcYStride,
                            const uint8_t* srcU, int srcUStride,
                            const uint8_t* srcV, int srcVStride,
                            uint8_t* dst, int dstStride,
                            int width, int height,
                            const vImage_YpCbCrToARGBMatrix* matrix,
                            const vImage_YpCbCrPixelRange* range)
{
    vImage_Buffer yBuffer = { (void*)srcY, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)srcYStride };
    vImage_Buffer uBuffer = { (void*)srcU, (vImagePixelCount)(height / 2), (vImagePixelCount)(width / 2), (size_t)srcUStride };
    vImage_Buffer vBuffer = { (void*)srcV, (vImagePixelCount)(height / 2), (vImagePixelCount)(width / 2), (size_t)srcVStride };
    vImage_Buffer dstBuffer = { dst, (vImagePixelCount)height, (vImagePixelCount)width, (size_t)dstStride };

    vImage_YpCbCrToARGB info;
    vImage_Error err = vImageConvert_YpCbCrToARGB_GenerateConversion(
        matrix,
        range,
        &info,
        kvImage420Yp8_Cb8_Cr8, // I420 格式
        kvImageARGB8888,       // 输出格式
        kvImageNoFlags);
    if (err != kvImageNoError)
    {
        CCAP_LOG_E("vImageConvert_YpCbCrToARGB_GenerateConversion failed: %zu", err);
        return;
    }

    err = vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(
        &yBuffer,
        &uBuffer,
        &vBuffer,
        &dstBuffer,
        &info,
        nullptr,
        255,
        kvImageNoFlags);

    if (err != kvImageNoError)
    {
        CCAP_LOG_E("vImageConvert_420Yp8_Cb8_Cr8ToARGB8888 failed: %zu", err);
        return;
    }
}

template <bool isFullRange, bool isBT601>
void i420ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcU, int srcUStride,
                        const uint8_t* srcV, int srcVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height)
{
    // Video Range
    constexpr vImage_YpCbCrPixelRange videoRange = { 16, 128, 265, 240, 235, 16, 240, 16 };
    // Full Range
    constexpr vImage_YpCbCrPixelRange fullRange = { 0, 128, 255, 255, 255, 1, 255, 0 };

    const auto* range = isFullRange ? &fullRange : &videoRange;
    const vImage_YpCbCrToARGBMatrix* matrix = isBT601 ? kvImage_YpCbCrToARGBMatrix_ITU_R_601_4 : kvImage_YpCbCrToARGBMatrix_ITU_R_709_2;

    i420ToBgra32_apple_imp(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride,
                           dst, dstStride, width, height, matrix, range);
}

// 显式实例化
template void i420ToBgra32_apple<true, true>(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int);
template void i420ToBgra32_apple<true, false>(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int);
template void i420ToBgra32_apple<false, true>(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int);
template void i420ToBgra32_apple<false, false>(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int);

} // namespace ccap

#endif