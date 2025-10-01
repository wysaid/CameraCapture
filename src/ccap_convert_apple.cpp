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
#include <cassert>

namespace ccap {
template <int inputChannels, int outputChannels, bool swapRB>
void colorShuffle_apple(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height) {
    static_assert((inputChannels == 3 || inputChannels == 4) && (outputChannels == 3 || outputChannels == 4),
                  "inputChannels and outputChannels must be 3 or 4");

    assert(width > 0 && height != 0); // width must be positive, height can be negative for vertical flip

    vImage_Buffer dstBuffer;
    std::shared_ptr<ccap::Allocator> sharedAllocator;

    if (height > 0) {
        dstBuffer = { dst, (uint32_t)height, (uint32_t)width, (size_t)dstStride };
    } else {
        height = -height; // Negative height means vertical flip
        sharedAllocator = ccap::getSharedAllocator();
        assert(sharedAllocator != nullptr);
        if (dstStride * height > sharedAllocator->size()) {
            // Resize the shared allocator to accommodate the new size
            sharedAllocator->resize(dstStride * height);
        }
        dstBuffer = { sharedAllocator->data(), (uint32_t)height, (uint32_t)width, (size_t)dstStride };
    }
    vImage_Buffer srcBuffer = { (void*)src, (uint32_t)height, (uint32_t)width, (size_t)srcStride };

    if constexpr (inputChannels == outputChannels) {
        /// Conversion with the same number of channels, only RGB <-> BGR, RGBA <-> BGRA, swapRB must be true.

        static_assert(swapRB, "swapRB must be true when inputChannels == outputChannels");

        // RGBA8888 <-> BGRA8888: Channel reordering required
        // vImagePermuteChannels_ARGB8888 needs 4 indices to implement arbitrary channel arrangement
        // RGBA8888: [R, G, B, A], BGRA8888: [B, G, R, A]
        constexpr uint8_t permuteMap[4] = { 2, 1, 0, 3 };

        if constexpr (inputChannels == 4) { // RGBA -> BGRA
            vImagePermuteChannels_ARGB8888(&srcBuffer, &dstBuffer, permuteMap, kvImageNoFlags);
        } else {
            vImagePermuteChannels_RGB888(&srcBuffer, &dstBuffer, permuteMap, kvImageNoFlags);
        }
    } else { // Different number of channels, only 4 channels <-> 3 channels

        if constexpr (inputChannels == 4) { // 4 channels -> 3 channels
            if constexpr (swapRB) {         // Possible cases: RGBA->BGR, BGRA->RGB
                vImageConvert_RGBA8888toBGR888(&srcBuffer, &dstBuffer, kvImageNoFlags);
            } else { // Possible cases: RGBA->RGB, BGRA->BGR
                vImageConvert_RGBA8888toRGB888(&srcBuffer, &dstBuffer, kvImageNoFlags);
            }
        } else {                    /// 3 channels -> 4 channels
            if constexpr (swapRB) { // Possible cases: BGR->RGBA, RGB->BGRA
                vImageConvert_RGB888toBGRA8888(&srcBuffer, nullptr, 0xff, &dstBuffer, false, kvImageNoFlags);
            } else { // Possible cases: BGR->BGRA, RGB->RGBA
                vImageConvert_RGB888toRGBA8888(&srcBuffer, nullptr, 0xff, &dstBuffer, false, kvImageNoFlags);
            }
        }
    }

    if (sharedAllocator) { // Perform vertical flip and write the final result to dst. Even with two-step operation, it's still much faster than regular CPU operations.
        verticalFlip_apple((const uint8_t*)dstBuffer.data, dstStride, dst, dstStride, height);
    }
}

template void colorShuffle_apple<4, 4, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_apple<4, 3, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_apple<4, 3, false>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_apple<3, 4, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_apple<3, 4, false>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_apple<3, 3, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

void verticalFlip_apple(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int height) {
    assert(src != nullptr && dst != nullptr);
    assert(srcStride > 0 && dstStride > 0 && height > 0); // width and height must be positive
    vImage_Buffer srcBuffer = { (void*)src, (vImagePixelCount)height, (vImagePixelCount)srcStride, (size_t)srcStride };
    vImage_Buffer dstBuffer = { dst, (vImagePixelCount)height, (vImagePixelCount)dstStride, (size_t)dstStride };
    vImageVerticalReflect_Planar8(&srcBuffer, &dstBuffer, kvImageNoFlags);
}

////////////////// NV12 to BGRA8888 //////////////////////

// void checkAlpha(const uint8_t* rgbaSrc, int srcStride, int width, int height, int alphaOffset = 3) {
//     assert(rgbaSrc != nullptr);
//     assert(width > 0 && height > 0); // width and height must be positive
//     for (int y = 0; y < height; ++y) {
//         const uint8_t* row = rgbaSrc + y * srcStride;
//         for (int x = 0; x < width; ++x) {
//             if (row[x * 4 + alphaOffset] != 0xff) { // Check alpha channel
//                 // Print current pixel coordinates and RGB values
//                 auto r = row[x * 4 + 0];
//                 auto g = row[x * 4 + 1];
//                 auto b = row[x * 4 + 2];
//                 auto a = row[x * 4 + 3];
//                 CCAP_LOG_E("Alpha channel (%d) is not 255 at pixel (%d, %d, %d): (%d, %d, %d, %d)", (int)row[x * 4 + alphaOffset], x, y, alphaOffset, r, g, b, a);
//                 assert(false && "Alpha channel is not 255, this is an error in the conversion process.");
//             }
//         }
//     }
// }

void nv12ToArgb32_apple_imp(const uint8_t* srcY, int srcYStride,
                            const uint8_t* srcUV, int srcUVStride,
                            uint8_t* dst, int dstStride,
                            int width, int height,
                            const vImage_YpCbCrToARGBMatrix* matrix,
                            const vImage_YpCbCrPixelRange* range,
                            const uint8_t permuteMap[4]) {
    assert(width > 0 && height != 0); // Internal implementation
    // Construct vImage_Buffer
    vImage_Buffer dstBuffer;
    std::shared_ptr<ccap::Allocator> sharedAllocator = ccap::getSharedAllocator();
    assert(sharedAllocator != nullptr);
    auto realHeight = std::abs(height);

    if (dstStride * realHeight > sharedAllocator->size()) {
        // Resize the shared allocator to accommodate the new size
        sharedAllocator->resize(dstStride * realHeight);
    }
    if (height < 0) {
        dstBuffer = { sharedAllocator->data(), (uint32_t)realHeight, (uint32_t)width, (size_t)dstStride };
    } else {
        dstBuffer = { dst, (uint32_t)realHeight, (uint32_t)width, (size_t)dstStride };
    }

    vImage_Buffer yBuffer = { (void*)srcY, (vImagePixelCount)realHeight, (vImagePixelCount)width, (size_t)srcYStride };
    vImage_Buffer uvBuffer = { (void*)srcUV, (vImagePixelCount)(realHeight / 2), (vImagePixelCount)(width / 2), (size_t)srcUVStride };

    // Generate conversion descriptor
    vImage_YpCbCrToARGB info;
    vImage_Error err = vImageConvert_YpCbCrToARGB_GenerateConversion(matrix, range, &info,
                                                                     kvImage420Yp8_CbCr8, // NV12 format
                                                                     kvImageARGB8888,     // Output format
                                                                     kvImageNoFlags);
    if (err != kvImageNoError) {
        CCAP_LOG_E("vImageConvert_YpCbCrToARGB_GenerateConversion failed: %zu", err);
        return;
    }

    // Execute NV12 to BGRA8888 conversion
    err = vImageConvert_420Yp8_CbCr8ToARGB8888(&yBuffer, &uvBuffer, &dstBuffer, &info,
                                               permuteMap, // No premultiplied alpha
                                               255,        // Alpha channel fully opaque
                                               kvImageNoFlags);

    if (err != kvImageNoError) {
        CCAP_LOG_E("vImageConvert_420Yp8_CbCr8ToARGB8888 failed: %zu", err);
        return;
    }

    if (height < 0) { // Complete flip and pixel reordering, write final result to dst. Even with two-step operation, it's still much faster than regular CPU operations.
        verticalFlip_apple((const uint8_t*)dstBuffer.data, dstStride, dst, dstStride, realHeight);
    }
}

void nv12ToRgbColor_apple(const uint8_t* srcY, int srcYStride,
                          const uint8_t* srcUV, int srcUVStride,
                          uint8_t* dst, int dstStride,
                          int width, int height,
                          ConvertFlag flag, ccap::PixelFormat targetPixelFormat) {
    // @refitem <https://developer.apple.com/documentation/accelerate/vimage_ypcbcrpixelrange?language=objc>

    // Video Range (unclamped)
    constexpr vImage_YpCbCrPixelRange videoRange = { 16,  // Yp_bias
                                                     128, // CbCr_bias
                                                     235, // YpRangeMax
                                                     240, // CbCrRangeMax
                                                     255, // YpMax
                                                     0,   // YpMin
                                                     255, // CbCrMax
                                                     1 }; // CbCrMin

    // Full Range
    constexpr vImage_YpCbCrPixelRange fullRange = { 0,   // Yp_bias
                                                    128, // CbCr_bias
                                                    255, // YpRangeMax
                                                    255, // CbCrRangeMax
                                                    255, // YpMax
                                                    1,   // YpMin
                                                    255, // CbCrMax
                                                    0 }; // CbCrMin

    const auto* range = (flag & ConvertFlag::FullRange) ? &fullRange : &videoRange;

    // Select color space matrix (typically NV12 uses BT.601, video range)
    const vImage_YpCbCrToARGBMatrix* matrix = (flag & ConvertFlag::BT601) ? kvImage_YpCbCrToARGBMatrix_ITU_R_601_4 : kvImage_YpCbCrToARGBMatrix_ITU_R_709_2;

    auto allocator = ccap::getSharedAllocator();
    auto realHeight = std::abs(height);
    bool hasAlpha = ccap::pixelFormatInclude(targetPixelFormat, ccap::kPixelFormatAlphaColorBit);
    auto argbStride = (4 * width + 31) & ~31; // 32-byte alignment

    if (!hasAlpha && allocator->size() < argbStride * realHeight) {
        allocator->resize(argbStride * realHeight); // Ensure allocator has sufficient space
    }

    uint8_t* argbData = hasAlpha ? dst : allocator->data(); // If no alpha channel, use shared allocator data

    uint8_t permuteMap[4]; /// Convert from ARGB to other formats.
    if (ccap::pixelFormatInclude(targetPixelFormat, ccap::kPixelFormatBGRBit)) {
        // BGR or BGRA
        permuteMap[0] = 3; // B
        permuteMap[1] = 2; // G
        permuteMap[2] = 1; // R
        permuteMap[3] = 0; // A (if exists)
    } else {
        // RGB or RGBA
        permuteMap[0] = 1; // R
        permuteMap[1] = 2; // G
        permuteMap[2] = 3; // B
        permuteMap[3] = 0; // A (if exists)
    }

    nv12ToArgb32_apple_imp(srcY, srcYStride, srcUV, srcUVStride, argbData, argbStride, width, height, matrix, range, permuteMap);

    if (!hasAlpha) {
        vImage_Buffer srcBuffer = { argbData, (vImagePixelCount)realHeight, (vImagePixelCount)width, (size_t)argbStride };
        vImage_Buffer dstBuffer = { dst, (vImagePixelCount)realHeight, (vImagePixelCount)width, (size_t)dstStride };
        vImageConvert_RGBA8888toRGB888(&srcBuffer, &dstBuffer, kvImageNoFlags);
    }
}

void nv12ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcUV, int srcUVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag) {
    nv12ToRgbColor_apple(srcY, srcYStride, srcUV, srcUVStride,
                         dst, dstStride, width, height, flag, ccap::PixelFormat::BGRA32);
}

