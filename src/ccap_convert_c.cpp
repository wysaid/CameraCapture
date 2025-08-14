/**
 * @file ccap_convert_c.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface implementation for pixel conversion functions in ccap library.
 * @date 2025-05
 *
 */

#include "ccap_convert_c.h"

#include "ccap_convert.h"

#include <cstring>

// Static assertions to ensure C and C++ convert enum values are consistent
// This prevents type casting issues when passing enum values between C and C++ layers

// ConvertBackend enum consistency checks
static_assert(static_cast<uint32_t>(CCAP_CONVERT_BACKEND_AUTO) == static_cast<uint32_t>(ccap::ConvertBackend::AUTO),
              "C and C++ ConvertBackend::AUTO values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_BACKEND_CPU) == static_cast<uint32_t>(ccap::ConvertBackend::CPU),
              "C and C++ ConvertBackend::CPU values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_BACKEND_AVX2) == static_cast<uint32_t>(ccap::ConvertBackend::AVX2),
              "C and C++ ConvertBackend::AVX2 values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_BACKEND_APPLE_ACCELERATE) == static_cast<uint32_t>(ccap::ConvertBackend::AppleAccelerate),
              "C and C++ ConvertBackend::AppleAccelerate values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_BACKEND_NEON) == static_cast<uint32_t>(ccap::ConvertBackend::NEON),
              "C and C++ ConvertBackend::NEON values must match");

// ConvertFlag enum consistency checks
static_assert(static_cast<uint32_t>(CCAP_CONVERT_FLAG_BT601) == static_cast<uint32_t>(ccap::ConvertFlag::BT601),
              "C and C++ ConvertFlag::BT601 values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_FLAG_BT709) == static_cast<uint32_t>(ccap::ConvertFlag::BT709),
              "C and C++ ConvertFlag::BT709 values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_FLAG_FULL_RANGE) == static_cast<uint32_t>(ccap::ConvertFlag::FullRange),
              "C and C++ ConvertFlag::FullRange values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_FLAG_VIDEO_RANGE) == static_cast<uint32_t>(ccap::ConvertFlag::VideoRange),
              "C and C++ ConvertFlag::VideoRange values must match");
static_assert(static_cast<uint32_t>(CCAP_CONVERT_FLAG_DEFAULT) == static_cast<uint32_t>(ccap::ConvertFlag::Default),
              "C and C++ ConvertFlag::Default values must match");

