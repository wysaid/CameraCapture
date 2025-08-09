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

// Coefficient function from AVX2 implementation  
inline void getYuvToRgbCoefficients(bool isBT601, bool isFullRange, int& cy, int& cr, int& cgu, int& cgv, int& cb) {
    if (isBT601) {
        if (isFullRange) { // BT.601 Full Range: 256, 351, 86, 179, 443 (divided by 4)
            cy = 64;
            cr = 88;
            cgu = 22;
            cgv = 45;
            cb = 111;
        } else { // BT.601 Video Range: 298, 409, 100, 208, 516 (divided by 4)
            cy = 75;
            cr = 102;
            cgu = 25;
            cgv = 52;
            cb = 129;
        }
    } else {
        if (isFullRange) { // BT.709 Full Range: 256, 403, 48, 120, 475 (divided by 4)
            cy = 64;
            cr = 101;
            cgu = 12;
            cgv = 30;
            cb = 119;
        } else { // BT.709 Video Range: 298, 459, 55, 136, 541 (divided by 4)
            cy = 75;
            cr = 115;
            cgu = 14;
            cgv = 34;
            cb = 135;
        }
    }
}
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

    // 基于 swapRB 参数生成 shuffle 数组
    const uint8_t inputShuffle[4] = {
        swapRB ? 2u : 0u, // R 通道
        1,                // G 通道
        swapRB ? 0u : 2u, // B 通道
        3                 // A 通道
    };

    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    alignas(16) uint8_t shuffleData[16];
    memset(shuffleData, 0xff, sizeof(shuffleData));

    constexpr uint32_t inputPatchSize = inputChannels == 4 ? 4 : 5;
    constexpr uint32_t outputPatchSize = outputChannels == 4 ? 4 : 5;
    constexpr uint32_t patchSize = inputPatchSize < outputPatchSize ? inputPatchSize : outputPatchSize;

    // Build shuffle lookup table for NEON vtbl
    for (int i = 0; i < patchSize; ++i) {
        auto idx1 = i * outputChannels;
        auto idx2 = i * inputChannels;
        if (idx1 + outputChannels <= 16) {
            shuffleData[idx1] = inputShuffle[0] + idx2;
            shuffleData[idx1 + 1] = inputShuffle[1] + idx2;
            shuffleData[idx1 + 2] = inputShuffle[2] + idx2;

            if constexpr (outputChannels == 4) {
                if constexpr (inputChannels == 4)
                    shuffleData[idx1 + 3] = inputShuffle[3] + idx2;
                else
                    shuffleData[idx1 + 3] = 0xFF; // no alpha
            }
        }
    }

    uint8x16_t shuffle_tbl = vld1q_u8(shuffleData);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        uint32_t x = 0;

        while (x + patchSize <= (uint32_t)width) {
            if constexpr (outputChannels == 4 && inputChannels == 4) {
                // 4 -> 4: Load 16 bytes (4 pixels), shuffle, store 16 bytes
                uint8x16_t src_data = vld1q_u8(srcRow + x * 4);
                uint8x16_t dst_data = vqtbl1q_u8(src_data, shuffle_tbl);
                vst1q_u8(dstRow + x * 4, dst_data);
                x += 4;
            } else if constexpr (outputChannels == 3 && inputChannels == 3) {
                // 3 -> 3: Load 15 bytes (5 pixels), shuffle, store 15 bytes
                uint8x16_t src_data = vld1q_u8(srcRow + x * 3);
                uint8x16_t dst_data = vqtbl1q_u8(src_data, shuffle_tbl);
                vst1q_u8(dstRow + x * 3, dst_data);
                x += 5;
            } else if constexpr (outputChannels == 4 && inputChannels == 3) {
                // 3 -> 4: Process 3 pixels at a time
                for (int i = 0; i < 3 && x + i < (uint32_t)width; ++i) {
                    uint8_t r = srcRow[(x + i) * 3 + inputShuffle[0]];
                    uint8_t g = srcRow[(x + i) * 3 + inputShuffle[1]];
                    uint8_t b = srcRow[(x + i) * 3 + inputShuffle[2]];

                    dstRow[(x + i) * 4 + 0] = r;
                    dstRow[(x + i) * 4 + 1] = g;
                    dstRow[(x + i) * 4 + 2] = b;
                    dstRow[(x + i) * 4 + 3] = 255; // alpha
                }
                x += 3;
            } else // outputChannels == 3 && inputChannels == 4
            {
                // 4 -> 3: Process 4 pixels at a time
                for (int i = 0; i < 4 && x + i < (uint32_t)width; ++i) {
                    uint8_t r = srcRow[(x + i) * 4 + inputShuffle[0]];
                    uint8_t g = srcRow[(x + i) * 4 + inputShuffle[1]];
                    uint8_t b = srcRow[(x + i) * 4 + inputShuffle[2]];

                    dstRow[(x + i) * 3 + 0] = r;
                    dstRow[(x + i) * 3 + 1] = g;
                    dstRow[(x + i) * 3 + 2] = b;
                }
                x += 4;
            }
        }

        // Handle remaining pixels
        for (; x < (uint32_t)width; ++x) {
            if constexpr (outputChannels == 4) {
                uint8_t r = srcRow[x * inputChannels + inputShuffle[0]];
                uint8_t g = srcRow[x * inputChannels + inputShuffle[1]];
                uint8_t b = srcRow[x * inputChannels + inputShuffle[2]];
                uint8_t a = (inputChannels == 4) ? srcRow[x * inputChannels + inputShuffle[3]] : 255;

                dstRow[x * 4 + 0] = r;
                dstRow[x * 4 + 1] = g;
                dstRow[x * 4 + 2] = b;
                dstRow[x * 4 + 3] = a;
            } else {
                uint8_t r = srcRow[x * inputChannels + inputShuffle[0]];
                uint8_t g = srcRow[x * inputChannels + inputShuffle[1]];
                uint8_t b = srcRow[x * inputChannels + inputShuffle[2]];

                dstRow[x * 3 + 0] = r;
                dstRow[x * 3 + 1] = g;
                dstRow[x * 3 + 2] = b;
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

template <bool isBGRA, bool isFullRange>
void nv12ToRgbaColor_neon_imp(const uint8_t* srcY, int srcYStride,
                              const uint8_t* srcUV, int srcUVStride,
                              uint8_t* dst, int dstStride,
                              int width, int height, bool is601) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }
    
    // Get dynamic YUV-to-RGB coefficients based on is601 and isFullRange
    int cy, cr, cgu, cgv, cb;
    if (is601) {
        if (isFullRange) {
            cy = 76; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 FullRange
        } else {
            cy = 75; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 VideoRange  
        }
    } else {
        if (isFullRange) {
            cy = 76; cr = 111; cgu = 13; cgv = 34; cb = 135; // BT.709 FullRange
        } else {
            cy = 75; cr = 111; cgu = 13; cgv = 34; cb = 135; // BT.709 VideoRange
        }
    }

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
            // UV is 2:1 horizontally subsampled, so for 16 Y pixels we need 8 UV pairs
            uint8x16_t uv_vals = vld1q_u8(uvRow + (x / 2) * 2);

            // 3. Deinterleave U and V (NV12 format: UVUVUV...)
            uint8x8x2_t uv_deint = vuzp_u8(vget_low_u8(uv_vals), vget_high_u8(uv_vals));
            uint8x8_t u_vals = uv_deint.val[0]; // U: 0,2,4,6...
            uint8x8_t v_vals = uv_deint.val[1]; // V: 1,3,5,7...

            // 4. Duplicate each U and V value for 2 pixels (since UV is subsampled)
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // 5. Convert to 16-bit and apply offsets (fixed for FullRange support)
            uint8x8_t yOffset = vdup_n_u8(isFullRange ? 0 : 16);
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), yOffset));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), yOffset));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 6. Dynamic conversion coefficients matching AVX2 exactly
            int16x8_t c_cy = vdupq_n_s16(cy);
            int16x8_t c_cr = vdupq_n_s16(cr);
            int16x8_t c_cgu = vdupq_n_s16(cgu);
            int16x8_t c_cgv = vdupq_n_s16(cgv);
            int16x8_t c_cb = vdupq_n_s16(cb);
            int16x8_t c32 = vdupq_n_s16(32);

            // 7. Calculate R, G, B for low 8 pixels using dynamic coefficients
            int16x8_t ycy_lo = vmulq_s16(y_lo, c_cy);
            int16x8_t r_lo = vaddq_s16(ycy_lo, vmulq_s16(v_lo, c_cr));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(ycy_lo, vmulq_s16(u_lo, c_cgu));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c_cgv));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(ycy_lo, vmulq_s16(u_lo, c_cb));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 8. Calculate R, G, B for high 8 pixels using dynamic coefficients
            int16x8_t ycy_hi = vmulq_s16(y_hi, c_cy);
            int16x8_t r_hi = vaddq_s16(ycy_hi, vmulq_s16(v_hi, c_cr));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(ycy_hi, vmulq_s16(u_hi, c_cgu));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c_cgv));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(ycy_hi, vmulq_s16(u_hi, c_cb));
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

        // Process remaining pixels (scalar fallback)
        for (; x < width; x += 2) {
            // Use coefficient-based conversion for remaining pixels
            int cy, cr, cgu, cgv, cb;
            if (is601) {
                if (isFullRange) {
                    cy = 76; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Full Range
                } else {
                    cy = 75; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Video Range
                }
            } else {
                if (isFullRange) {
                    cy = 76; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Full Range
                } else {
                    cy = 75; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Video Range
                }
            }
            
            int yOffset = isFullRange ? 0 : 16;
            int y0 = yRow[x] - yOffset;
            int y1 = (x + 1 < width) ? yRow[x + 1] - yOffset : y0;
            int u = uvRow[x] - 128;     // U at even positions
            int v = uvRow[x + 1] - 128; // V at odd positions

            // Convert first pixel
            int r0 = (cy * y0 + cr * v + 32) >> 6;
            int g0 = (cy * y0 - cgu * u - cgv * v + 32) >> 6;
            int b0 = (cy * y0 + cb * u + 32) >> 6;
            
            // Convert second pixel  
            int r1 = (cy * y1 + cr * v + 32) >> 6;
            int g1 = (cy * y1 - cgu * u - cgv * v + 32) >> 6;
            int b1 = (cy * y1 + cb * u + 32) >> 6;
            
            // Clamp values
            r0 = std::max(0, std::min(255, r0));
            g0 = std::max(0, std::min(255, g0));
            b0 = std::max(0, std::min(255, b0));
            r1 = std::max(0, std::min(255, r1));
            g1 = std::max(0, std::min(255, g1));
            b1 = std::max(0, std::min(255, b1));

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

template <bool isBGR, bool isFullRange>
void _nv12ToRgbColor_neon_imp(const uint8_t* srcY, int srcYStride,
                              const uint8_t* srcUV, int srcUVStride,
                              uint8_t* dst, int dstStride,
                              int width, int height, bool is601) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }
    
    // Get dynamic YUV-to-RGB coefficients based on is601 and isFullRange
    int cy, cr, cgu, cgv, cb;
    if (is601) {
        if (isFullRange) {
            cy = 76; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 FullRange
        } else {
            cy = 75; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 VideoRange  
        }
    } else {
        if (isFullRange) {
            cy = 76; cr = 111; cgu = 13; cgv = 34; cb = 135; // BT.709 FullRange
        } else {
            cy = 75; cr = 111; cgu = 13; cgv = 34; cb = 135; // BT.709 VideoRange
        }
    }

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            uint8x16_t y_vals = vld1q_u8(yRow + x);

            // 2. Load 16 bytes UV (8 UV pairs for 16 pixels) - FIXED: UV is 2:1 horizontally subsampled  
            // For 16 Y pixels at x, we need 8 UV pairs starting at uvRow + (x/2)*2
            uint8x16_t uv_vals = vld1q_u8(uvRow + (x / 2) * 2);

            // 3. Deinterleave U and V (NV12 format: UVUVUV...)
            uint8x8x2_t uv_deint = vuzp_u8(vget_low_u8(uv_vals), vget_high_u8(uv_vals));
            uint8x8_t u_vals = uv_deint.val[0]; // U: 0,2,4,6...
            uint8x8_t v_vals = uv_deint.val[1]; // V: 1,3,5,7...

            // 4. Duplicate each U and V value for 2 pixels
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // 5. Convert to 16-bit and apply offsets (fixed for FullRange support)
            uint8x8_t yOffset = vdup_n_u8(isFullRange ? 0 : 16);
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), yOffset));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), yOffset));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 6. Dynamic conversion coefficients matching AVX2 exactly
            int16x8_t c_cy = vdupq_n_s16(cy);
            int16x8_t c_cr = vdupq_n_s16(cr);
            int16x8_t c_cgu = vdupq_n_s16(cgu);
            int16x8_t c_cgv = vdupq_n_s16(cgv);
            int16x8_t c_cb = vdupq_n_s16(cb);
            int16x8_t c32 = vdupq_n_s16(32);

            // 7. Calculate R, G, B for low 8 pixels using dynamic coefficients
            int16x8_t ycy_lo = vmulq_s16(y_lo, c_cy);
            int16x8_t r_lo = vaddq_s16(ycy_lo, vmulq_s16(v_lo, c_cr));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(ycy_lo, vmulq_s16(u_lo, c_cgu));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c_cgv));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(ycy_lo, vmulq_s16(u_lo, c_cb));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 8. Calculate R, G, B for high 8 pixels using dynamic coefficients
            int16x8_t ycy_hi = vmulq_s16(y_hi, c_cy);
            int16x8_t r_hi = vaddq_s16(ycy_hi, vmulq_s16(v_hi, c_cr));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(ycy_hi, vmulq_s16(u_hi, c_cgu));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c_cgv));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(ycy_hi, vmulq_s16(u_hi, c_cb));
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

            // 10. Store RGB24 data using arrays (similar to AVX2 approach)
            alignas(16) uint8_t r_arr[16], g_arr[16], b_arr[16];
            vst1q_u8(r_arr, r8);
            vst1q_u8(g_arr, g8);
            vst1q_u8(b_arr, b8);

            // Store interleaved RGB24 data
            for (int i = 0; i < 16; ++i) {
                if constexpr (isBGR) {
                    dstRow[(x + i) * 3 + 0] = b_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = r_arr[i];
                } else {
                    dstRow[(x + i) * 3 + 0] = r_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = b_arr[i];
                }
            }
        }

        // Process remaining pixels (scalar fallback)
        for (; x < width; x += 2) {
            // Use coefficient-based conversion for remaining pixels
            int cy, cr, cgu, cgv, cb;
            if (is601) {
                if (isFullRange) {
                    cy = 76; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Full Range
                } else {
                    cy = 75; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Video Range
                }
            } else {
                if (isFullRange) {
                    cy = 76; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Full Range
                } else {
                    cy = 75; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Video Range
                }
            }
            
            int yOffset = isFullRange ? 0 : 16;
            int y0 = yRow[x] - yOffset;
            int y1 = (x + 1 < width) ? yRow[x + 1] - yOffset : y0;
            int u = uvRow[(x / 2) * 2] - 128;     // U at even positions
            int v = uvRow[(x / 2) * 2 + 1] - 128; // V at odd positions

            // Convert first pixel
            int r0 = (cy * y0 + cr * v + 32) >> 6;
            int g0 = (cy * y0 - cgu * u - cgv * v + 32) >> 6;
            int b0 = (cy * y0 + cb * u + 32) >> 6;
            
            // Convert second pixel  
            int r1 = (cy * y1 + cr * v + 32) >> 6;
            int g1 = (cy * y1 - cgu * u - cgv * v + 32) >> 6;
            int b1 = (cy * y1 + cb * u + 32) >> 6;
            
            // Clamp values
            r0 = std::max(0, std::min(255, r0));
            g0 = std::max(0, std::min(255, g0));
            b0 = std::max(0, std::min(255, b0));
            r1 = std::max(0, std::min(255, r1));
            g1 = std::max(0, std::min(255, g1));
            b1 = std::max(0, std::min(255, b1));

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

template <bool isBGRA, bool isFullRange>
void _i420ToRgba_neon_imp(const uint8_t* srcY, int srcYStride,
                          const uint8_t* srcU, int srcUStride,
                          const uint8_t* srcV, int srcVStride,
                          uint8_t* dst, int dstStride,
                          int width, int height, bool is601) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Select coefficients based on flags
    int cy, cr, cgu, cgv, cb;
    if (is601) {
        if (isFullRange) {
            cy = 76; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 FullRange
        } else {
            cy = 75; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 VideoRange  
        }
    } else {
        if (isFullRange) {
            cy = 76; cr = 111; cgu = 13; cgv = 34; cb = 135; // BT.709 FullRange
        } else {
            cy = 75; cr = 111; cgu = 13; cgv = 34; cb = 135; // BT.709 VideoRange
        }
    }

    // Load conversion coefficients into NEON registers
    int16x8_t c_y = vdupq_n_s16(cy);
    int16x8_t c_r = vdupq_n_s16(cr);
    int16x8_t c_gu = vdupq_n_s16(cgu);
    int16x8_t c_gv = vdupq_n_s16(cgv);
    int16x8_t c_b = vdupq_n_s16(cb);

    uint8x8_t c128 = vdup_n_u8(128);
    uint8x8_t yOffset = vdup_n_u8(isFullRange ? 0 : 16);



    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16) {
            // Load and process Y, U, V data
            uint8x16_t y_vals = vld1q_u8(yRow + x);
            uint8x8_t u_vals = vld1_u8(uRow + x / 2);
            uint8x8_t v_vals = vld1_u8(vRow + x / 2);

            // Duplicate each U and V value for 2 pixels
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // Convert to 16-bit and apply offsets
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), yOffset));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), yOffset));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), c128));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), c128));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), c128));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), c128));

            // YUV to RGB conversion
            int16x8_t y_scaled_lo = vmulq_s16(y_lo, c_y);
            int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, c_r));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, vdupq_n_s16(32)), 6);

            int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, c_gu));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c_gv));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, vdupq_n_s16(32)), 6);

            int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, c_b));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, vdupq_n_s16(32)), 6);

            int16x8_t y_scaled_hi = vmulq_s16(y_hi, c_y);
            int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, c_r));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, vdupq_n_s16(32)), 6);

            int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, c_gu));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c_gv));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, vdupq_n_s16(32)), 6);

            int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, c_b));
            b_hi = vshrq_n_s16(vaddq_s16(b_hi, vdupq_n_s16(32)), 6);

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

            // Interleave and store RGBA data
            uint8x16x4_t rgba;
            if constexpr (isBGRA) {
                rgba.val[0] = b8;
                rgba.val[1] = g8;
                rgba.val[2] = r8;
                rgba.val[3] = a8;
            } else {
                rgba.val[0] = r8;
                rgba.val[1] = g8;
                rgba.val[2] = b8;
                rgba.val[3] = a8;
            }

            vst4q_u8(dstRow + x * 4, rgba);
        }

        // Process remaining pixels with scalar fallback
        for (; x < width; x++) {
            int y_val = yRow[x] - (isFullRange ? 0 : 16);
            int u_val = uRow[x / 2] - 128;
            int v_val = vRow[x / 2] - 128;

            int r, g, b;
            // Manual YUV to RGB conversion using coefficients
            int r_temp = (cy * y_val + cr * v_val + 32) >> 6;
            int g_temp = (cy * y_val - cgu * u_val - cgv * v_val + 32) >> 6;
            int b_temp = (cy * y_val + cb * u_val + 32) >> 6;
            
            r = std::max(0, std::min(255, r_temp));
            g = std::max(0, std::min(255, g_temp));
            b = std::max(0, std::min(255, b_temp));

            if constexpr (isBGRA) {
                dstRow[x * 4 + 0] = b;
                dstRow[x * 4 + 1] = g;
                dstRow[x * 4 + 2] = r;
                dstRow[x * 4 + 3] = 255;
            } else {
                dstRow[x * 4 + 0] = r;
                dstRow[x * 4 + 1] = g;
                dstRow[x * 4 + 2] = b;
                dstRow[x * 4 + 3] = 255;
            }
        }
    }
}

