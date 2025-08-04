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

// 内部辅助函数：转换NV12格式到RGB
static bool convertNV12ToRGB(VideoFrame* frame, PixelFormat toFormat, bool outputHasAlpha, bool isOutputBGR, int width, int height) {
    // 保存原始数据
    uint8_t* inputData0 = frame->data[0];
    uint8_t* inputData1 = frame->data[1];
    int stride0 = frame->stride[0];
    int stride1 = frame->stride[1];
    
    // 创建临时缓冲区保存原始数据
    std::vector<uint8_t> tempBuffer(frame->sizeInBytes);
    std::memcpy(tempBuffer.data(), inputData0, stride0 * frame->height);
    if (inputData1) {
        std::memcpy(tempBuffer.data() + stride0 * frame->height, inputData1, frame->sizeInBytes - stride0 * frame->height);
        inputData1 = tempBuffer.data() + stride0 * frame->height;
    }
    inputData0 = tempBuffer.data();
    
    // 准备输出缓冲区
    auto newLineSize = outputHasAlpha ? frame->width * 4 : (frame->width * 3 + 31) & ~31;
    frame->allocator->resize(newLineSize * frame->height);
    frame->data[0] = frame->allocator->data();
    frame->stride[0] = newLineSize;
    frame->data[1] = nullptr;
    frame->data[2] = nullptr;
    frame->stride[1] = 0;
    frame->stride[2] = 0;
    frame->pixelFormat = toFormat;
    
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
}

// 内部辅助函数：转换I420格式到RGB
static bool convertI420ToRGB(VideoFrame* frame, PixelFormat toFormat, bool outputHasAlpha, bool isOutputBGR, int width, int height) {
    // 保存原始数据
    uint8_t* inputData0 = frame->data[0];
    uint8_t* inputData1 = frame->data[1];
    uint8_t* inputData2 = frame->data[2];
    int stride0 = frame->stride[0];
    int stride1 = frame->stride[1];
    int stride2 = frame->stride[2];
    
    // 创建临时缓冲区保存原始数据
    std::vector<uint8_t> tempBuffer(frame->sizeInBytes);
    size_t plane0Size = stride0 * frame->height;
    size_t plane1Size = stride1 * (frame->height / 2);
    std::memcpy(tempBuffer.data(), inputData0, plane0Size);
    if (inputData1) {
        std::memcpy(tempBuffer.data() + plane0Size, inputData1, plane1Size);
        inputData1 = tempBuffer.data() + plane0Size;
    }
    if (inputData2) {
        std::memcpy(tempBuffer.data() + plane0Size + plane1Size, inputData2, frame->sizeInBytes - plane0Size - plane1Size);
        inputData2 = tempBuffer.data() + plane0Size + plane1Size;
    }
    inputData0 = tempBuffer.data();
    
    // 准备输出缓冲区
    auto newLineSize = outputHasAlpha ? frame->width * 4 : (frame->width * 3 + 31) & ~31;
    frame->allocator->resize(newLineSize * frame->height);
    frame->data[0] = frame->allocator->data();
    frame->stride[0] = newLineSize;
    frame->data[1] = nullptr;
    frame->data[2] = nullptr;
    frame->stride[1] = 0;
    frame->stride[2] = 0;
    frame->pixelFormat = toFormat;
    
    if (outputHasAlpha) {
#if ENABLE_LIBYUV
        return libyuv::I420ToARGB(inputData0, stride0, inputData1, stride1, inputData2, stride2, 
                                 frame->data[0], newLineSize, width, height) == 0;
#else
        if (isOutputBGR) {
            i420ToBgra32(inputData0, stride0, inputData1, stride1, inputData2, stride2, 
                        frame->data[0], newLineSize, width, height);
        } else {
            i420ToRgba32(inputData0, stride0, inputData1, stride1, inputData2, stride2, 
                        frame->data[0], newLineSize, width, height);
        }
        return true;
#endif
    } else {
#if ENABLE_LIBYUV
        return libyuv::I420ToRGB24(inputData0, stride0, inputData1, stride1, inputData2, stride2, 
                                  frame->data[0], newLineSize, width, height) == 0;
#else
        if (isOutputBGR) {
            i420ToBgr24(inputData0, stride0, inputData1, stride1, inputData2, stride2, 
                       frame->data[0], newLineSize, width, height);
        } else {
            i420ToRgb24(inputData0, stride0, inputData1, stride1, inputData2, stride2, 
                       frame->data[0], newLineSize, width, height);
        }
        return true;
#endif
    }
}

