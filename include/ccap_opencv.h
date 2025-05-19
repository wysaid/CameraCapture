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
 * @brief 把一个 RGB/BGR/RGBA/BGRA 图像格式的 Frame 转换为 cv::Mat, 不会改变通道顺序, 也不支持 YUV 格式.
 * @param frame
 * @param mat
 * @note 注意, 此函数不会拷贝数据, 需要保持 Frame 的生命周期.
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