void nv12ToRgba32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcUV, int srcUVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag) {
    nv12ToRgbColor_apple(srcY, srcYStride, srcUV, srcUVStride,
                         dst, dstStride, width, height, flag, ccap::PixelFormat::RGBA32);
}

void nv12ToBgr24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    nv12ToRgbColor_apple(srcY, srcYStride, srcUV, srcUVStride,
                         dst, dstStride, width, height, flag, ccap::PixelFormat::BGR24);
}

void nv12ToRgb24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    nv12ToRgbColor_apple(srcY, srcYStride, srcUV, srcUVStride,
                         dst, dstStride, width, height, flag, ccap::PixelFormat::RGB24);
}

//////////////////////  I420 to BGRA8888 //////////////////////

void i420ToBgra32_apple_imp(const uint8_t* srcY, int srcYStride,
                            const uint8_t* srcU, int srcUStride,
                            const uint8_t* srcV, int srcVStride,
                            uint8_t* dst, int dstStride,
                            int width, int height,
                            const vImage_YpCbCrToARGBMatrix* matrix,
                            const vImage_YpCbCrPixelRange* range,
                            const uint8_t permuteMap[4]) {
    assert(width > 0 && height != 0); // Internal implementation
    // Construct vImage_Buffer
    vImage_Buffer dstBuffer;
    std::shared_ptr<ccap::Allocator> sharedAllocator = ccap::getSharedAllocator();
    assert(sharedAllocator != nullptr);
    auto realHeight = std::abs(height);

    if (dstStride * realHeight > sharedAllocator->size()) {
        // Resize the shared allocator to accommodate the new size
        sharedAllocator->resize(dstStride * realHeight);
    }

    if (height < 0) {
        dstBuffer = { sharedAllocator->data(), (uint32_t)realHeight, (uint32_t)width, (size_t)dstStride };
    } else {
        dstBuffer = { dst, (uint32_t)realHeight, (uint32_t)width, (size_t)dstStride };
    }

    vImage_Buffer yBuffer = { (void*)srcY, (vImagePixelCount)realHeight, (vImagePixelCount)width, (size_t)srcYStride };
    vImage_Buffer uBuffer = { (void*)srcU, (vImagePixelCount)(realHeight / 2), (vImagePixelCount)(width / 2), (size_t)srcUStride };
    vImage_Buffer vBuffer = { (void*)srcV, (vImagePixelCount)(realHeight / 2), (vImagePixelCount)(width / 2), (size_t)srcVStride };

    vImage_YpCbCrToARGB info;
    vImage_Error err = vImageConvert_YpCbCrToARGB_GenerateConversion(matrix, range, &info,
                                                                     kvImage420Yp8_Cb8_Cr8, // I420 format
                                                                     kvImageARGB8888,       // Output format
                                                                     kvImageNoFlags);
    if (err != kvImageNoError) {
        CCAP_LOG_E("vImageConvert_YpCbCrToARGB_GenerateConversion failed: %zu", err);
        return;
    }

    err = vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&yBuffer, &uBuffer, &vBuffer, &dstBuffer, &info, permuteMap, 255, kvImageNoFlags);

    if (err != kvImageNoError) {
        CCAP_LOG_E("vImageConvert_420Yp8_Cb8_Cr8ToARGB8888 failed: %zu", err);
        return;
    }

    if (height < 0) { // Complete flip and pixel reordering, write final result to dst. Even with two-step operation, it's still much faster than regular CPU operations.
        verticalFlip_apple((const uint8_t*)dstBuffer.data, dstStride, dst, dstStride, realHeight);
    }
}