template <bool isBGR, bool isFullRange>
void _i420ToRgb_neon_imp(const uint8_t* srcY, int srcYStride,
                         const uint8_t* srcU, int srcUStride,
                         const uint8_t* srcV, int srcVStride,
                         uint8_t* dst, int dstStride,
                         int width, int height, bool is601) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

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

            // 5. Use dynamic coefficients instead of hardcoded ones
            // Select coefficients based on flags
            int cy, cr, cgu, cgv, cb;
            if (is601) {
                if (isFullRange) {
                    cy = 76; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Full Range
                } else {
                    cy = 75; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Video Range
                }
            } else {
                if (isFullRange) {
                    cy = 76; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Full Range
                } else {
                    cy = 75; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Video Range
                }
            }

            // Load conversion coefficients into NEON registers
            int16x8_t c_y = vdupq_n_s16(cy);
            int16x8_t c_r = vdupq_n_s16(cr);
            int16x8_t c_gu = vdupq_n_s16(cgu);
            int16x8_t c_gv = vdupq_n_s16(cgv);
            int16x8_t c_b = vdupq_n_s16(cb);

            uint8x8_t yOffset = vdup_n_u8(isFullRange ? 0 : 16);
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), yOffset));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), yOffset));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 6. Calculate R, G, B for low 8 pixels using dynamic coefficients
            int16x8_t y_scaled_lo = vmulq_s16(y_lo, c_y);
            int16x8_t r_lo = vaddq_s16(y_scaled_lo, vmulq_s16(v_lo, c_r));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, vdupq_n_s16(32)), 6);

            int16x8_t g_lo = vsubq_s16(y_scaled_lo, vmulq_s16(u_lo, c_gu));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c_gv));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, vdupq_n_s16(32)), 6);

            int16x8_t b_lo = vaddq_s16(y_scaled_lo, vmulq_s16(u_lo, c_b));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, vdupq_n_s16(32)), 6);

            // 7. Calculate R, G, B for high 8 pixels using dynamic coefficients
            int16x8_t y_scaled_hi = vmulq_s16(y_hi, c_y);
            int16x8_t r_hi = vaddq_s16(y_scaled_hi, vmulq_s16(v_hi, c_r));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, vdupq_n_s16(32)), 6);

            int16x8_t g_hi = vsubq_s16(y_scaled_hi, vmulq_s16(u_hi, c_gu));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c_gv));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, vdupq_n_s16(32)), 6);

            int16x8_t b_hi = vaddq_s16(y_scaled_hi, vmulq_s16(u_hi, c_b));
            b_hi = vshrq_n_s16(vaddq_s16(b_hi, vdupq_n_s16(32)), 6);

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

            // 9. Store RGB24 data using arrays (similar to AVX2 approach)
            alignas(16) uint8_t r_arr[16], g_arr[16], b_arr[16];
            vst1q_u8(r_arr, r8);
            vst1q_u8(g_arr, g8);
            vst1q_u8(b_arr, b8);

            // Store interleaved RGB24 data
            for (int i = 0; i < 16; ++i) {
                if constexpr (isBGR) {
                    dstRow[(x + i) * 3 + 0] = b_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = r_arr[i];
                } else {
                    dstRow[(x + i) * 3 + 0] = r_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = b_arr[i];
                }
            }
        }

        // Process remaining pixels with scalar fallback
        // Get YUV-to-RGB coefficients
        int cy, cr, cgu, cgv, cb;
        if (is601) {
            if (isFullRange) {
                cy = 76; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Full Range
            } else {
                cy = 75; cr = 102; cgu = 25; cgv = 52; cb = 129; // BT.601 Video Range
            }
        } else {
            if (isFullRange) {
                cy = 76; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Full Range
            } else {
                cy = 75; cr = 115; cgu = 22; cgv = 46; cb = 115; // BT.709 Video Range
            }
        }
        for (; x < width; x++) {
            int y_val = yRow[x] - (isFullRange ? 0 : 16);
            int u_val = uRow[x / 2] - 128;
            int v_val = vRow[x / 2] - 128;

            int r, g, b;
            // Manual YUV to RGB conversion using coefficients
            int r_temp = (cy * y_val + cr * v_val + 32) >> 6;
            int g_temp = (cy * y_val - cgu * u_val - cgv * v_val + 32) >> 6;
            int b_temp = (cy * y_val + cb * u_val + 32) >> 6;
            
            r = std::max(0, std::min(255, r_temp));
            g = std::max(0, std::min(255, g_temp));
            b = std::max(0, std::min(255, b_temp));

            if constexpr (isBGR) {
                dstRow[x * 3 + 0] = b;
                dstRow[x * 3 + 1] = g;
                dstRow[x * 3 + 2] = r;
            } else {
                dstRow[x * 3 + 0] = r;
                dstRow[x * 3 + 1] = g;
                dstRow[x * 3 + 2] = b;
            }
        }
    }
}

