/**
 * @file ccap_convert_frame.h
 * @author wysaid (this@wysaid.org)
 * @brief pixel convert functions for ccap.
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_CONVERT_FRAME_H
#define CCAP_CONVERT_FRAME_H

#include "ccap_def.h"

/// The methods here require that the data field of frame is not allocated with an allocator.
/// This method will use an allocator to allocate memory and convert to a new data format.

namespace ccap {

bool inplaceConvertFrame(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip);
bool inplaceConvertFrameRGB(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip);
bool inplaceConvertFrameYUV2RGBColor(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip);

} // namespace ccap

#endif // CCAP_CONVERT_FRAME_H