void i420ToRgbColor_apple(const uint8_t* srcY, int srcYStride,
                          const uint8_t* srcU, int srcUStride,
                          const uint8_t* srcV, int srcVStride,
                          uint8_t* dst, int dstStride,
                          int width, int height,
                          ConvertFlag flag, ccap::PixelFormat targetPixelFormat) {
    // Video Range (unclamped)
    constexpr vImage_YpCbCrPixelRange videoRange = { 16,  // Yp_bias
                                                     128, // CbCr_bias
                                                     235, // YpRangeMax
                                                     240, // CbCrRangeMax
                                                     255, // YpMax
                                                     0,   // YpMin
                                                     255, // CbCrMax
                                                     1 }; // CbCrMin
    // Full Range
    constexpr vImage_YpCbCrPixelRange fullRange = { 0,   // Yp_bias
                                                    128, // CbCr_bias
                                                    255, // YpRangeMax
                                                    255, // CbCrRangeMax
                                                    255, // YpMax
                                                    1,   // YpMin
                                                    255, // CbCrMax
                                                    0 }; // CbCrMin

    const auto* range = (flag & ConvertFlag::FullRange) ? &fullRange : &videoRange;
    const vImage_YpCbCrToARGBMatrix* matrix = (flag & ConvertFlag::BT601) ? kvImage_YpCbCrToARGBMatrix_ITU_R_601_4 : kvImage_YpCbCrToARGBMatrix_ITU_R_709_2;

    auto allocator = ccap::getSharedAllocator();
    auto realHeight = std::abs(height);
    bool hasAlpha = ccap::pixelFormatInclude(targetPixelFormat, ccap::kPixelFormatAlphaColorBit);
    auto argbStride = (4 * width + 31) & ~31; // 32-byte alignment

    if (!hasAlpha && allocator->size() < argbStride * realHeight) {
        allocator->resize(argbStride * realHeight); // Ensure allocator has sufficient space
    }

    uint8_t* argbData = hasAlpha ? dst : allocator->data(); // If no alpha channel, use shared allocator data

    uint8_t permuteMap[4]; /// Convert from ARGB to other formats.
    if (ccap::pixelFormatInclude(targetPixelFormat, ccap::kPixelFormatBGRBit)) {
        // BGR or BGRA
        permuteMap[0] = 3; // B
        permuteMap[1] = 2; // G
        permuteMap[2] = 1; // R
        permuteMap[3] = 0; // A (if exists)
    } else {
        // RGB or RGBA
        permuteMap[0] = 1; // R
        permuteMap[1] = 2; // G
        permuteMap[2] = 3; // B
        permuteMap[3] = 0; // A (if exists)
    }

    i420ToBgra32_apple_imp(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, argbData, argbStride, width, height, matrix, range, permuteMap);

    if (!hasAlpha) {
        vImage_Buffer srcBuffer = { argbData, (vImagePixelCount)realHeight, (vImagePixelCount)width, (size_t)argbStride };
        vImage_Buffer dstBuffer = { dst, (vImagePixelCount)realHeight, (vImagePixelCount)width, (size_t)dstStride };
        vImageConvert_RGBA8888toRGB888(&srcBuffer, &dstBuffer, kvImageNoFlags);
    }
}

