/**
 * @file ccap_convert_c.h
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface for pixel conversion functions in ccap library.
 * @date 2025-05
 *
 * @note This is the C interface for pixel conversion. For C++, use ccap_convert.h instead.
 */

#pragma once
#ifndef CCAP_CONVERT_C_H
#define CCAP_CONVERT_C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Conversion Backend Management ========== */

/** @brief Conversion backend enumeration */
typedef enum {
    CCAP_CONVERT_BACKEND_AUTO = 0,             /**< Automatically choose the best available backend */
    CCAP_CONVERT_BACKEND_CPU = 1,              /**< CPU implementation */
    CCAP_CONVERT_BACKEND_AVX2 = 2,             /**< AVX2 implementation */
    CCAP_CONVERT_BACKEND_APPLE_ACCELERATE = 3, /**< Apple Accelerate implementation */
    CCAP_CONVERT_BACKEND_NEON = 4,             /**< ARM NEON implementation */
} CcapConvertBackend;

/** @brief Conversion flags for color space and range */
typedef enum {
    CCAP_CONVERT_FLAG_BT601 = 0x1,                                                      /**< Use BT.601 color space */
    CCAP_CONVERT_FLAG_BT709 = 0x2,                                                      /**< Use BT.709 color space */
    CCAP_CONVERT_FLAG_FULL_RANGE = 0x10,                                                /**< Use full range color space */
    CCAP_CONVERT_FLAG_VIDEO_RANGE = 0x20,                                               /**< Use video range color space */
    CCAP_CONVERT_FLAG_DEFAULT = CCAP_CONVERT_FLAG_BT601 | CCAP_CONVERT_FLAG_VIDEO_RANGE /**< Default: BT.601 video range */
} CcapConvertFlag;

/**
 * @brief Check if AVX2 is supported by the CPU
 * @return true if AVX2 is available, false otherwise
 */
bool ccap_convert_has_avx2(void);

/**
 * @brief Check if AVX2 is currently enabled
 * @return true if AVX2 is enabled, false otherwise
 */
bool ccap_convert_can_use_avx2(void);

/**
 * @brief Enable or disable AVX2 implementation
 * @param enable true to enable AVX2, false to disable
 * @return true if AVX2 is available and enabled, false otherwise
 */
bool ccap_convert_enable_avx2(bool enable);

/**
 * @brief Check if Apple Accelerate is available
 * @return true if Apple Accelerate is available, false otherwise
 */
bool ccap_convert_has_apple_accelerate(void);

/**
 * @brief Check if Apple Accelerate is currently enabled
 * @return true if Apple Accelerate is enabled, false otherwise
 */
bool ccap_convert_can_use_apple_accelerate(void);

/**
 * @brief Enable or disable Apple Accelerate implementation
 * @param enable true to enable Apple Accelerate, false to disable
 * @return true if Apple Accelerate is available and enabled, false otherwise
 */
bool ccap_convert_enable_apple_accelerate(bool enable);

/**
 * @brief Check if ARM NEON is supported by the CPU
 * @return true if ARM NEON is available, false otherwise
 */
bool ccap_convert_has_neon(void);

/**
 * @brief Check if ARM NEON is currently enabled
 * @return true if ARM NEON is enabled, false otherwise
 */
bool ccap_convert_can_use_neon(void);

/**
 * @brief Enable or disable ARM NEON implementation
 * @param enable true to enable ARM NEON, false to disable
 * @return true if ARM NEON is available and enabled, false otherwise
 */
bool ccap_convert_enable_neon(bool enable);

/**
 * @brief Get the current conversion backend that will be used
 * @return Current conversion backend
 */
CcapConvertBackend ccap_convert_get_backend(void);

/**
 * @brief Set the conversion backend
 * @param backend Backend to set
 * @return true if the backend was set successfully, false otherwise
 */
bool ccap_convert_set_backend(CcapConvertBackend backend);

/* ========== Color Space Conversion Functions ========== */

/**
 * @brief Convert single YUV pixel to RGB using BT.601 video range
 * @param y Y component
 * @param u U component
 * @param v V component
 * @param r Output R component (0-255)
 * @param g Output G component (0-255)
 * @param b Output B component (0-255)
 */
void ccap_convert_yuv_to_rgb_601v(int y, int u, int v, int* r, int* g, int* b);

/**
 * @brief Convert single YUV pixel to RGB using BT.709 video range
 * @param y Y component
 * @param u U component
 * @param v V component
 * @param r Output R component (0-255)
 * @param g Output G component (0-255)
 * @param b Output B component (0-255)
 */
void ccap_convert_yuv_to_rgb_709v(int y, int u, int v, int* r, int* g, int* b);

/**
 * @brief Convert single YUV pixel to RGB using BT.601 full range
 * @param y Y component
 * @param u U component
 * @param v V component
 * @param r Output R component (0-255)
 * @param g Output G component (0-255)
 * @param b Output B component (0-255)
 */
void ccap_convert_yuv_to_rgb_601f(int y, int u, int v, int* r, int* g, int* b);

/**
 * @brief Convert single YUV pixel to RGB using BT.709 full range
 * @param y Y component
 * @param u U component
 * @param v V component
 * @param r Output R component (0-255)
 * @param g Output G component (0-255)
 * @param b Output B component (0-255)
 */
void ccap_convert_yuv_to_rgb_709f(int y, int u, int v, int* r, int* g, int* b);

/* ========== Color Channel Shuffling ========== */