// NEON accelerated conversion functions
void nv12ToBgra32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        nv12ToRgbaColor_neon_imp<true, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        nv12ToRgbaColor_neon_imp<true, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

void nv12ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        nv12ToRgbaColor_neon_imp<false, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        nv12ToRgbaColor_neon_imp<false, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

void nv12ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        _nv12ToRgbColor_neon_imp<true, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        _nv12ToRgbColor_neon_imp<true, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

void nv12ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        _nv12ToRgbColor_neon_imp<false, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        _nv12ToRgbColor_neon_imp<false, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

void i420ToBgra32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        _i420ToRgba_neon_imp<true, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgba_neon_imp<true, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

void i420ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        _i420ToRgba_neon_imp<false, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgba_neon_imp<false, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

void i420ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        _i420ToRgb_neon_imp<true, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgb_neon_imp<true, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

void i420ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    bool is601 = !(flag & ConvertFlag::BT709);
    bool isFullRange = (flag & ConvertFlag::FullRange);
    
    if (isFullRange) {
        _i420ToRgb_neon_imp<false, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgb_neon_imp<false, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

///////////// YUYV/UYVY to RGB conversion functions /////////////

template <bool isBGRA>
void _yuyvToRgba_neon_imp(const uint8_t* src, int srcStride,
                          uint8_t* dst, int dstStride,
                          int width, int height) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

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

            // Convert YUV to RGB using fixed-point arithmetic
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(16)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(16)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // YUV to RGB conversion coefficients (scaled by 256)
            const int16_t c_y = 298;
            const int16_t c_u_b = 516;
            const int16_t c_u_g = -100;
            const int16_t c_v_g = -208;
            const int16_t c_v_r = 409;

            // Convert to RGB
            int16x8_t r_lo = vaddq_s16(vmulq_n_s16(y_lo, c_y), vmulq_n_s16(v_lo, c_v_r));
            int16x8_t g_lo = vaddq_s16(vaddq_s16(vmulq_n_s16(y_lo, c_y), vmulq_n_s16(u_lo, c_u_g)), vmulq_n_s16(v_lo, c_v_g));
            int16x8_t b_lo = vaddq_s16(vmulq_n_s16(y_lo, c_y), vmulq_n_s16(u_lo, c_u_b));

            int16x8_t r_hi = vaddq_s16(vmulq_n_s16(y_hi, c_y), vmulq_n_s16(v_hi, c_v_r));
            int16x8_t g_hi = vaddq_s16(vaddq_s16(vmulq_n_s16(y_hi, c_y), vmulq_n_s16(u_hi, c_u_g)), vmulq_n_s16(v_hi, c_v_g));
            int16x8_t b_hi = vaddq_s16(vmulq_n_s16(y_hi, c_y), vmulq_n_s16(u_hi, c_u_b));

            // Shift and clamp to 8-bit
            uint8x8_t r_lo_u8 = vqrshrun_n_s16(r_lo, 8);
            uint8x8_t g_lo_u8 = vqrshrun_n_s16(g_lo, 8);
            uint8x8_t b_lo_u8 = vqrshrun_n_s16(b_lo, 8);
            uint8x8_t r_hi_u8 = vqrshrun_n_s16(r_hi, 8);
            uint8x8_t g_hi_u8 = vqrshrun_n_s16(g_hi, 8);
            uint8x8_t b_hi_u8 = vqrshrun_n_s16(b_hi, 8);

            uint8x16_t r_vals = vcombine_u8(r_lo_u8, r_hi_u8);
            uint8x16_t g_vals = vcombine_u8(g_lo_u8, g_hi_u8);
            uint8x16_t b_vals = vcombine_u8(b_lo_u8, b_hi_u8);
            uint8x16_t a_vals = vdupq_n_u8(255);

            // Store as RGBA or BGRA
            uint8x16x4_t rgba;
            if constexpr (isBGRA) {
                rgba.val[0] = b_vals;
                rgba.val[1] = g_vals;
                rgba.val[2] = r_vals;
            } else {
                rgba.val[0] = r_vals;
                rgba.val[1] = g_vals;
                rgba.val[2] = b_vals;
            }
            rgba.val[3] = a_vals;

            vst4q_u8(dstRow + x * 4, rgba);
        }

        // Handle remaining pixels
        for (; x < width; x += 2) {
            if (x + 1 >= width) break;

            uint8_t y0 = srcRow[x * 2];
            uint8_t u = srcRow[x * 2 + 1];
            uint8_t y1 = srcRow[x * 2 + 2];
            uint8_t v = srcRow[x * 2 + 3];

            // YUV to RGB conversion for both pixels
            int c_y0 = (int)y0 - 16;
            int c_y1 = (int)y1 - 16;
            int c_u = (int)u - 128;
            int c_v = (int)v - 128;

            int r0 = (298 * c_y0 + 409 * c_v + 128) >> 8;
            int g0 = (298 * c_y0 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b0 = (298 * c_y0 + 516 * c_u + 128) >> 8;

            int r1 = (298 * c_y1 + 409 * c_v + 128) >> 8;
            int g1 = (298 * c_y1 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b1 = (298 * c_y1 + 516 * c_u + 128) >> 8;

            // Clamp values
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
    }
}

template <bool isBGR>
void _yuyvToRgb_neon_imp(const uint8_t* src, int srcStride,
                         uint8_t* dst, int dstStride,
                         int width, int height) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process pixels in pairs
        for (; x + 2 <= width; x += 2) {
            uint8_t y0 = srcRow[x * 2];
            uint8_t u = srcRow[x * 2 + 1];
            uint8_t y1 = srcRow[x * 2 + 2];
            uint8_t v = srcRow[x * 2 + 3];

            // YUV to RGB conversion
            int c_y0 = (int)y0 - 16;
            int c_y1 = (int)y1 - 16;
            int c_u = (int)u - 128;
            int c_v = (int)v - 128;

            int r0 = (298 * c_y0 + 409 * c_v + 128) >> 8;
            int g0 = (298 * c_y0 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b0 = (298 * c_y0 + 516 * c_u + 128) >> 8;

            int r1 = (298 * c_y1 + 409 * c_v + 128) >> 8;
            int g1 = (298 * c_y1 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b1 = (298 * c_y1 + 516 * c_u + 128) >> 8;

            // Clamp values
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
    }
}

template <bool isBGRA>
void _uyvyToRgba_neon_imp(const uint8_t* src, int srcStride,
                          uint8_t* dst, int dstStride,
                          int width, int height) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process pixels in pairs
        for (; x + 2 <= width; x += 2) {
            uint8_t u = srcRow[x * 2];
            uint8_t y0 = srcRow[x * 2 + 1];
            uint8_t v = srcRow[x * 2 + 2];
            uint8_t y1 = srcRow[x * 2 + 3];

            // YUV to RGB conversion
            int c_y0 = (int)y0 - 16;
            int c_y1 = (int)y1 - 16;
            int c_u = (int)u - 128;
            int c_v = (int)v - 128;

            int r0 = (298 * c_y0 + 409 * c_v + 128) >> 8;
            int g0 = (298 * c_y0 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b0 = (298 * c_y0 + 516 * c_u + 128) >> 8;

            int r1 = (298 * c_y1 + 409 * c_v + 128) >> 8;
            int g1 = (298 * c_y1 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b1 = (298 * c_y1 + 516 * c_u + 128) >> 8;

            // Clamp values
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
    }
}

template <bool isBGR>
void _uyvyToRgb_neon_imp(const uint8_t* src, int srcStride,
                         uint8_t* dst, int dstStride,
                         int width, int height) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process pixels in pairs
        for (; x + 2 <= width; x += 2) {
            uint8_t u = srcRow[x * 2];
            uint8_t y0 = srcRow[x * 2 + 1];
            uint8_t v = srcRow[x * 2 + 2];
            uint8_t y1 = srcRow[x * 2 + 3];

            // YUV to RGB conversion
            int c_y0 = (int)y0 - 16;
            int c_y1 = (int)y1 - 16;
            int c_u = (int)u - 128;
            int c_v = (int)v - 128;

            int r0 = (298 * c_y0 + 409 * c_v + 128) >> 8;
            int g0 = (298 * c_y0 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b0 = (298 * c_y0 + 516 * c_u + 128) >> 8;

            int r1 = (298 * c_y1 + 409 * c_v + 128) >> 8;
            int g1 = (298 * c_y1 - 100 * c_u - 208 * c_v + 128) >> 8;
            int b1 = (298 * c_y1 + 516 * c_u + 128) >> 8;

            // Clamp values
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
    }
}

// YUYV conversion functions
void yuyvToBgr24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _yuyvToRgb_neon_imp<true>(src, srcStride, dst, dstStride, width, height);
}

void yuyvToRgb24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _yuyvToRgb_neon_imp<false>(src, srcStride, dst, dstStride, width, height);
}

void yuyvToBgra32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _yuyvToRgba_neon_imp<true>(src, srcStride, dst, dstStride, width, height);
}

void yuyvToRgba32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _yuyvToRgba_neon_imp<false>(src, srcStride, dst, dstStride, width, height);
}

// UYVY conversion functions
void uyvyToBgr24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _uyvyToRgb_neon_imp<true>(src, srcStride, dst, dstStride, width, height);
}

void uyvyToRgb24_neon(const uint8_t* src, int srcStride,
                      uint8_t* dst, int dstStride,
                      int width, int height, ConvertFlag flag) {
    _uyvyToRgb_neon_imp<false>(src, srcStride, dst, dstStride, width, height);
}

void uyvyToBgra32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _uyvyToRgba_neon_imp<true>(src, srcStride, dst, dstStride, width, height);
}

void uyvyToRgba32_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, ConvertFlag flag) {
    _uyvyToRgba_neon_imp<false>(src, srcStride, dst, dstStride, width, height);
}

#endif // ENABLE_NEON_IMP
} // namespace ccap