void i420ToBgra32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcU, int srcUStride,
                        const uint8_t* srcV, int srcVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag) {
    i420ToRgbColor_apple(srcY, srcYStride, srcU, srcUStride,
                         srcV, srcVStride, dst, dstStride,
                         width, height, flag, ccap::PixelFormat::BGRA32);
}

void i420ToRgba32_apple(const uint8_t* srcY, int srcYStride,
                        const uint8_t* srcU, int srcUStride,
                        const uint8_t* srcV, int srcVStride,
                        uint8_t* dst, int dstStride,
                        int width, int height, ConvertFlag flag) {
    i420ToRgbColor_apple(srcY, srcYStride, srcU, srcUStride,
                         srcV, srcVStride, dst, dstStride,
                         width, height, flag, ccap::PixelFormat::RGBA32);
}

void i420ToBgr24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    i420ToRgbColor_apple(srcY, srcYStride, srcU, srcUStride,
                         srcV, srcVStride, dst, dstStride,
                         width, height, flag, ccap::PixelFormat::BGR24);
}

void i420ToRgb24_apple(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    i420ToRgbColor_apple(srcY, srcYStride, srcU, srcUStride,
                         srcV, srcVStride, dst, dstStride,
                         width, height, flag, ccap::PixelFormat::RGB24);
}

} // namespace ccap

#endif