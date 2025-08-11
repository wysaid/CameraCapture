/**
 * @file ccap_convert_neon.cpp
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#include "ccap_convert_neon.h"

#include "ccap_convert.h"

#include <cstring>

namespace ccap {
bool hasNEON() {
#if ENABLE_NEON_IMP
    static bool s_hasNEON = hasNEON_();
    return s_hasNEON;
#else
    return false;
#endif
}

#if ENABLE_NEON_IMP

template <int inputChannels, int outputChannels, int swapRB>
void colorShuffle_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height) {
    // Implement a general colorShuffle, accelerated by NEON
    static_assert((inputChannels == 3 || inputChannels == 4) &&
                      (outputChannels == 3 || outputChannels == 4),
                  "inputChannels and outputChannels must be 3 or 4");

    // 基于 swapRB 参数生成索引（仅用于标量尾部和 vld/vst 寄存器交换）
    constexpr int idxR = swapRB ? 2 : 0;
    constexpr int idxG = 1;
    constexpr int idxB = swapRB ? 0 : 2;
    constexpr int idxA = 3;

    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // 1) 主循环：一次处理 16 像素（128bit 寄存器宽度）
        if constexpr (inputChannels == 4 && outputChannels == 4) {
            for (; x + 16 <= width; x += 16) {
                uint8x16x4_t rgba = vld4q_u8(srcRow + x * 4);
                if constexpr (swapRB) {
                    // 交换 R/B
                    uint8x16_t tmp = rgba.val[0];
                    rgba.val[0] = rgba.val[2];
                    rgba.val[2] = tmp;
                }
                vst4q_u8(dstRow + x * 4, rgba);
            }
        } else if constexpr (inputChannels == 3 && outputChannels == 3) {
            for (; x + 16 <= width; x += 16) {
                uint8x16x3_t rgb = vld3q_u8(srcRow + x * 3);
                if constexpr (swapRB) {
                    uint8x16_t tmp = rgb.val[0];
                    rgb.val[0] = rgb.val[2];
                    rgb.val[2] = tmp;
                }
                vst3q_u8(dstRow + x * 3, rgb);
            }
        } else if constexpr (inputChannels == 3 && outputChannels == 4) {
            for (; x + 16 <= width; x += 16) {
                uint8x16x3_t rgb = vld3q_u8(srcRow + x * 3);
                if constexpr (swapRB) {
                    uint8x16_t tmp = rgb.val[0];
                    rgb.val[0] = rgb.val[2];
                    rgb.val[2] = tmp;
                }
                uint8x16_t a = vdupq_n_u8(255);
                uint8x16x4_t rgba;
                rgba.val[0] = rgb.val[0];
                rgba.val[1] = rgb.val[1];
                rgba.val[2] = rgb.val[2];
                rgba.val[3] = a;
                vst4q_u8(dstRow + x * 4, rgba);
            }
        } else { // inputChannels == 4 && outputChannels == 3
            for (; x + 16 <= width; x += 16) {
                uint8x16x4_t rgba = vld4q_u8(srcRow + x * 4);
                // 生成 RGB/BGR 三通道
                uint8x16x3_t rgb;
                if constexpr (swapRB) {
                    rgb.val[0] = rgba.val[2];
                    rgb.val[1] = rgba.val[1];
                    rgb.val[2] = rgba.val[0];
                } else {
                    rgb.val[0] = rgba.val[0];
                    rgb.val[1] = rgba.val[1];
                    rgb.val[2] = rgba.val[2];
                }
                vst3q_u8(dstRow + x * 3, rgb);
            }
        }

        // 2) 次级循环：一次处理 8 像素，减少尾部标量比例
        if constexpr (inputChannels == 4 && outputChannels == 4) {
            for (; x + 8 <= width; x += 8) {
                uint8x8x4_t rgba = vld4_u8(srcRow + x * 4);
                if constexpr (swapRB) {
                    uint8x8_t tmp = rgba.val[0];
                    rgba.val[0] = rgba.val[2];
                    rgba.val[2] = tmp;
                }
                vst4_u8(dstRow + x * 4, rgba);
            }
        } else if constexpr (inputChannels == 3 && outputChannels == 3) {
            for (; x + 8 <= width; x += 8) {
                uint8x8x3_t rgb = vld3_u8(srcRow + x * 3);
                if constexpr (swapRB) {
                    uint8x8_t tmp = rgb.val[0];
                    rgb.val[0] = rgb.val[2];
                    rgb.val[2] = tmp;
                }
                vst3_u8(dstRow + x * 3, rgb);
            }
        } else if constexpr (inputChannels == 3 && outputChannels == 4) {
            for (; x + 8 <= width; x += 8) {
                uint8x8x3_t rgb = vld3_u8(srcRow + x * 3);
                if constexpr (swapRB) {
                    uint8x8_t tmp = rgb.val[0];
                    rgb.val[0] = rgb.val[2];
                    rgb.val[2] = tmp;
                }
                uint8x8_t a = vdup_n_u8(255);
                uint8x8x4_t rgba;
                rgba.val[0] = rgb.val[0];
                rgba.val[1] = rgb.val[1];
                rgba.val[2] = rgb.val[2];
                rgba.val[3] = a;
                vst4_u8(dstRow + x * 4, rgba);
            }
        } else { // inputChannels == 4 && outputChannels == 3
            for (; x + 8 <= width; x += 8) {
                uint8x8x4_t rgba = vld4_u8(srcRow + x * 4);
                uint8x8x3_t rgb;
                if constexpr (swapRB) {
                    rgb.val[0] = rgba.val[2];
                    rgb.val[1] = rgba.val[1];
                    rgb.val[2] = rgba.val[0];
                } else {
                    rgb.val[0] = rgba.val[0];
                    rgb.val[1] = rgba.val[1];
                    rgb.val[2] = rgba.val[2];
                }
                vst3_u8(dstRow + x * 3, rgb);
            }
        }

        // 3) 标量尾部处理
        for (; x < width; ++x) {
            if constexpr (outputChannels == 4 && inputChannels == 4) {
                const uint8_t* s = srcRow + x * 4;
                uint8_t* d = dstRow + x * 4;
                d[0] = s[idxR];
                d[1] = s[idxG];
                d[2] = s[idxB];
                d[3] = s[idxA];
            } else if constexpr (inputChannels == 3 && outputChannels == 3) {
                const uint8_t* s = srcRow + x * 3;
                uint8_t* d = dstRow + x * 3;
                d[0] = s[idxR];
                d[1] = s[idxG];
                d[2] = s[idxB];
            } else if constexpr (inputChannels == 3 && outputChannels == 4) {
                const uint8_t* s = srcRow + x * 3;
                uint8_t* d = dstRow + x * 4;
                d[0] = s[idxR];
                d[1] = s[idxG];
                d[2] = s[idxB];
                d[3] = 255;
            } else { // inputChannels == 4 && outputChannels == 3
                const uint8_t* s = srcRow + x * 4;
                uint8_t* d = dstRow + x * 3;
                d[0] = s[idxR];
                d[1] = s[idxG];
                d[2] = s[idxB];
            }
        }
    }
}

// Explicit template instantiations
template void colorShuffle_neon<4, 4, 0>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

template void colorShuffle_neon<4, 4, 1>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

template void colorShuffle_neon<4, 3, 0>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

template void colorShuffle_neon<4, 3, 1>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

template void colorShuffle_neon<3, 4, 0>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

template void colorShuffle_neon<3, 4, 1>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

template void colorShuffle_neon<3, 3, 0>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

template void colorShuffle_neon<3, 3, 1>(const uint8_t* src, int srcStride,
                                         uint8_t* dst, int dstStride,
                                         int width, int height);

///////////// YUV to RGB conversion functions /////////////

// Forward declaration of the canonical fixed-point coefficients provider (×64)
inline void getYuvToRgbCoefficients_neon(bool isBT601, bool isFullRange,
                                         int& cy, int& cr, int& cgu, int& cgv,
                                         int& cb, int& y_offset);

// Helper overload: get fixed-point (×64) coefficients from flag directly
inline void getYuvToRgbCoefficients_neon(ConvertFlag flag,
                                         int& cy, int& cr, int& cgu, int& cgv,
                                         int& cb, int& y_offset) {
    const bool isBT601 = (flag & ConvertFlag::BT601);
    const bool isFullRange = (flag & ConvertFlag::FullRange);
    getYuvToRgbCoefficients_neon(isBT601, isFullRange, cy, cr, cgu, cgv, cb, y_offset);
}

// 标量尾部：使用整数定点系数（×64）和 +32 >> 6 的舍入，并显式 clamp
inline void yuv2rgbGeneric_int(int y, int u, int v, int& r, int& g, int& b,
                               int cy, int cr, int cgu, int cgv, int cb) {
    // y,u,v 已经完成偏移（y-y_offset，u-128，v-128）
    int fr = (cy * y + cr * v + 32) >> 6;
    int fg = (cy * y - cgu * u - cgv * v + 32) >> 6;
    int fb = (cy * y + cb * u + 32) >> 6;
    // clamp 到 [0,255]
    r = fr < 0 ? 0 : (fr > 255 ? 255 : fr);
    g = fg < 0 ? 0 : (fg > 255 ? 255 : fg);
    b = fb < 0 ? 0 : (fb > 255 ? 255 : fb);
}

inline void getYuvToRgbCoefficients_neon(bool isBT601, bool isFullRange, int& cy, int& cr, int& cgu, int& cgv, int& cb, int& y_offset) {
    if (isBT601) {
        if (isFullRange) { // BT.601 Full Range: 256, 351, 86, 179, 443 (divided by 4)
            cy = 64;
            cr = 88;
            cgu = 22;
            cgv = 45;
            cb = 111;
            y_offset = 0;
        } else { // BT.601 Video Range: 298, 409, 100, 208, 516 (divided by 4)
            cy = 75;
            cr = 102;
            cgu = 25;
            cgv = 52;
            cb = 129;
            y_offset = 16;
        }
    } else {
        if (isFullRange) { // BT.709 Full Range: 256, 403, 48, 120, 475 (divided by 4)
            cy = 64;
            cr = 101;
            cgu = 12;
            cgv = 30;
            cb = 119;
            y_offset = 0;
        } else { // BT.709 Video Range: 298, 459, 55, 136, 541 (divided by 4)
            cy = 75;
            cr = 115;
            cgu = 14;
            cgv = 34;
            cb = 135;
            y_offset = 16;
        }
    }
}

template <bool isBGRA>
void nv12ToRgbaColor_neon_imp(const uint8_t* srcY, int srcYStride,
                              const uint8_t* srcUV, int srcUVStride,
                              uint8_t* dst, int dstStride,
                              int width, int height, bool is601, bool isFullRange) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Get coefficients based on color space and range
    int cy, cr, cgu, cgv, cb, y_offset;
    getYuvToRgbCoefficients_neon(is601, isFullRange, cy, cr, cgu, cgv, cb, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            uint8x16_t y_vals = vld1q_u8(yRow + x);

            // 2. Load 16 bytes UV (8 UV pairs for 16 pixels)
            uint8x16_t uv_vals = vld1q_u8(uvRow + x);

            // 3. Deinterleave U and V (NV12 format: UVUVUV...)
            uint8x8x2_t uv_deint = vuzp_u8(vget_low_u8(uv_vals), vget_high_u8(uv_vals));
            uint8x8_t u_vals = uv_deint.val[0]; // U: 0,2,4,6...
            uint8x8_t v_vals = uv_deint.val[1]; // V: 1,3,5,7...

            // 4. Duplicate each U and V value for 2 pixels (since UV is subsampled)
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // 5. Convert to 16-bit and apply offsets
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 6. Dynamic conversion constants based on color space and range
            int16x8_t c_y = vdupq_n_s16(cy);
            int16x8_t c_r = vdupq_n_s16(cr);
            int16x8_t c_gu = vdupq_n_s16(cgu);
            int16x8_t c_gv = vdupq_n_s16(cgv);
            int16x8_t c_b = vdupq_n_s16(cb);
            int16x8_t c32 = vdupq_n_s16(32);

            // 7. Calculate R, G, B for low 8 pixels
            int16x8_t y_scaled_lo = vmulq_s16(y_lo, c_y);
            int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, c_r));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, c_gu));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c_gv));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, c_b));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 8. Calculate R, G, B for high 8 pixels
            int16x8_t y_scaled_hi = vmulq_s16(y_hi, c_y);
            int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, c_r));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, c_gu));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c_gv));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, c_b));
            b_hi = vshrq_n_s16(vaddq_s16(b_hi, c32), 6);

            // 9. Clamp and convert back to 8-bit
            uint8x8_t r8_lo = vqmovun_s16(r_lo);
            uint8x8_t g8_lo = vqmovun_s16(g_lo);
            uint8x8_t b8_lo = vqmovun_s16(b_lo);
            uint8x8_t r8_hi = vqmovun_s16(r_hi);
            uint8x8_t g8_hi = vqmovun_s16(g_hi);
            uint8x8_t b8_hi = vqmovun_s16(b_hi);

            uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
            uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
            uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);
            uint8x16_t a8 = vdupq_n_u8(255);

            // 10. Interleave and store RGBA/BGRA
            if constexpr (isBGRA) {
                uint8x16x4_t bgra;
                bgra.val[0] = b8;
                bgra.val[1] = g8;
                bgra.val[2] = r8;
                bgra.val[3] = a8;
                vst4q_u8(dstRow + x * 4, bgra);
            } else {
                uint8x16x4_t rgba;
                rgba.val[0] = r8;
                rgba.val[1] = g8;
                rgba.val[2] = b8;
                rgba.val[3] = a8;
                vst4q_u8(dstRow + x * 4, rgba);
            }
        }

        // Process remaining pixels using dynamic coefficients
        for (; x < width; x += 2) {
            int y0 = yRow[x] - y_offset;
            int y1 = (x + 1 < width) ? yRow[x + 1] - y_offset : y0;
            int u = uvRow[x] - 128;     // U at even positions
            int v = uvRow[x + 1] - 128; // V at odd positions

            // Convert using dynamic coefficients
            int r0 = (cy * y0 + cr * v) >> 6;
            int g0 = (cy * y0 - cgu * u - cgv * v) >> 6;
            int b0 = (cy * y0 + cb * u) >> 6;

            int r1 = (cy * y1 + cr * v) >> 6;
            int g1 = (cy * y1 - cgu * u - cgv * v) >> 6;
            int b1 = (cy * y1 + cb * u) >> 6;

            // Clamp to [0, 255]
            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            r1 = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
            g1 = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
            b1 = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);

            if constexpr (isBGRA) {
                dstRow[x * 4 + 0] = b0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = r0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 4 + 0] = b1;
                    dstRow[(x + 1) * 4 + 1] = g1;
                    dstRow[(x + 1) * 4 + 2] = r1;
                    dstRow[(x + 1) * 4 + 3] = 255;
                }
            } else {
                dstRow[x * 4 + 0] = r0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = b0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 4 + 0] = r1;
                    dstRow[(x + 1) * 4 + 1] = g1;
                    dstRow[(x + 1) * 4 + 2] = b1;
                    dstRow[(x + 1) * 4 + 3] = 255;
                }
            }
        }
    }
}

template <bool isBGR>
void _nv12ToRgbColor_neon_imp(const uint8_t* srcY, int srcYStride,
                              const uint8_t* srcUV, int srcUVStride,
                              uint8_t* dst, int dstStride,
                              int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    // Get coefficients based on color space and range
    int cy, cr, cgu, cgv, cb, y_offset;
    getYuvToRgbCoefficients_neon(is601, isFullRange, cy, cr, cgu, cgv, cb, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            uint8x16_t y_vals = vld1q_u8(yRow + x);

            // 2. Load 16 bytes UV (8 UV pairs for 16 pixels)
            uint8x16_t uv_vals = vld1q_u8(uvRow + x);

            // 3. Deinterleave U and V (NV12 format: UVUVUV...)
            uint8x8x2_t uv_deint = vuzp_u8(vget_low_u8(uv_vals), vget_high_u8(uv_vals));
            uint8x8_t u_vals = uv_deint.val[0]; // U: 0,2,4,6...
            uint8x8_t v_vals = uv_deint.val[1]; // V: 1,3,5,7...

            // 4. Duplicate each U and V value for 2 pixels (since UV is subsampled)
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // 5. Convert to 16-bit and apply offsets
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 6. Dynamic conversion constants based on color space and range
            int16x8_t c_y = vdupq_n_s16(cy);
            int16x8_t c_r = vdupq_n_s16(cr);
            int16x8_t c_gu = vdupq_n_s16(cgu);
            int16x8_t c_gv = vdupq_n_s16(cgv);
            int16x8_t c_b = vdupq_n_s16(cb);

            // 7. Apply conversion (Y * cy + U/V * coeffs) >> 6
            int16x8_t r_lo = vaddq_s16(vmulq_s16(c_y, y_lo), vmulq_s16(c_r, v_lo));
            int16x8_t r_hi = vaddq_s16(vmulq_s16(c_y, y_hi), vmulq_s16(c_r, v_hi));

            int16x8_t g_lo = vsubq_s16(vmulq_s16(c_y, y_lo), vaddq_s16(vmulq_s16(c_gu, u_lo), vmulq_s16(c_gv, v_lo)));
            int16x8_t g_hi = vsubq_s16(vmulq_s16(c_y, y_hi), vaddq_s16(vmulq_s16(c_gu, u_hi), vmulq_s16(c_gv, v_hi)));

            int16x8_t b_lo = vaddq_s16(vmulq_s16(c_y, y_lo), vmulq_s16(c_b, u_lo));
            int16x8_t b_hi = vaddq_s16(vmulq_s16(c_y, y_hi), vmulq_s16(c_b, u_hi));

            // 8. Arithmetic right shift by 6 (divide by 64)
            r_lo = vshrq_n_s16(r_lo, 6);
            r_hi = vshrq_n_s16(r_hi, 6);
            g_lo = vshrq_n_s16(g_lo, 6);
            g_hi = vshrq_n_s16(g_hi, 6);
            b_lo = vshrq_n_s16(b_lo, 6);
            b_hi = vshrq_n_s16(b_hi, 6);

            // 9. Clamp and convert back to 8-bit
            uint8x8_t r8_lo = vqmovun_s16(r_lo);
            uint8x8_t g8_lo = vqmovun_s16(g_lo);
            uint8x8_t b8_lo = vqmovun_s16(b_lo);
            uint8x8_t r8_hi = vqmovun_s16(r_hi);
            uint8x8_t g8_hi = vqmovun_s16(g_hi);
            uint8x8_t b8_hi = vqmovun_s16(b_hi);

            uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
            uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
            uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);

            // 10. Interleave and store RGB/BGR
            if constexpr (isBGR) {
                uint8x16x3_t bgr;
                bgr.val[0] = b8;
                bgr.val[1] = g8;
                bgr.val[2] = r8;
                vst3q_u8(dstRow + x * 3, bgr);
            } else {
                uint8x16x3_t rgb;
                rgb.val[0] = r8;
                rgb.val[1] = g8;
                rgb.val[2] = b8;
                vst3q_u8(dstRow + x * 3, rgb);
            }
        }

        // Process remaining pixels using dynamic coefficients
        for (; x < width; x += 2) {
            int y0 = yRow[x] - y_offset;
            int y1 = (x + 1 < width) ? yRow[x + 1] - y_offset : y0;
            int u = uvRow[x] - 128;     // U at even positions
            int v = uvRow[x + 1] - 128; // V at odd positions

            // Convert using dynamic coefficients
            int r0 = (cy * y0 + cr * v) >> 6;
            int g0 = (cy * y0 - cgu * u - cgv * v) >> 6;
            int b0 = (cy * y0 + cb * u) >> 6;

            int r1 = (cy * y1 + cr * v) >> 6;
            int g1 = (cy * y1 - cgu * u - cgv * v) >> 6;
            int b1 = (cy * y1 + cb * u) >> 6;

            // Clamp to [0, 255]
            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            r1 = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
            g1 = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
            b1 = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);

            if constexpr (isBGR) {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 3 + 0] = b1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = r1;
                }
            } else {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 3 + 0] = r1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = b1;
                }
            }
        }
    }
}

template <bool isBGRA>
void _i420ToRgba_neon_imp(const uint8_t* srcY, int srcYStride,
                          const uint8_t* srcU, int srcUStride,
                          const uint8_t* srcV, int srcVStride,
                          uint8_t* dst, int dstStride,
                          int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // 获取整数定点（×64）系数
    int cy, cr, cgu, cgv, cb, y_offset;
    getYuvToRgbCoefficients_neon(flag, cy, cr, cgu, cgv, cb, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            uint8x16_t y_vals = vld1q_u8(yRow + x);

            // 2. Load 8 U and 8 V values
            uint8x8_t u_vals = vld1_u8(uRow + x / 2);
            uint8x8_t v_vals = vld1_u8(vRow + x / 2);

            // 3. Duplicate each U and V value for 2 pixels
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // 4. Convert to 16-bit and apply offsets
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 5. 使用整数定点（×64）系数
            int16x8_t cy_coeff = vdupq_n_s16(static_cast<int16_t>(cy));
            int16x8_t cr_coeff = vdupq_n_s16(static_cast<int16_t>(cr));
            int16x8_t cgu_coeff = vdupq_n_s16(static_cast<int16_t>(cgu));
            int16x8_t cgv_coeff = vdupq_n_s16(static_cast<int16_t>(cgv));
            int16x8_t cb_coeff = vdupq_n_s16(static_cast<int16_t>(cb));
            int16x8_t c32 = vdupq_n_s16(32);

            // 6. Calculate R, G, B for low 8 pixels
            int16x8_t y_scaled_lo = vmulq_s16(y_lo, cy_coeff);
            int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, cr_coeff));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, cgu_coeff));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, cgv_coeff));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, cb_coeff));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 7. Calculate R, G, B for high 8 pixels
            int16x8_t y_scaled_hi = vmulq_s16(y_hi, cy_coeff);
            int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, cr_coeff));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, cgu_coeff));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, cgv_coeff));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, cb_coeff));
            b_hi = vshrq_n_s16(vaddq_s16(b_hi, c32), 6);

            // 8. Clamp and convert back to 8-bit
            uint8x8_t r8_lo = vqmovun_s16(r_lo);
            uint8x8_t g8_lo = vqmovun_s16(g_lo);
            uint8x8_t b8_lo = vqmovun_s16(b_lo);
            uint8x8_t r8_hi = vqmovun_s16(r_hi);
            uint8x8_t g8_hi = vqmovun_s16(g_hi);
            uint8x8_t b8_hi = vqmovun_s16(b_hi);

            uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
            uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
            uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);
            uint8x16_t a8 = vdupq_n_u8(255);

            // 9. Interleave and store RGBA/BGRA
            if constexpr (isBGRA) {
                uint8x16x4_t bgra;
                bgra.val[0] = b8;
                bgra.val[1] = g8;
                bgra.val[2] = r8;
                bgra.val[3] = a8;
                vst4q_u8(dstRow + x * 4, bgra);
            } else {
                uint8x16x4_t rgba;
                rgba.val[0] = r8;
                rgba.val[1] = g8;
                rgba.val[2] = b8;
                rgba.val[3] = a8;
                vst4q_u8(dstRow + x * 4, rgba);
            }
        }

        // Process remaining pixels
        for (; x < width; x += 2) {
            int y0 = yRow[x] - y_offset;
            int y1 = (x + 1 < width) ? yRow[x + 1] - y_offset : y0;
            int u = uRow[x / 2] - 128;
            int v = vRow[x / 2] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgbGeneric_int(y0, u, v, r0, g0, b0, cy, cr, cgu, cgv, cb);
            yuv2rgbGeneric_int(y1, u, v, r1, g1, b1, cy, cr, cgu, cgv, cb);

            if constexpr (isBGRA) {
                dstRow[x * 4 + 0] = b0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = r0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 4 + 0] = b1;
                    dstRow[(x + 1) * 4 + 1] = g1;
                    dstRow[(x + 1) * 4 + 2] = r1;
                    dstRow[(x + 1) * 4 + 3] = 255;
                }
            } else {
                dstRow[x * 4 + 0] = r0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = b0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 4 + 0] = r1;
                    dstRow[(x + 1) * 4 + 1] = g1;
                    dstRow[(x + 1) * 4 + 2] = b1;
                    dstRow[(x + 1) * 4 + 3] = 255;
                }
            }
        }
    }
}

template <bool isBGR>
void _i420ToRgb_neon_imp(const uint8_t* srcY, int srcYStride,
                         const uint8_t* srcU, int srcUStride,
                         const uint8_t* srcV, int srcVStride,
                         uint8_t* dst, int dstStride,
                         int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // 获取整数定点（×64）系数
    int cy, cr, cgu, cgv, cb, y_offset;
    getYuvToRgbCoefficients_neon(flag, cy, cr, cgu, cgv, cb, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            uint8x16_t y_vals = vld1q_u8(yRow + x);

            // 2. Load 8 U and 8 V values
            uint8x8_t u_vals = vld1_u8(uRow + x / 2);
            uint8x8_t v_vals = vld1_u8(vRow + x / 2);

            // 3. Duplicate each U and V value for 2 pixels
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // 4. Convert to 16-bit and apply offsets
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 5. 使用整数定点（×64）系数
            int16x8_t cy_coeff = vdupq_n_s16(static_cast<int16_t>(cy));
            int16x8_t cr_coeff = vdupq_n_s16(static_cast<int16_t>(cr));
            int16x8_t cgu_coeff = vdupq_n_s16(static_cast<int16_t>(cgu));
            int16x8_t cgv_coeff = vdupq_n_s16(static_cast<int16_t>(cgv));
            int16x8_t cb_coeff = vdupq_n_s16(static_cast<int16_t>(cb));
            int16x8_t c32 = vdupq_n_s16(32);

            // 6. Calculate R, G, B for low 8 pixels
            int16x8_t y_scaled_lo = vmulq_s16(y_lo, cy_coeff);
            int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, cr_coeff));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, cgu_coeff));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, cgv_coeff));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, cb_coeff));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 7. Calculate R, G, B for high 8 pixels
            int16x8_t y_scaled_hi = vmulq_s16(y_hi, cy_coeff);
            int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, cr_coeff));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, cgu_coeff));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, cgv_coeff));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, cb_coeff));
            b_hi = vshrq_n_s16(vaddq_s16(b_hi, c32), 6);

            // 8. Clamp and convert back to 8-bit
            uint8x8_t r8_lo = vqmovun_s16(r_lo);
            uint8x8_t g8_lo = vqmovun_s16(g_lo);
            uint8x8_t b8_lo = vqmovun_s16(b_lo);
            uint8x8_t r8_hi = vqmovun_s16(r_hi);
            uint8x8_t g8_hi = vqmovun_s16(g_hi);
            uint8x8_t b8_hi = vqmovun_s16(b_hi);

            uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
            uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
            uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);

            // 9. Store RGB24 data with interleaved NEON store
            uint8x16x3_t interleaved;
            if constexpr (isBGR) {
                interleaved.val[0] = b8;
                interleaved.val[1] = g8;
                interleaved.val[2] = r8;
            } else {
                interleaved.val[0] = r8;
                interleaved.val[1] = g8;
                interleaved.val[2] = b8;
            }
            vst3q_u8(dstRow + x * 3, interleaved);
        }

        // Process remaining pixels
        for (; x < width; x += 2) {
            int y0 = yRow[x] - y_offset;
            int y1 = (x + 1 < width) ? yRow[x + 1] - y_offset : y0;
            int u = uRow[x / 2] - 128;
            int v = vRow[x / 2] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgbGeneric_int(y0, u, v, r0, g0, b0, cy, cr, cgu, cgv, cb);
            yuv2rgbGeneric_int(y1, u, v, r1, g1, b1, cy, cr, cgu, cgv, cb);

            if constexpr (isBGR) {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 3 + 0] = b1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = r1;
                }
            } else {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 3 + 0] = r1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = b1;
                }
            }
        }
    }
}

// NEON accelerated conversion functions
void nv12ToBgra32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;
    nv12ToRgbaColor_neon_imp<true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601, isFullRange);
}

void nv12ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;
    nv12ToRgbaColor_neon_imp<false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601, isFullRange);
}

void nv12ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _nv12ToRgbColor_neon_imp<true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void nv12ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _nv12ToRgbColor_neon_imp<false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void i420ToBgra32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _i420ToRgba_neon_imp<true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _i420ToRgba_neon_imp<false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _i420ToRgb_neon_imp<true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _i420ToRgb_neon_imp<false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

///////////// YUYV/UYVY to RGB conversion functions /////////////

template <bool isBGRA>
void _yuyvToRgba_neon_imp(const uint8_t* src, int srcStride,
                          uint8_t* dst, int dstStride,
                          int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    // Get coefficients based on color space and range
    int cy, cr, cgu, cgv, cb, y_offset;
    getYuvToRgbCoefficients_neon(is601, isFullRange, cy, cr, cgu, cgv, cb, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time (32 bytes of YUYV data)
        for (; x + 16 <= width; x += 16) {
            // Load 32 bytes of YUYV data: Y0U0Y1V0 Y2U1Y3V1 ...
            uint8x16x2_t yuyv_data = vld2q_u8(srcRow + x * 2);
            uint8x16_t y_vals = yuyv_data.val[0];  // Y0 Y1 Y2 Y3 ...
            uint8x16_t uv_vals = yuyv_data.val[1]; // U0 V0 U1 V1 ...

            // Deinterleave U and V
            uint8x8x2_t uv_deint = vuzp_u8(vget_low_u8(uv_vals), vget_high_u8(uv_vals));
            uint8x8_t u_vals = uv_deint.val[0];
            uint8x8_t v_vals = uv_deint.val[1];

            // Duplicate U and V values for each pair of pixels
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // Convert to 16-bit and apply offsets
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(y_offset)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // Dynamic conversion constants based on color space and range
            int16x8_t c_y = vdupq_n_s16(cy);
            int16x8_t c_r = vdupq_n_s16(cr);
            int16x8_t c_gu = vdupq_n_s16(cgu);
            int16x8_t c_gv = vdupq_n_s16(cgv);
            int16x8_t c_b = vdupq_n_s16(cb);

            // Apply conversion (Y * cy + U/V * coeffs) >> 6
            int16x8_t r_lo = vaddq_s16(vmulq_s16(c_y, y_lo), vmulq_s16(c_r, v_lo));
            int16x8_t r_hi = vaddq_s16(vmulq_s16(c_y, y_hi), vmulq_s16(c_r, v_hi));

            int16x8_t g_lo = vsubq_s16(vmulq_s16(c_y, y_lo), vaddq_s16(vmulq_s16(c_gu, u_lo), vmulq_s16(c_gv, v_lo)));
            int16x8_t g_hi = vsubq_s16(vmulq_s16(c_y, y_hi), vaddq_s16(vmulq_s16(c_gu, u_hi), vmulq_s16(c_gv, v_hi)));

            int16x8_t b_lo = vaddq_s16(vmulq_s16(c_y, y_lo), vmulq_s16(c_b, u_lo));
            int16x8_t b_hi = vaddq_s16(vmulq_s16(c_y, y_hi), vmulq_s16(c_b, u_hi));

            // Arithmetic right shift by 6 (divide by 64)
            r_lo = vshrq_n_s16(r_lo, 6);
            r_hi = vshrq_n_s16(r_hi, 6);
            g_lo = vshrq_n_s16(g_lo, 6);
            g_hi = vshrq_n_s16(g_hi, 6);
            b_lo = vshrq_n_s16(b_lo, 6);
            b_hi = vshrq_n_s16(b_hi, 6);

            // Clamp and convert back to 8-bit
            uint8x8_t r8_lo = vqmovun_s16(r_lo);
            uint8x8_t g8_lo = vqmovun_s16(g_lo);
            uint8x8_t b8_lo = vqmovun_s16(b_lo);
            uint8x8_t r8_hi = vqmovun_s16(r_hi);
            uint8x8_t g8_hi = vqmovun_s16(g_hi);
            uint8x8_t b8_hi = vqmovun_s16(b_hi);

            uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
            uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
            uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);
            uint8x16_t a8 = vdupq_n_u8(255);

            // Store as RGBA or BGRA
            if constexpr (isBGRA) {
                uint8x16x4_t bgra;
                bgra.val[0] = b8;
                bgra.val[1] = g8;
                bgra.val[2] = r8;
                bgra.val[3] = a8;
                vst4q_u8(dstRow + x * 4, bgra);
            } else {
                uint8x16x4_t rgba;
                rgba.val[0] = r8;
                rgba.val[1] = g8;
                rgba.val[2] = b8;
                rgba.val[3] = a8;
                vst4q_u8(dstRow + x * 4, rgba);
            }
        }

        // Handle remaining pixels using dynamic coefficients
        for (; x < width; x += 2) {
            if (x + 1 >= width) break;

            uint8_t y0 = srcRow[x * 2];
            uint8_t u = srcRow[x * 2 + 1];
            uint8_t y1 = srcRow[x * 2 + 2];
            uint8_t v = srcRow[x * 2 + 3];

            // Apply dynamic offsets
            int c_y0 = (int)y0 - y_offset;
            int c_y1 = (int)y1 - y_offset;
            int c_u = (int)u - 128;
            int c_v = (int)v - 128;

            // Convert using dynamic coefficients
            int r0 = (cy * c_y0 + cr * c_v) >> 6;
            int g0 = (cy * c_y0 - cgu * c_u - cgv * c_v) >> 6;
            int b0 = (cy * c_y0 + cb * c_u) >> 6;

            int r1 = (cy * c_y1 + cr * c_v) >> 6;
            int g1 = (cy * c_y1 - cgu * c_u - cgv * c_v) >> 6;
            int b1 = (cy * c_y1 + cb * c_u) >> 6;

            // Clamp to [0, 255]
            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            r1 = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
            g1 = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
            b1 = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);

            if constexpr (isBGRA) {
                dstRow[x * 4] = b0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = r0;
                dstRow[x * 4 + 3] = 255;
                dstRow[(x + 1) * 4] = b1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = r1;
                dstRow[(x + 1) * 4 + 3] = 255;
            } else {
                dstRow[x * 4] = r0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = b0;
                dstRow[x * 4 + 3] = 255;
                dstRow[(x + 1) * 4] = r1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = b1;
                dstRow[(x + 1) * 4 + 3] = 255;
            }
        }
    }
}

template <bool isBGR>
void _yuyvToRgb_neon_imp(const uint8_t* src, int srcStride,
                         uint8_t* dst, int dstStride,
                         int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Get integer coefficients (scaled by 64) to align with other SIMD paths
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;
    int cy_i, cr_i, cgu_i, cgv_i, cb_i, y_offset;
    getYuvToRgbCoefficients_neon(is601, isFullRange, cy_i, cr_i, cgu_i, cgv_i, cb_i, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON when possible
        if (width >= 16) {
            // Each YUYV pair produces 2 pixels, so 8 YUYV pairs = 16 pixels
            for (; x + 16 <= width; x += 16) {
                // Load 32 bytes = 8 YUYV pairs = 16 pixels
                uint8x16_t yuv1 = vld1q_u8(srcRow + x * 2);      // First 16 bytes
                uint8x16_t yuv2 = vld1q_u8(srcRow + x * 2 + 16); // Second 16 bytes

                // Extract Y values: Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7, Y8, Y9, Y10, Y11, Y12, Y13, Y14, Y15
                uint8x8_t y_low = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                           vcreate_u8(0x0E0C0A0806040200ULL)); // Y indices: 0,2,4,6,8,10,12,14
                uint8x8_t y_high = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                            vcreate_u8(0x0E0C0A0806040200ULL)); // Y indices: 0,2,4,6,8,10,12,14
                uint8x16_t y_vals = vcombine_u8(y_low, y_high);

                // Extract U values and duplicate for 2 pixels each
                uint8x8_t u_packed = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                              vcreate_u8(0x0D0D090905050101ULL)); // U indices: 1,1,5,5,9,9,13,13
                uint8x8_t u_packed2 = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                               vcreate_u8(0x0D0D090905050101ULL)); // U indices: 1,1,5,5,9,9,13,13
                uint8x16_t u_vals = vcombine_u8(u_packed, u_packed2);

                // Extract V values and duplicate for 2 pixels each
                uint8x8_t v_packed = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                              vcreate_u8(0x0F0F0B0B07070303ULL)); // V indices: 3,3,7,7,11,11,15,15
                uint8x8_t v_packed2 = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                               vcreate_u8(0x0F0F0B0B07070303ULL)); // V indices: 3,3,7,7,11,11,15,15
                uint8x16_t v_vals = vcombine_u8(v_packed, v_packed2);

                // Convert to 16-bit with proper signed subtraction (avoid unsigned wrap)
                int16x8_t y_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(y_vals))), vdupq_n_s16(y_offset));
                int16x8_t y_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(y_vals))), vdupq_n_s16(y_offset));
                int16x8_t u_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(u_vals))), vdupq_n_s16(128));
                int16x8_t u_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(u_vals))), vdupq_n_s16(128));
                int16x8_t v_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(v_vals))), vdupq_n_s16(128));
                int16x8_t v_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(v_vals))), vdupq_n_s16(128));

                // Dynamic conversion coefficients (already scaled by 64)
                int16x8_t cy_coeff = vdupq_n_s16(static_cast<int16_t>(cy_i));
                int16x8_t cr_coeff = vdupq_n_s16(static_cast<int16_t>(cr_i));
                int16x8_t cgu_coeff = vdupq_n_s16(static_cast<int16_t>(cgu_i));
                int16x8_t cgv_coeff = vdupq_n_s16(static_cast<int16_t>(cgv_i));
                int16x8_t cb_coeff = vdupq_n_s16(static_cast<int16_t>(cb_i));
                int16x8_t c32 = vdupq_n_s16(32);

                // Calculate R, G, B for low 8 pixels
                int16x8_t y_scaled_lo = vmulq_s16(y_lo, cy_coeff);
                int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, cr_coeff));
                r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

                int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, cgu_coeff));
                g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, cgv_coeff));
                g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

                int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, cb_coeff));
                b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

                // Calculate R, G, B for high 8 pixels
                int16x8_t y_scaled_hi = vmulq_s16(y_hi, cy_coeff);
                int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, cr_coeff));
                r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

                int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, cgu_coeff));
                g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, cgv_coeff));
                g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

                int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, cb_coeff));
                b_hi = vshrq_n_s16(vaddq_s16(b_hi, c32), 6);

                // Clamp and convert back to 8-bit
                uint8x8_t r8_lo = vqmovun_s16(r_lo);
                uint8x8_t g8_lo = vqmovun_s16(g_lo);
                uint8x8_t b8_lo = vqmovun_s16(b_lo);
                uint8x8_t r8_hi = vqmovun_s16(r_hi);
                uint8x8_t g8_hi = vqmovun_s16(g_hi);
                uint8x8_t b8_hi = vqmovun_s16(b_hi);

                uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
                uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
                uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);

                // 使用 NEON 交错存储写回 RGB（vst3q_u8），避免标量循环
                if constexpr (isBGR) {
                    uint8x16x3_t bgr;
                    bgr.val[0] = b8;
                    bgr.val[1] = g8;
                    bgr.val[2] = r8;
                    // 这里 x+16<=width 已由循环条件保证，可安全整块写回
                    vst3q_u8(dstRow + x * 3, bgr);
                } else {
                    uint8x16x3_t rgb;
                    rgb.val[0] = r8;
                    rgb.val[1] = g8;
                    rgb.val[2] = b8;
                    vst3q_u8(dstRow + x * 3, rgb);
                }
            }
        }

        // Process remaining pixels in pairs
        for (; x + 2 <= width; x += 2) {
            uint8_t y0 = srcRow[x * 2];
            uint8_t u = srcRow[x * 2 + 1];
            uint8_t y1 = srcRow[x * 2 + 2];
            uint8_t v = srcRow[x * 2 + 3];

            int y0c = static_cast<int>(y0) - y_offset;
            int y1c = static_cast<int>(y1) - y_offset;
            int uc = static_cast<int>(u) - 128;
            int vc = static_cast<int>(v) - 128;

            int r0 = (cy_i * y0c + cr_i * vc + 32) >> 6;
            int g0 = (cy_i * y0c - cgu_i * uc - cgv_i * vc + 32) >> 6;
            int b0 = (cy_i * y0c + cb_i * uc + 32) >> 6;

            int r1 = (cy_i * y1c + cr_i * vc + 32) >> 6;
            int g1 = (cy_i * y1c - cgu_i * uc - cgv_i * vc + 32) >> 6;
            int b1 = (cy_i * y1c + cb_i * uc + 32) >> 6;

            // Clamp to [0, 255] to match NEON saturating behavior
            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            r1 = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
            g1 = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
            b1 = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);

            if constexpr (isBGR) {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 3 + 0] = b1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = r1;
                }
            } else {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * 3 + 0] = r1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = b1;
                }
            }
        }

        // Handle remaining single pixel (if width is odd)
        if (x < width) {
            uint8_t y0 = srcRow[x * 2];
            uint8_t u = srcRow[x * 2 + 1];
            uint8_t y1 = srcRow[x * 2 + 2];
            uint8_t v = srcRow[x * 2 + 3];

            int y0c = static_cast<int>(y0) - y_offset;
            int uc = static_cast<int>(u) - 128;
            int vc = static_cast<int>(v) - 128;

            int r0 = (cy_i * y0c + cr_i * vc + 32) >> 6;
            int g0 = (cy_i * y0c - cgu_i * uc - cgv_i * vc + 32) >> 6;
            int b0 = (cy_i * y0c + cb_i * uc + 32) >> 6;

            // Clamp to [0, 255]
            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);

            if constexpr (isBGR) {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;
            } else {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;
            }
        }
    }
}

template <bool isBGRA>
void _uyvyToRgba_neon_imp(const uint8_t* src, int srcStride,
                          uint8_t* dst, int dstStride,
                          int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // 使用与 YUYV 相同的整数系数（×64）与动态 y_offset，保持 SIMD/标量一致的舍入规则
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;
    int cy_i, cr_i, cgu_i, cgv_i, cb_i, y_offset;
    getYuvToRgbCoefficients_neon(is601, isFullRange, cy_i, cr_i, cgu_i, cgv_i, cb_i, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // 向量路径：每次处理 16 像素（8 个 UYVY 组 = 32 字节）
        if (width >= 16) {
            for (; x + 16 <= width; x += 16) {
                // 载入 32 字节
                uint8x16_t yuv1 = vld1q_u8(srcRow + x * 2);
                uint8x16_t yuv2 = vld1q_u8(srcRow + x * 2 + 16);

                // 从 UYVY 提取 Y（奇数索引 1,3,...,15）
                uint8x8_t y_low = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                           vcreate_u8(0x0F0D0B0907050301ULL));
                uint8x8_t y_high = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                            vcreate_u8(0x0F0D0B0907050301ULL));
                uint8x16_t y_vals = vcombine_u8(y_low, y_high);

                // 提取并复制 U（索引 0,0,4,4,8,8,12,12）
                uint8x8_t u_packed1 = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                               vcreate_u8(0x0C0C080804040000ULL));
                uint8x8_t u_packed2 = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                               vcreate_u8(0x0C0C080804040000ULL));
                uint8x16_t u_vals = vcombine_u8(u_packed1, u_packed2);

                // 提取并复制 V（索引 2,2,6,6,10,10,14,14）
                uint8x8_t v_packed1 = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                               vcreate_u8(0x0E0E0A0A06060202ULL));
                uint8x8_t v_packed2 = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                               vcreate_u8(0x0E0E0A0A06060202ULL));
                uint8x16_t v_vals = vcombine_u8(v_packed1, v_packed2);

                // 宽化并做带符号偏移
                int16x8_t y_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(y_vals))), vdupq_n_s16(y_offset));
                int16x8_t y_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(y_vals))), vdupq_n_s16(y_offset));
                int16x8_t u_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(u_vals))), vdupq_n_s16(128));
                int16x8_t u_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(u_vals))), vdupq_n_s16(128));
                int16x8_t v_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(v_vals))), vdupq_n_s16(128));
                int16x8_t v_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(v_vals))), vdupq_n_s16(128));

                // 系数（×64）与 +32 >> 6 舍入
                int16x8_t cy_c = vdupq_n_s16(static_cast<int16_t>(cy_i));
                int16x8_t cr_c = vdupq_n_s16(static_cast<int16_t>(cr_i));
                int16x8_t cgu_c = vdupq_n_s16(static_cast<int16_t>(cgu_i));
                int16x8_t cgv_c = vdupq_n_s16(static_cast<int16_t>(cgv_i));
                int16x8_t cb_c = vdupq_n_s16(static_cast<int16_t>(cb_i));
                int16x8_t c32 = vdupq_n_s16(32);

                // 计算低 8 像素
                int16x8_t y_scaled_lo = vmulq_s16(y_lo, cy_c);
                int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, cr_c));
                r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);
                int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, cgu_c));
                g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, cgv_c));
                g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);
                int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, cb_c));
                b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

                // 计算高 8 像素
                int16x8_t y_scaled_hi = vmulq_s16(y_hi, cy_c);
                int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, cr_c));
                r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);
                int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, cgu_c));
                g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, cgv_c));
                g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);
                int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, cb_c));
                b_hi = vshrq_n_s16(vaddq_s16(b_hi, c32), 6);

                // 饱和收缩为 8 位
                uint8x8_t r8_lo = vqmovun_s16(r_lo);
                uint8x8_t g8_lo = vqmovun_s16(g_lo);
                uint8x8_t b8_lo = vqmovun_s16(b_lo);
                uint8x8_t r8_hi = vqmovun_s16(r_hi);
                uint8x8_t g8_hi = vqmovun_s16(g_hi);
                uint8x8_t b8_hi = vqmovun_s16(b_hi);

                uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
                uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
                uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);

                // 使用 NEON 交错存储写回 RGBA（vst4q_u8），避免标量循环
                uint8x16_t a8 = vdupq_n_u8(255);
                uint8x16x4_t px;
                if constexpr (isBGRA) {
                    px.val[0] = b8;
                    px.val[1] = g8;
                    px.val[2] = r8;
                    px.val[3] = a8;
                } else {
                    px.val[0] = r8;
                    px.val[1] = g8;
                    px.val[2] = b8;
                    px.val[3] = a8;
                }
                vst4q_u8(dstRow + x * 4, px);
            }
        }

        // 标量尾部：按 2 像素一组
        for (; x + 2 <= width; x += 2) {
            uint8_t u = srcRow[x * 2];
            uint8_t y0 = srcRow[x * 2 + 1];
            uint8_t v = srcRow[x * 2 + 2];
            uint8_t y1 = srcRow[x * 2 + 3];

            int c_y0 = static_cast<int>(y0) - y_offset;
            int c_y1 = static_cast<int>(y1) - y_offset;
            int c_u = static_cast<int>(u) - 128;
            int c_v = static_cast<int>(v) - 128;

            int r0 = (cy_i * c_y0 + cr_i * c_v + 32) >> 6;
            int g0 = (cy_i * c_y0 - cgu_i * c_u - cgv_i * c_v + 32) >> 6;
            int b0 = (cy_i * c_y0 + cb_i * c_u + 32) >> 6;
            int r1 = (cy_i * c_y1 + cr_i * c_v + 32) >> 6;
            int g1 = (cy_i * c_y1 - cgu_i * c_u - cgv_i * c_v + 32) >> 6;
            int b1 = (cy_i * c_y1 + cb_i * c_u + 32) >> 6;

            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            r1 = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
            g1 = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
            b1 = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);

            if constexpr (isBGRA) {
                dstRow[x * 4 + 0] = b0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = r0;
                dstRow[x * 4 + 3] = 255;
                dstRow[(x + 1) * 4 + 0] = b1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = r1;
                dstRow[(x + 1) * 4 + 3] = 255;
            } else {
                dstRow[x * 4 + 0] = r0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = b0;
                dstRow[x * 4 + 3] = 255;
                dstRow[(x + 1) * 4 + 0] = r1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = b1;
                dstRow[(x + 1) * 4 + 3] = 255;
            }
        }

        // 单像素尾（奇宽）
        if (x < width) {
            uint8_t u = srcRow[x * 2 + 0];
            uint8_t y0 = srcRow[x * 2 + 1];
            uint8_t v = srcRow[x * 2 + 2];

            int c_y0 = static_cast<int>(y0) - y_offset;
            int c_u = static_cast<int>(u) - 128;
            int c_v = static_cast<int>(v) - 128;
            int r0 = (cy_i * c_y0 + cr_i * c_v + 32) >> 6;
            int g0 = (cy_i * c_y0 - cgu_i * c_u - cgv_i * c_v + 32) >> 6;
            int b0 = (cy_i * c_y0 + cb_i * c_u + 32) >> 6;
            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            if constexpr (isBGRA) {
                dstRow[x * 4 + 0] = b0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = r0;
                dstRow[x * 4 + 3] = 255;
            } else {
                dstRow[x * 4 + 0] = r0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = b0;
                dstRow[x * 4 + 3] = 255;
            }
        }
    }
}

template <bool isBGR>
void _uyvyToRgb_neon_imp(const uint8_t* src, int srcStride,
                         uint8_t* dst, int dstStride,
                         int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // 使用与 YUYV 相同的整数系数（×64）与动态 y_offset
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;
    int cy_i, cr_i, cgu_i, cgv_i, cb_i, y_offset;
    getYuvToRgbCoefficients_neon(is601, isFullRange, cy_i, cr_i, cgu_i, cgv_i, cb_i, y_offset);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // 向量路径：每次处理 16 像素
        if (width >= 16) {
            for (; x + 16 <= width; x += 16) {
                uint8x16_t yuv1 = vld1q_u8(srcRow + x * 2);
                uint8x16_t yuv2 = vld1q_u8(srcRow + x * 2 + 16);

                // Y 索引 1,3,5,7,9,11,13,15
                uint8x8_t y_low = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                           vcreate_u8(0x0F0D0B0907050301ULL));
                uint8x8_t y_high = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                            vcreate_u8(0x0F0D0B0907050301ULL));
                uint8x16_t y_vals = vcombine_u8(y_low, y_high);

                // U: 0,0,4,4,8,8,12,12
                uint8x8_t u_p1 = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                          vcreate_u8(0x0C0C080804040000ULL));
                uint8x8_t u_p2 = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                          vcreate_u8(0x0C0C080804040000ULL));
                uint8x16_t u_vals = vcombine_u8(u_p1, u_p2);

                // V: 2,2,6,6,10,10,14,14
                uint8x8_t v_p1 = vtbl2_u8({ vget_low_u8(yuv1), vget_high_u8(yuv1) },
                                          vcreate_u8(0x0E0E0A0A06060202ULL));
                uint8x8_t v_p2 = vtbl2_u8({ vget_low_u8(yuv2), vget_high_u8(yuv2) },
                                          vcreate_u8(0x0E0E0A0A06060202ULL));
                uint8x16_t v_vals = vcombine_u8(v_p1, v_p2);

                // 宽化与偏移
                int16x8_t y_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(y_vals))), vdupq_n_s16(y_offset));
                int16x8_t y_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(y_vals))), vdupq_n_s16(y_offset));
                int16x8_t u_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(u_vals))), vdupq_n_s16(128));
                int16x8_t u_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(u_vals))), vdupq_n_s16(128));
                int16x8_t v_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(v_vals))), vdupq_n_s16(128));
                int16x8_t v_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(v_vals))), vdupq_n_s16(128));

                int16x8_t cy_c = vdupq_n_s16(static_cast<int16_t>(cy_i));
                int16x8_t cr_c = vdupq_n_s16(static_cast<int16_t>(cr_i));
                int16x8_t cgu_c = vdupq_n_s16(static_cast<int16_t>(cgu_i));
                int16x8_t cgv_c = vdupq_n_s16(static_cast<int16_t>(cgv_i));
                int16x8_t cb_c = vdupq_n_s16(static_cast<int16_t>(cb_i));
                int16x8_t c32 = vdupq_n_s16(32);

                // 低 8 像素
                int16x8_t y_scaled_lo = vmulq_s16(y_lo, cy_c);
                int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, cr_c));
                r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);
                int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, cgu_c));
                g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, cgv_c));
                g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);
                int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, cb_c));
                b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

                // 高 8 像素
                int16x8_t y_scaled_hi = vmulq_s16(y_hi, cy_c);
                int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, cr_c));
                r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);
                int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, cgu_c));
                g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, cgv_c));
                g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);
                int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, cb_c));
                b_hi = vshrq_n_s16(vaddq_s16(b_hi, c32), 6);

                // 饱和收缩
                uint8x8_t r8_lo = vqmovun_s16(r_lo);
                uint8x8_t g8_lo = vqmovun_s16(g_lo);
                uint8x8_t b8_lo = vqmovun_s16(b_lo);
                uint8x8_t r8_hi = vqmovun_s16(r_hi);
                uint8x8_t g8_hi = vqmovun_s16(g_hi);
                uint8x8_t b8_hi = vqmovun_s16(b_hi);

                uint8x16_t r8 = vcombine_u8(r8_lo, r8_hi);
                uint8x16_t g8 = vcombine_u8(g8_lo, g8_hi);
                uint8x16_t b8 = vcombine_u8(b8_lo, b8_hi);

                // 使用 NEON 交错存储写回 RGB（vst3q_u8），避免标量循环
                uint8x16x3_t px3;
                if constexpr (isBGR) {
                    px3.val[0] = b8;
                    px3.val[1] = g8;
                    px3.val[2] = r8;
                } else {
                    px3.val[0] = r8;
                    px3.val[1] = g8;
                    px3.val[2] = b8;
                }
                vst3q_u8(dstRow + x * 3, px3);
            }
        }

        // 标量尾部
        for (; x + 2 <= width; x += 2) {
            uint8_t u = srcRow[x * 2];
            uint8_t y0 = srcRow[x * 2 + 1];
            uint8_t v = srcRow[x * 2 + 2];
            uint8_t y1 = srcRow[x * 2 + 3];

            int c_y0 = static_cast<int>(y0) - y_offset;
            int c_y1 = static_cast<int>(y1) - y_offset;
            int c_u = static_cast<int>(u) - 128;
            int c_v = static_cast<int>(v) - 128;

            int r0 = (cy_i * c_y0 + cr_i * c_v + 32) >> 6;
            int g0 = (cy_i * c_y0 - cgu_i * c_u - cgv_i * c_v + 32) >> 6;
            int b0 = (cy_i * c_y0 + cb_i * c_u + 32) >> 6;
            int r1 = (cy_i * c_y1 + cr_i * c_v + 32) >> 6;
            int g1 = (cy_i * c_y1 - cgu_i * c_u - cgv_i * c_v + 32) >> 6;
            int b1 = (cy_i * c_y1 + cb_i * c_u + 32) >> 6;

            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            r1 = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
            g1 = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
            b1 = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);

            if constexpr (isBGR) {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;
                dstRow[(x + 1) * 3 + 0] = b1;
                dstRow[(x + 1) * 3 + 1] = g1;
                dstRow[(x + 1) * 3 + 2] = r1;
            } else {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;
                dstRow[(x + 1) * 3 + 0] = r1;
                dstRow[(x + 1) * 3 + 1] = g1;
                dstRow[(x + 1) * 3 + 2] = b1;
            }
        }

        // 单像素尾
        if (x < width) {
            uint8_t u = srcRow[x * 2 + 0];
            uint8_t y0 = srcRow[x * 2 + 1];
            uint8_t v = srcRow[x * 2 + 2];

            int c_y0 = static_cast<int>(y0) - y_offset;
            int c_u = static_cast<int>(u) - 128;
            int c_v = static_cast<int>(v) - 128;
            int r0 = (cy_i * c_y0 + cr_i * c_v + 32) >> 6;
            int g0 = (cy_i * c_y0 - cgu_i * c_u - cgv_i * c_v + 32) >> 6;
            int b0 = (cy_i * c_y0 + cb_i * c_u + 32) >> 6;
            r0 = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
            g0 = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
            b0 = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
            if constexpr (isBGR) {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;
            } else {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;
            }
        }
    }
}

// YUYV conversion functions
void yuyvToBgr24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _yuyvToRgb_neon_imp<true>(src, srcStride, dst, dstStride, width, height, flag);
}

void yuyvToRgb24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _yuyvToRgb_neon_imp<false>(src, srcStride, dst, dstStride, width, height, flag);
}

void yuyvToBgra32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _yuyvToRgba_neon_imp<true>(src, srcStride, dst, dstStride, width, height, flag);
}

void yuyvToRgba32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _yuyvToRgba_neon_imp<false>(src, srcStride, dst, dstStride, width, height, flag);
}

// UYVY conversion functions
void uyvyToBgr24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _uyvyToRgb_neon_imp<true>(src, srcStride, dst, dstStride, width, height, flag);
}

void uyvyToRgb24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _uyvyToRgb_neon_imp<false>(src, srcStride, dst, dstStride, width, height, flag);
}

void uyvyToBgra32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _uyvyToRgba_neon_imp<true>(src, srcStride, dst, dstStride, width, height, flag);
}

void uyvyToRgba32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _uyvyToRgba_neon_imp<false>(src, srcStride, dst, dstStride, width, height, flag);
}

#endif // ENABLE_NEON_IMP
} // namespace ccap
