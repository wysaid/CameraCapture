//
// Created by wysaid on 2025/5/19.
//

#pragma once
#ifndef CCAP_OPENCV_H
#define CCAP_OPENCV_H

#include "ccap_core.h"

#include <memory>
#include <opencv2/core.hpp>

/// header only, to convert ccap::Frame to cv::Mat

namespace ccap
{
/**
 * @brief Converts a Frame in RGB/BGR/RGBA/BGRA image format to cv::Mat. Does not change channel order and does not support YUV format.
 * @param frame
 * @param mat
 * @note Note: This function does not copy data. The lifetime of the Frame must be maintained.
 */
inline cv::Mat convertRgbFrameToMat(const Frame& frame)
{
    if (!((uint32_t)frame.pixelFormat & (uint32_t)kPixelFormatRGBColorBit))
    {
        return {};
    }

    auto typeEnum = (uint32_t)frame.pixelFormat & (uint32_t)kPixelFormatAlphaColorBit ? CV_8UC4 : CV_8UC3;
    return cv::Mat(frame.height, frame.width, typeEnum, frame.data[0], frame.stride[0]);
}
} // namespace ccap

#endif // CCAP_OPENCV_H