extern "C" {

/* ========== Conversion Backend Management ========== */

bool ccap_convert_has_avx2(void) {
    return ccap::hasAVX2();
}

bool ccap_convert_can_use_avx2(void) {
    return ccap::canUseAVX2();
}

bool ccap_convert_enable_avx2(bool enable) {
    return ccap::enableAVX2(enable);
}

bool ccap_convert_has_apple_accelerate(void) {
    return ccap::hasAppleAccelerate();
}

bool ccap_convert_can_use_apple_accelerate(void) {
    return ccap::canUseAppleAccelerate();
}

bool ccap_convert_enable_apple_accelerate(bool enable) {
    return ccap::enableAppleAccelerate(enable);
}

bool ccap_convert_has_neon(void) {
    return ccap::hasNEON();
}

bool ccap_convert_can_use_neon(void) {
    return ccap::canUseNEON();
}

bool ccap_convert_enable_neon(bool enable) {
    return ccap::enableNEON(enable);
}

CcapConvertBackend ccap_convert_get_backend(void) {
    ccap::ConvertBackend backend = ccap::getConvertBackend();
    switch (backend) {
    case ccap::ConvertBackend::AUTO:
        return CCAP_CONVERT_BACKEND_AUTO;
    case ccap::ConvertBackend::CPU:
        return CCAP_CONVERT_BACKEND_CPU;
    case ccap::ConvertBackend::AVX2:
        return CCAP_CONVERT_BACKEND_AVX2;
    case ccap::ConvertBackend::AppleAccelerate:
        return CCAP_CONVERT_BACKEND_APPLE_ACCELERATE;
    case ccap::ConvertBackend::NEON:
        return CCAP_CONVERT_BACKEND_NEON;
    default:
        return CCAP_CONVERT_BACKEND_AUTO;
    }
}

bool ccap_convert_set_backend(CcapConvertBackend backend) {
    ccap::ConvertBackend cppBackend;
    switch (backend) {
    case CCAP_CONVERT_BACKEND_AUTO:
        cppBackend = ccap::ConvertBackend::AUTO;
        break;
    case CCAP_CONVERT_BACKEND_CPU:
        cppBackend = ccap::ConvertBackend::CPU;
        break;
    case CCAP_CONVERT_BACKEND_AVX2:
        cppBackend = ccap::ConvertBackend::AVX2;
        break;
    case CCAP_CONVERT_BACKEND_APPLE_ACCELERATE:
        cppBackend = ccap::ConvertBackend::AppleAccelerate;
        break;
    case CCAP_CONVERT_BACKEND_NEON:
        cppBackend = ccap::ConvertBackend::NEON;
        break;
    default:
        return false;
    }
    return ccap::setConvertBackend(cppBackend);
}

/* ========== Helper function to convert flags ========== */

static ccap::ConvertFlag convertFlags(CcapConvertFlag flag) {
    ccap::ConvertFlag cppFlag = static_cast<ccap::ConvertFlag>(0);

    if (flag & CCAP_CONVERT_FLAG_BT601) {
        cppFlag = cppFlag | ccap::ConvertFlag::BT601;
    }
    if (flag & CCAP_CONVERT_FLAG_BT709) {
        cppFlag = cppFlag | ccap::ConvertFlag::BT709;
    }
    if (flag & CCAP_CONVERT_FLAG_FULL_RANGE) {
        cppFlag = cppFlag | ccap::ConvertFlag::FullRange;
    }
    if (flag & CCAP_CONVERT_FLAG_VIDEO_RANGE) {
        cppFlag = cppFlag | ccap::ConvertFlag::VideoRange;
    }

    return cppFlag;
}

/* ========== Color Space Conversion Functions ========== */

void ccap_convert_yuv_to_rgb_601v(int y, int u, int v, int* r, int* g, int* b) {
    if (!r || !g || !b) return;
    ccap::yuv2rgb601v(y, u, v, *r, *g, *b);
}

void ccap_convert_yuv_to_rgb_709v(int y, int u, int v, int* r, int* g, int* b) {
    if (!r || !g || !b) return;
    ccap::yuv2rgb709v(y, u, v, *r, *g, *b);
}

void ccap_convert_yuv_to_rgb_601f(int y, int u, int v, int* r, int* g, int* b) {
    if (!r || !g || !b) return;
    ccap::yuv2rgb601f(y, u, v, *r, *g, *b);
}

void ccap_convert_yuv_to_rgb_709f(int y, int u, int v, int* r, int* g, int* b) {
    if (!r || !g || !b) return;
    ccap::yuv2rgb709f(y, u, v, *r, *g, *b);
}

/* ========== Color Channel Shuffling ========== */

void ccap_convert_rgba_to_bgra(const uint8_t* src, int src_stride,
                               uint8_t* dst, int dst_stride,
                               int width, int height) {
    if (!src || !dst) return;
    ccap::rgbaToBgra(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_bgra_to_rgba(const uint8_t* src, int src_stride,
                               uint8_t* dst, int dst_stride,
                               int width, int height) {
    if (!src || !dst) return;
    ccap::bgraToRgba(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_rgba_to_bgr(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::rgbaToBgr(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_bgra_to_rgb(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::bgraToRgb(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_rgba_to_rgb(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::rgbaToRgb(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_bgra_to_bgr(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::bgra2bgr(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_rgb_to_bgra(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::rgbToBgra(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_bgr_to_rgba(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::bgrToRgba(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_rgb_to_rgba(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::rgbToRgba(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_bgr_to_bgra(const uint8_t* src, int src_stride,
                              uint8_t* dst, int dst_stride,
                              int width, int height) {
    if (!src || !dst) return;
    ccap::bgrToBgra(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_rgb_to_bgr(const uint8_t* src, int src_stride,
                             uint8_t* dst, int dst_stride,
                             int width, int height) {
    if (!src || !dst) return;
    ccap::rgbToBgr(src, src_stride, dst, dst_stride, width, height);
}

void ccap_convert_bgr_to_rgb(const uint8_t* src, int src_stride,
                             uint8_t* dst, int dst_stride,
                             int width, int height) {
    if (!src || !dst) return;
    ccap::bgrToRgb(src, src_stride, dst, dst_stride, width, height);
}

/* ========== YUV to RGB Conversions ========== */

void ccap_convert_nv12_to_bgr24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_uv, int src_uv_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_uv || !dst) return;
    ccap::nv12ToBgr24(src_y, src_y_stride, src_uv, src_uv_stride,
                      dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_nv12_to_rgb24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_uv, int src_uv_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_uv || !dst) return;
    ccap::nv12ToRgb24(src_y, src_y_stride, src_uv, src_uv_stride,
                      dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_nv12_to_bgra32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_uv, int src_uv_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_uv || !dst) return;
    ccap::nv12ToBgra32(src_y, src_y_stride, src_uv, src_uv_stride,
                       dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_nv12_to_rgba32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_uv, int src_uv_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_uv || !dst) return;
    ccap::nv12ToRgba32(src_y, src_y_stride, src_uv, src_uv_stride,
                       dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_i420_to_bgr24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_u, int src_u_stride,
                                const uint8_t* src_v, int src_v_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_u || !src_v || !dst) return;
    ccap::i420ToBgr24(src_y, src_y_stride, src_u, src_u_stride, src_v, src_v_stride,
                      dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_i420_to_rgb24(const uint8_t* src_y, int src_y_stride,
                                const uint8_t* src_u, int src_u_stride,
                                const uint8_t* src_v, int src_v_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_u || !src_v || !dst) return;
    ccap::i420ToRgb24(src_y, src_y_stride, src_u, src_u_stride, src_v, src_v_stride,
                      dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_i420_to_bgra32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_u, int src_u_stride,
                                 const uint8_t* src_v, int src_v_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_u || !src_v || !dst) return;
    ccap::i420ToBgra32(src_y, src_y_stride, src_u, src_u_stride, src_v, src_v_stride,
                       dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_i420_to_rgba32(const uint8_t* src_y, int src_y_stride,
                                 const uint8_t* src_u, int src_u_stride,
                                 const uint8_t* src_v, int src_v_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src_y || !src_u || !src_v || !dst) return;
    ccap::i420ToRgba32(src_y, src_y_stride, src_u, src_u_stride, src_v, src_v_stride,
                       dst, dst_stride, width, height, convertFlags(flag));
}

/* ========== YUYV (YUV 4:2:2 packed) Conversions ========== */

void ccap_convert_yuyv_to_bgr24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::yuyvToBgr24(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_yuyv_to_rgb24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::yuyvToRgb24(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_yuyv_to_bgra32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::yuyvToBgra32(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_yuyv_to_rgba32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::yuyvToRgba32(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

/* ========== UYVY (YUV 4:2:2 packed) Conversions ========== */

void ccap_convert_uyvy_to_bgr24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::uyvyToBgr24(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_uyvy_to_rgb24(const uint8_t* src, int src_stride,
                                uint8_t* dst, int dst_stride,
                                int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::uyvyToRgb24(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_uyvy_to_bgra32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::uyvyToBgra32(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

void ccap_convert_uyvy_to_rgba32(const uint8_t* src, int src_stride,
                                 uint8_t* dst, int dst_stride,
                                 int width, int height, CcapConvertFlag flag) {
    if (!src || !dst) return;
    ccap::uyvyToRgba32(src, src_stride, dst, dst_stride, width, height, convertFlags(flag));
}

} // extern "C"