// 内部辅助函数：转换打包的YUV格式(YUYV/UYVY)到RGB
static bool convertPackedYUVToRGB(VideoFrame* frame, PixelFormat toFormat, bool isYUYV, bool outputHasAlpha, bool isOutputBGR, int width, int height) {
    // 保存原始数据
    uint8_t* inputData0 = frame->data[0];
    int stride0 = frame->stride[0];
    
    // 创建临时缓冲区保存原始数据
    std::vector<uint8_t> tempBuffer(frame->sizeInBytes);
    std::memcpy(tempBuffer.data(), inputData0, frame->sizeInBytes);
    inputData0 = tempBuffer.data();
    
    // 准备输出缓冲区
    auto newLineSize = outputHasAlpha ? frame->width * 4 : (frame->width * 3 + 31) & ~31;
    frame->allocator->resize(newLineSize * frame->height);
    frame->data[0] = frame->allocator->data();
    frame->stride[0] = newLineSize;
    frame->data[1] = nullptr;
    frame->data[2] = nullptr;
    frame->stride[1] = 0;
    frame->stride[2] = 0;
    frame->pixelFormat = toFormat;
    
    if (isYUYV) { // YUYV -> RGB
        if (outputHasAlpha) {
            if (isOutputBGR) {
                yuyvToBgra32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                yuyvToRgba32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
        } else {
            if (isOutputBGR) {
                yuyvToBgr24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                yuyvToRgb24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
        }
    } else { // UYVY -> RGB
        if (outputHasAlpha) {
            if (isOutputBGR) {
                uyvyToBgra32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                uyvyToRgba32(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
        } else {
            if (isOutputBGR) {
                uyvyToBgr24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            } else {
                uyvyToRgb24(inputData0, stride0, frame->data[0], newLineSize, width, height);
            }
        }
    }
    return true;
}

bool inplaceConvertFrameYUV2RGBColor(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) { /// (NV12/I420/YUYV/UYVY) -> (BGR24/BGRA32)

    /// TODO: 这里修正一下 toFormat, 只支持 YUV -> (BGR24/BGRA24). 简化一下 SDK 的设计. 后续再完善.

    auto inputFormat = frame->pixelFormat;
    assert((inputFormat & kPixelFormatYUVColorBit) != 0 && (toFormat & kPixelFormatYUVColorBit) == 0);
    
    bool isInputNV12 = pixelFormatInclude(inputFormat, PixelFormat::NV12);
    bool isInputYUYV = pixelFormatInclude(inputFormat, PixelFormat::YUYV);
    bool isInputUYVY = pixelFormatInclude(inputFormat, PixelFormat::UYVY);
    bool outputHasAlpha = toFormat & kPixelFormatAlphaColorBit;
    bool isOutputBGR = toFormat & kPixelFormatBGRBit; // 不是 BGR 就是 RGB

    int width = frame->width;
    int height = verticalFlip ? -(int)frame->height : frame->height;

    // 根据输入格式调用对应的转换函数
    if (isInputNV12) {
        return convertNV12ToRGB(frame, toFormat, outputHasAlpha, isOutputBGR, width, height);
    } else if (isInputYUYV || isInputUYVY) {
        return convertPackedYUVToRGB(frame, toFormat, isInputYUYV, outputHasAlpha, isOutputBGR, width, height);
    } else { // I420
        return convertI420ToRGB(frame, toFormat, outputHasAlpha, isOutputBGR, width, height);
    }
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
