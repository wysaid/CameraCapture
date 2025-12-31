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
#include <cstring>

namespace ccap {
bool inplaceConvertFrameYUV2RGBColor(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) { /// (NV12/I420/YUYV/UYVY) -> (BGR24/BGRA32)

    /// TODO: Fix toFormat here, only support YUV -> (BGR24/BGRA32). Simplify SDK design. Will improve later.

    // ASSERTION: Ensure frame->data[0] points to EXTERNAL memory, not allocator->data()
    // This validates the design constraint: VideoFrame should only be converted once
    assert(frame->allocator == nullptr || frame->data[0] != frame->allocator->data() && "DESIGN VIOLATION: frame->data[0] must point to external memory (e.g., camera buffer), not allocator memory. "
                                                                                        "Each VideoFrame should only be converted ONCE using inplaceConvertFrame*() functions.");

    auto inputFormat = frame->pixelFormat;
    assert((inputFormat & kPixelFormatYUVColorBit) != 0 && (toFormat & kPixelFormatYUVColorBit) == 0);
    bool isInputNV12 = pixelFormatInclude(inputFormat, PixelFormat::NV12);
    bool isInputYUYV = pixelFormatInclude(inputFormat, PixelFormat::YUYV);
    bool isInputUYVY = pixelFormatInclude(inputFormat, PixelFormat::UYVY);
    bool outputHasAlpha = toFormat & kPixelFormatAlphaColorBit;
    bool isOutputBGR = toFormat & kPixelFormatBGRBit; // If not BGR, then RGB

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

    if (isInputNV12) { // NV12 -> BGR24, RGB24 in libyuv is actually BGR24

        if (outputHasAlpha) {
            if (isOutputBGR) {
                nv12ToBgra32(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            } else {
                nv12ToRgba32(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            }
            return true;
        } else {
            if (isOutputBGR) {
                nv12ToBgr24(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            } else {
                nv12ToRgb24(inputData0, stride0, inputData1, stride1, frame->data[0], newLineSize, width, height);
            }
            return true;
        }
    } else if (isInputYUYV) { // YUYV -> BGR24/BGRA32

        if (outputHasAlpha) {
            if (isOutputBGR) {
                yuyvToBgra32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                yuyvToRgba32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
            return true;
        } else {
            if (isOutputBGR) {
                yuyvToBgr24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                yuyvToRgb24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
            return true;
        }
    } else if (isInputUYVY) { // UYVY -> BGR24/BGRA32

        if (outputHasAlpha) {
            if (isOutputBGR) {
                uyvyToBgra32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                uyvyToRgba32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
            return true;
        } else {
            if (isOutputBGR) {
                uyvyToBgr24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                uyvyToRgb24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
            return true;
        }
    } else { // I420 -> BGR24

        if (outputHasAlpha) {
            if (isOutputBGR) {
                i420ToBgra32(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            } else {
                i420ToRgba32(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            }
            return true;
        } else {
            if (isOutputBGR) {
                i420ToBgr24(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            } else {
                i420ToRgb24(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], newLineSize, width, height);
            }
            return true;
        }
    }

    return false;
}

bool inplaceConvertFrameRGB(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) {
    // RGB(A) interconversion

    // ASSERTION: Ensure frame->data[0] points to EXTERNAL memory, not allocator->data()
    // This validates the design constraint: VideoFrame should only be converted once
    assert(frame->allocator == nullptr || frame->data[0] != frame->allocator->data() && "DESIGN VIOLATION: frame->data[0] must point to external memory (e.g., camera buffer), not allocator memory. "
                                                                                        "Each VideoFrame should only be converted ONCE using inplaceConvertFrame*() functions.");

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
            const uint8_t kShuffleMap[4] = { 2, 1, 0, 3 }; // RGBA->BGRA or BGRA->RGBA
            libyuv::ARGBShuffle(inputBytes, inputLineSize, outputBytes, newLineSize, kShuffleMap, frame->width, height);
#else
            rgbaToBgra(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
#endif
        } else // RGB <-> BGR
        {
            rgbToBgr(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, height);
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

inline bool inplaceConvertFrameImp(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) {
    // ASSERTION: Ensure frame->data[0] points to EXTERNAL memory, not allocator->data()
    // This validates the design constraint: VideoFrame should only be converted once
    assert(frame->allocator == nullptr || frame->data[0] != frame->allocator->data() && "DESIGN VIOLATION: frame->data[0] must point to external memory (e.g., camera buffer), not allocator memory. "
                                                                                        "Each VideoFrame should only be converted ONCE using inplaceConvertFrame*() functions.");

    if (frame->pixelFormat == toFormat) {
        if (verticalFlip && (toFormat & kPixelFormatRGBColorBit)) { // flip upside down
            int srcStride = (int)frame->stride[0];
            int dstStride = srcStride;
            auto height = frame->height;
            auto* src = frame->data[0];
            frame->allocator->resize(srcStride * height);
            auto* dst = frame->allocator->data();
            frame->data[0] = dst;
            /// Read in reverse order
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

bool inplaceConvertFrame(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) {
    auto ret = inplaceConvertFrameImp(frame, toFormat, verticalFlip);
    if (ret) {
        assert(frame->pixelFormat == toFormat);
        assert(frame->allocator != nullptr && frame->data[0] == frame->allocator->data());
        frame->sizeInBytes = frame->allocator->size();
    }
    return ret;
}

} // namespace ccap
