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

/// @brief Inplace frame conversion functions
/// 
/// IMPORTANT CONSTRAINTS:
/// - These methods require that frame->data[0] points to EXTERNAL memory (e.g., camera buffer)
///   and NOT to frame->allocator->data()
/// - Each VideoFrame should only be converted ONCE using these functions
/// - The functions will allocate new memory via frame->allocator and update frame->data[0]
/// 
/// TYPICAL USAGE:
/// 1. Capture frame from camera: frame->data[0] points to camera's buffer
/// 2. Call inplaceConvertFrame*() ONCE: converts and moves data to allocator
/// 3. After conversion: frame->data[0] == frame->allocator->data()
/// 
/// VIOLATION will cause:
/// - Data corruption (reading freed memory)
/// - Assertion failure in debug builds
/// - Undefined behavior

// Export internal functions only when building tests
#ifdef CCAP_BUILD_TESTS
    #define CCAP_TEST_EXPORT CCAP_EXPORT
#else
    #define CCAP_TEST_EXPORT
#endif

namespace ccap {

CCAP_TEST_EXPORT bool inplaceConvertFrame(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip);
CCAP_TEST_EXPORT bool inplaceConvertFrameRGB(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip);
CCAP_TEST_EXPORT bool inplaceConvertFrameYUV2RGBColor(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip);

} // namespace ccap

#endif // CCAP_CONVERT_FRAME_H