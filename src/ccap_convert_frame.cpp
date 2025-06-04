/**
 * @file ccap_convert_frame.cpp
 * @author wysaid (this@wysaid.org)
 * @brief pixel convert functions for ccap.
 * @date 2025-05
 *
 */

#include "ccap_convert_frame.h"

#include "ccap_convert.h"
#include "ccap_imp.h"

#include <cassert>

namespace ccap {
bool inplaceConvertFrameYUV2RGBColor(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) { /// (NV12/I420) -> (BGR24/BGRA32)

    /// TODO: 这里修正一下 toFormat, 只支持 YUV -> (BGR24/BGRA24). 简化一下 SDK 的设计. 后续再完善.

    auto inputFormat = frame->pixelFormat;
    assert((inputFormat & kPixelFormatYUVColorBit) != 0 && (toFormat & kPixelFormatYUVColorBit) == 0);
    bool isInputNV12 = pixelFormatInclude(inputFormat, PixelFormat::NV12);
    bool outputHasAlpha = toFormat & kPixelFormatAlphaColorBit;
    bool isOutputBGR = toFormat & kPixelFormatBGRBit; // 不是 BGR 就是 RGB

    uint8_t* inputData0 = frame->data[0];
    uint8_t* inputData1 = frame->data[1];
    uint8_t* inputData2 = frame->data[2];
    int stride0 = frame->stride[0];
    int stride1 = frame->stride[1];
    int stride2 = frame->stride[2];
    int width = frame->width;
    int height = verticalFlip ? -(int)frame->height : frame->height;

    auto newLineSize = outputHasAlpha ? frame->width * 4 : (frame->width * 3 + 31) & ~31;

    frame->allocator->resize(newLineSize * frame->height);
    frame->data[0] = frame->allocator->data();
    frame->stride[0] = newLineSize;
    frame->data[1] = nullptr;
    frame->data[2] = nullptr;
    frame->stride[1] = 0;
    frame->stride[2] = 0;
    frame->pixelFormat = toFormat;

    if (isInputNV12) { // NV12 -> BGR24, libyuv 里面的 RGB24 实际上是 BGR24

        if (outputHasAlpha) {
#if ENABLE_LIBYUV
            return libyuv::NV12ToARGB(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height) == 0;
#else
            if (isOutputBGR) {
                nv12ToBgra32(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            } else {
                nv12ToRgba32(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            }
            return true;
#endif
        } else {
#if ENABLE_LIBYUV
            return libyuv::NV12ToRGB24(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height) == 0;
#else
            if (isOutputBGR) {
                nv12ToBgr24(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            } else {
                nv12ToRgb24(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            }
            return true;
#endif
        }
    } else { // I420 -> BGR24

        if (outputHasAlpha) {
#if ENABLE_LIBYUV
            return libyuv::I420ToARGB(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width,
                                      height) == 0;
#else
            if (isOutputBGR) {
                i420ToBgra32(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            } else {
                i420ToRgba32(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            }
            return true;
#endif
        } else {
#if ENABLE_LIBYUV
            return libyuv::I420ToRGB24(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width,
                                       height) == 0;
#else
            if (isOutputBGR) {
                i420ToBgr24(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            } else {
                i420ToRgb24(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            }
            return true;
#endif
        }
    }

    return false;
}

bool inplaceConvertFrameRGB(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) {
    // rgb(a) 互转

    uint8_t* inputBytes = frame->data[0];
    int inputLineSize = frame->stride[0];
    auto outputChannelCount = (toFormat & kPixelFormatAlphaColorBit) ? 4 : 3;
    // Ensure 16/32 byte alignment for best performance
    auto newLineSize = outputChannelCount == 3 ? ((frame->width * 3 + 31) & ~31) : (frame->width * 4);
    auto inputFormat = frame->pixelFormat;

    auto inputChannelCount = (inputFormat & kPixelFormatAlphaColorBit) ? 4 : 3;

    bool isInputRGB = inputFormat & kPixelFormatRGBBit; ///< Not RGB means BGR
    bool isOutputRGB = toFormat & kPixelFormatRGBBit;   ///< Not RGB means BGR
    bool swapRB = isInputRGB != isOutputRGB;            ///< Whether R and B channels need to be swapped

    frame->allocator->resize(newLineSize * frame->height);

    uint8_t* outputBytes = frame->allocator->data();
    int height = verticalFlip ? -(int)frame->height : frame->height;

    frame->stride[0] = newLineSize;
    frame->data[0] = outputBytes;
    frame->pixelFormat = toFormat;

    if (inputChannelCount == outputChannelCount) { /// only RGB <-> BGR, RGBA <-> BGRA
        assert(swapRB);
        if (inputChannelCount == 4) // RGBA <-> BGRA
        {
#if ENABLE_LIBYUV
            const uint8_t kShuffleMap[4] = { 2, 1, 0, 3 }; // RGBA->BGRA 或 BGRA->RGBA
            libyuv::ARGBShuffle(inputBytes, inputLineSize, outputBytes, newLineSize, kShuffleMap, frame->width, height);
#else
            rgbaToBgra(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
#endif
        } else // RGB <-> BGR
        {
            rgbaToBgra(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
        }
    } else /// Different number of channels, only 4 channels <-> 3 channels
    {
        if (inputChannelCount == 4) // 4 channels -> 3 channels
        {
            if (swapRB) { // Possible cases: RGBA->BGR, BGRA->RGB
                rgbaToBgr(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
            } else { // Possible cases: RGBA->RGB, BGRA->BGR
                rgbaToRgb(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
            }
        } else // 3 channels -> 4 channels
        {
            if (swapRB) { // Possible cases: BGR->RGBA, RGB->BGRA
                rgbToBgra(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
            } else { // Possible cases: BGR->BGRA, RGB->RGBA
                rgbToRgba(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
            }
        }
    }
    return true;
}

bool inplaceConvertFrame(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) {
    if (frame->pixelFormat == toFormat) {
        if (verticalFlip && (toFormat & kPixelFormatRGBColorBit)) { // flip upside down
            int srcStride = (int)frame->stride[0];
            int dstStride = srcStride;
            auto height = frame->height;
            auto* src = frame->data[0];
            frame->allocator->resize(srcStride * height);
            auto* dst = frame->allocator->data();
            frame->data[0] = dst;
            /// 反向读取
            src = src + srcStride * (height - 1);
            srcStride = -srcStride;
            for (uint32_t i = 0; i < height; ++i) {
                memcpy(dst, src, dstStride);
                dst += dstStride;
                src += srcStride;
            }

            return true;
        }

        return false;
    }

    bool isInputYUV = (frame->pixelFormat & kPixelFormatYUVColorBit) != 0;
    bool isOutputYUV = (toFormat & kPixelFormatYUVColorBit) != 0;
    if (isInputYUV || isOutputYUV) // yuv <-> rgb
    {
#if ENABLE_LIBYUV
        if (isInputYUV && isOutputYUV) // yuv <-> yuv
            return inplaceConvertFrameYUV2YUV(frame, toFormat, verticalFlip);
#endif

        if (isInputYUV) // yuv -> BGR
            return inplaceConvertFrameYUV2RGBColor(frame, toFormat, verticalFlip);
        return false; // no rgb -> yuv
    }

    return inplaceConvertFrameRGB(frame, toFormat, verticalFlip);
}

} // namespace ccap