/**
 * @brief Convert RGBA to BGRA (swap R and B channels)
 * @param src Source RGBA data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGRA data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_rgba_to_bgra(const uint8_t* src, int src_stride,
                               uint8_t* dst, int dst_stride,
                               int width, int height);

/**
 * @brief Convert BGRA to RGBA (swap R and B channels)
 * @param src Source BGRA data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGBA data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_bgra_to_rgba(const uint8_t* src, int src_stride,
                               uint8_t* dst, int dst_stride,
                               int width, int height);

/**
 * @brief Convert RGBA to BGR (remove alpha, swap R and B channels)
 * @param src Source RGBA data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGR data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_rgba_to_bgr(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert BGRA to RGB (remove alpha, swap R and B channels)
 * @param src Source BGRA data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGB data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_bgra_to_rgb(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert RGBA to RGB (remove alpha channel)
 * @param src Source RGBA data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGB data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_rgba_to_rgb(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert BGRA to BGR (remove alpha channel)
 * @param src Source BGRA data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGR data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_bgra_to_bgr(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert RGB to BGRA (add alpha=255, swap R and B channels)
 * @param src Source RGB data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGRA data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_rgb_to_bgra(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert BGR to RGBA (add alpha=255, swap R and B channels)
 * @param src Source BGR data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGBA data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_bgr_to_rgba(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert RGB to RGBA (add alpha=255)
 * @param src Source RGB data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGBA data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_rgb_to_rgba(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert BGR to BGRA (add alpha=255)
 * @param src Source BGR data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGRA data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_bgr_to_bgra(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height);

/**
 * @brief Convert RGB to BGR (swap R and B channels)
 * @param src Source RGB data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGR data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_rgb_to_bgr(const uint8_t* src, int src_stride,
                             uint8_t* dst, int dst_stride,
                             int width, int height);

/**
 * @brief Convert BGR to RGB (swap R and B channels)
 * @param src Source BGR data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGB data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 */
void ccap_convert_bgr_to_rgb(const uint8_t* src, int src_stride,
                             uint8_t* dst, int dst_stride,
                             int width, int height);

/* ========== YUV to RGB Conversions ========== */

/**
 * @brief Convert NV12 to BGR24
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_uv UV plane data (interleaved)
 * @param src_uv_stride UV plane stride in bytes
 * @param dst Destination BGR24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_nv12_to_bgr24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_uv, int src_uv_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert NV12 to RGB24
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_uv UV plane data (interleaved)
 * @param src_uv_stride UV plane stride in bytes
 * @param dst Destination RGB24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_nv12_to_rgb24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_uv, int src_uv_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert NV12 to BGRA32
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_uv UV plane data (interleaved)
 * @param src_uv_stride UV plane stride in bytes
 * @param dst Destination BGRA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_nv12_to_bgra32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_uv, int src_uv_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert NV12 to RGBA32
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_uv UV plane data (interleaved)
 * @param src_uv_stride UV plane stride in bytes
 * @param dst Destination RGBA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_nv12_to_rgba32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_uv, int src_uv_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert I420 to BGR24
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_u U plane data
 * @param src_u_stride U plane stride in bytes
 * @param src_v V plane data
 * @param src_v_stride V plane stride in bytes
 * @param dst Destination BGR24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_i420_to_bgr24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_u, int src_u_stride,
                                const uint8_t* src_v, int src_v_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert I420 to RGB24
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_u U plane data
 * @param src_u_stride U plane stride in bytes
 * @param src_v V plane data
 * @param src_v_stride V plane stride in bytes
 * @param dst Destination RGB24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_i420_to_rgb24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_u, int src_u_stride,
                                const uint8_t* src_v, int src_v_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert I420 to BGRA32
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_u U plane data
 * @param src_u_stride U plane stride in bytes
 * @param src_v V plane data
 * @param src_v_stride V plane stride in bytes
 * @param dst Destination BGRA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_i420_to_bgra32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_u, int src_u_stride,
                                 const uint8_t* src_v, int src_v_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert I420 to RGBA32
 * @param src_y Y plane data
 * @param src_y_stride Y plane stride in bytes
 * @param src_u U plane data
 * @param src_u_stride U plane stride in bytes
 * @param src_v V plane data
 * @param src_v_stride V plane stride in bytes
 * @param dst Destination RGBA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_i420_to_rgba32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_u, int src_u_stride,
                                 const uint8_t* src_v, int src_v_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

/* ========== YUYV (YUV 4:2:2 packed) Conversions ========== */

/**
 * @brief Convert YUYV to BGR24
 * @param src Source YUYV data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGR24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_yuyv_to_bgr24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert YUYV to RGB24
 * @param src Source YUYV data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGB24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_yuyv_to_rgb24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert YUYV to BGRA32
 * @param src Source YUYV data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGRA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_yuyv_to_bgra32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert YUYV to RGBA32
 * @param src Source YUYV data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGBA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_yuyv_to_rgba32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

/* ========== UYVY (YUV 4:2:2 packed) Conversions ========== */

/**
 * @brief Convert UYVY to BGR24
 * @param src Source UYVY data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGR24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_uyvy_to_bgr24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert UYVY to RGB24
 * @param src Source UYVY data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGB24 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_uyvy_to_rgb24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert UYVY to BGRA32
 * @param src Source UYVY data
 * @param src_stride Source stride in bytes
 * @param dst Destination BGRA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_uyvy_to_bgra32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

/**
 * @brief Convert UYVY to RGBA32
 * @param src Source UYVY data
 * @param src_stride Source stride in bytes
 * @param dst Destination RGBA32 data
 * @param dst_stride Destination stride in bytes
 * @param width Image width in pixels
 * @param height Image height in pixels (negative for vertical flip)
 * @param flag Conversion flags (color space and range)
 */
void ccap_convert_uyvy_to_rgba32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag);

#ifdef __cplusplus
}
#endif

#endif /* CCAP_CONVERT_C_H */
