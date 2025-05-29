/**
 * @file ccap_convert_neon.cpp
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#include "ccap_convert_neon.h"

#include "ccap_convert.h"

namespace ccap
{
bool hasNEON()
{
#if ENABLE_NEON_IMP
    static bool s_hasNEON = hasNEON_();
    return s_hasNEON;
#else
    return false;
#endif
}

#if ENABLE_NEON_IMP

template <int inputChannels, int outputChannels>
void colorShuffle_neon(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, const uint8_t inputShuffle[])
{
    // Implement a general colorShuffle, accelerated by NEON
    static_assert((inputChannels == 3 || inputChannels == 4) &&
                      (outputChannels == 3 || outputChannels == 4),
                  "inputChannels and outputChannels must be 3 or 4");

    if (height < 0)
    {
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
    for (int i = 0; i < patchSize; ++i)
    {
        auto idx1 = i * outputChannels;
        auto idx2 = i * inputChannels;
        if (idx1 + outputChannels <= 16)
        {
            shuffleData[idx1] = inputShuffle[0] + idx2;
            shuffleData[idx1 + 1] = inputShuffle[1] + idx2;
            shuffleData[idx1 + 2] = inputShuffle[2] + idx2;

            if constexpr (outputChannels == 4)
            {
                if constexpr (inputChannels == 4)
                    shuffleData[idx1 + 3] = inputShuffle[3] + idx2;
                else
                    shuffleData[idx1 + 3] = 0xFF; // no alpha
            }
        }
    }

    uint8x16_t shuffle_tbl = vld1q_u8(shuffleData);

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        uint32_t x = 0;

        while (x + patchSize <= (uint32_t)width)
        {
            if constexpr (outputChannels == 4 && inputChannels == 4)
            {
                // 4 -> 4: Load 16 bytes (4 pixels), shuffle, store 16 bytes
                uint8x16_t src_data = vld1q_u8(srcRow + x * 4);
                uint8x16_t dst_data = vqtbl1q_u8(src_data, shuffle_tbl);
                vst1q_u8(dstRow + x * 4, dst_data);
                x += 4;
            }
            else if constexpr (outputChannels == 3 && inputChannels == 3)
            {
                // 3 -> 3: Load 15 bytes (5 pixels), shuffle, store 15 bytes
                uint8x16_t src_data = vld1q_u8(srcRow + x * 3);
                uint8x16_t dst_data = vqtbl1q_u8(src_data, shuffle_tbl);
                vst1q_u8(dstRow + x * 3, dst_data);
                x += 5;
            }
            else if constexpr (outputChannels == 4 && inputChannels == 3)
            {
                // 3 -> 4: Process 3 pixels at a time
                for (int i = 0; i < 3 && x + i < (uint32_t)width; ++i)
                {
                    uint8_t r = srcRow[(x + i) * 3 + inputShuffle[0]];
                    uint8_t g = srcRow[(x + i) * 3 + inputShuffle[1]];
                    uint8_t b = srcRow[(x + i) * 3 + inputShuffle[2]];

                    dstRow[(x + i) * 4 + 0] = r;
                    dstRow[(x + i) * 4 + 1] = g;
                    dstRow[(x + i) * 4 + 2] = b;
                    dstRow[(x + i) * 4 + 3] = 255; // alpha
                }
                x += 3;
            }
            else // outputChannels == 3 && inputChannels == 4
            {
                // 4 -> 3: Process 4 pixels at a time
                for (int i = 0; i < 4 && x + i < (uint32_t)width; ++i)
                {
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
        for (; x < (uint32_t)width; ++x)
        {
            if constexpr (outputChannels == 4)
            {
                uint8_t r = srcRow[x * inputChannels + inputShuffle[0]];
                uint8_t g = srcRow[x * inputChannels + inputShuffle[1]];
                uint8_t b = srcRow[x * inputChannels + inputShuffle[2]];
                uint8_t a = (inputChannels == 4) ? srcRow[x * inputChannels + inputShuffle[3]] : 255;

                dstRow[x * 4 + 0] = r;
                dstRow[x * 4 + 1] = g;
                dstRow[x * 4 + 2] = b;
                dstRow[x * 4 + 3] = a;
            }
            else
            {
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
template void colorShuffle_neon<4, 4>(const uint8_t* src, int srcStride,
                                      uint8_t* dst, int dstStride,
                                      int width, int height, const uint8_t inputShuffle[]);

template void colorShuffle_neon<4, 3>(const uint8_t* src, int srcStride,
                                      uint8_t* dst, int dstStride,
                                      int width, int height, const uint8_t inputShuffle[]);

template void colorShuffle_neon<3, 4>(const uint8_t* src, int srcStride,
                                      uint8_t* dst, int dstStride,
                                      int width, int height, const uint8_t inputShuffle[]);

template void colorShuffle_neon<3, 3>(const uint8_t* src, int srcStride,
                                      uint8_t* dst, int dstStride,
                                      int width, int height, const uint8_t inputShuffle[]);

///////////// YUV to RGB conversion functions /////////////

template <bool isBGRA>
void nv12ToRgbaColor_neon_imp(const uint8_t* srcY, int srcYStride,
                              const uint8_t* srcUV, int srcUVStride,
                              uint8_t* dst, int dstStride,
                              int width, int height)
{
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16)
        {
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
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(16)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(16)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 6. BT.601 conversion constants (same as AVX2)
            int16x8_t c74 = vdupq_n_s16(74);
            int16x8_t c102 = vdupq_n_s16(102);
            int16x8_t c25 = vdupq_n_s16(25);
            int16x8_t c52 = vdupq_n_s16(52);
            int16x8_t c129 = vdupq_n_s16(129);
            int16x8_t c32 = vdupq_n_s16(32);

            // 7. Calculate R, G, B for low 8 pixels
            int16x8_t y74_lo = vmulq_s16(y_lo, c74);
            int16x8_t r_lo = vaddq_s16(y74_lo, vmulq_s16(v_lo, c102));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(y74_lo, vmulq_s16(u_lo, c25));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c52));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(y74_lo, vmulq_s16(u_lo, c129));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 8. Calculate R, G, B for high 8 pixels
            int16x8_t y74_hi = vmulq_s16(y_hi, c74);
            int16x8_t r_hi = vaddq_s16(y74_hi, vmulq_s16(v_hi, c102));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(y74_hi, vmulq_s16(u_hi, c25));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c52));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(y74_hi, vmulq_s16(u_hi, c129));
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
            if constexpr (isBGRA)
            {
                uint8x16x4_t bgra;
                bgra.val[0] = b8;
                bgra.val[1] = g8;
                bgra.val[2] = r8;
                bgra.val[3] = a8;
                vst4q_u8(dstRow + x * 4, bgra);
            }
            else
            {
                uint8x16x4_t rgba;
                rgba.val[0] = r8;
                rgba.val[1] = g8;
                rgba.val[2] = b8;
                rgba.val[3] = a8;
                vst4q_u8(dstRow + x * 4, rgba);
            }
        }

        // Process remaining pixels
        for (; x < width; x += 2)
        {
            int y0 = yRow[x] - 16;
            int y1 = (x + 1 < width) ? yRow[x + 1] - 16 : y0;
            int u = uvRow[x] - 128;     // U at even positions
            int v = uvRow[x + 1] - 128; // V at odd positions

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            if constexpr (isBGRA)
            {
                dstRow[x * 4 + 0] = b0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = r0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width)
                {
                    dstRow[(x + 1) * 4 + 0] = b1;
                    dstRow[(x + 1) * 4 + 1] = g1;
                    dstRow[(x + 1) * 4 + 2] = r1;
                    dstRow[(x + 1) * 4 + 3] = 255;
                }
            }
            else
            {
                dstRow[x * 4 + 0] = r0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = b0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width)
                {
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
                              int width, int height)
{
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16)
        {
            // 1. Load 16 Y values
            uint8x16_t y_vals = vld1q_u8(yRow + x);

            // 2. Load 16 bytes UV (8 UV pairs for 16 pixels)
            uint8x16_t uv_vals = vld1q_u8(uvRow + x);

            // 3. Deinterleave U and V (NV12 format: UVUVUV...)
            uint8x8x2_t uv_deint = vuzp_u8(vget_low_u8(uv_vals), vget_high_u8(uv_vals));
            uint8x8_t u_vals = uv_deint.val[0]; // U: 0,2,4,6...
            uint8x8_t v_vals = uv_deint.val[1]; // V: 1,3,5,7...

            // 4. Duplicate each U and V value for 2 pixels
            uint8x8x2_t u_dup = vzip_u8(u_vals, u_vals);
            uint8x8x2_t v_dup = vzip_u8(v_vals, v_vals);
            uint8x16_t u_expanded = vcombine_u8(u_dup.val[0], u_dup.val[1]);
            uint8x16_t v_expanded = vcombine_u8(v_dup.val[0], v_dup.val[1]);

            // 5. Convert to 16-bit and apply offsets
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(16)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(16)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 6. BT.601 conversion constants (same as AVX2)
            int16x8_t c74 = vdupq_n_s16(74);
            int16x8_t c102 = vdupq_n_s16(102);
            int16x8_t c25 = vdupq_n_s16(25);
            int16x8_t c52 = vdupq_n_s16(52);
            int16x8_t c129 = vdupq_n_s16(129);
            int16x8_t c32 = vdupq_n_s16(32);

            // 7. Calculate R, G, B for low 8 pixels
            int16x8_t y74_lo = vmulq_s16(y_lo, c74);
            int16x8_t r_lo = vaddq_s16(y74_lo, vmulq_s16(v_lo, c102));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(y74_lo, vmulq_s16(u_lo, c25));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c52));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(y74_lo, vmulq_s16(u_lo, c129));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 8. Calculate R, G, B for high 8 pixels
            int16x8_t y74_hi = vmulq_s16(y_hi, c74);
            int16x8_t r_hi = vaddq_s16(y74_hi, vmulq_s16(v_hi, c102));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(y74_hi, vmulq_s16(u_hi, c25));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c52));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(y74_hi, vmulq_s16(u_hi, c129));
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
            for (int i = 0; i < 16; ++i)
            {
                if constexpr (isBGR)
                {
                    dstRow[(x + i) * 3 + 0] = b_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = r_arr[i];
                }
                else
                {
                    dstRow[(x + i) * 3 + 0] = r_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = b_arr[i];
                }
            }
        }

        // Process remaining pixels
        for (; x < width; x += 2)
        {
            int y0 = yRow[x] - 16;
            int y1 = (x + 1 < width) ? yRow[x + 1] - 16 : y0;
            int u = uvRow[x] - 128;     // U at even positions
            int v = uvRow[x + 1] - 128; // V at odd positions

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            if constexpr (isBGR)
            {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;

                if (x + 1 < width)
                {
                    dstRow[(x + 1) * 3 + 0] = b1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = r1;
                }
            }
            else
            {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;

                if (x + 1 < width)
                {
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
                          int width, int height)
{
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16)
        {
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
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(16)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(16)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 5. BT.601 conversion constants (same as AVX2)
            int16x8_t c74 = vdupq_n_s16(74);
            int16x8_t c102 = vdupq_n_s16(102);
            int16x8_t c25 = vdupq_n_s16(25);
            int16x8_t c52 = vdupq_n_s16(52);
            int16x8_t c129 = vdupq_n_s16(129);
            int16x8_t c32 = vdupq_n_s16(32);

            // 6. Calculate R, G, B for low 8 pixels
            int16x8_t y74_lo = vmulq_s16(y_lo, c74);
            int16x8_t r_lo = vaddq_s16(y74_lo, vmulq_s16(v_lo, c102));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(y74_lo, vmulq_s16(u_lo, c25));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c52));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(y74_lo, vmulq_s16(u_lo, c129));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 7. Calculate R, G, B for high 8 pixels
            int16x8_t y74_hi = vmulq_s16(y_hi, c74);
            int16x8_t r_hi = vaddq_s16(y74_hi, vmulq_s16(v_hi, c102));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(y74_hi, vmulq_s16(u_hi, c25));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c52));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(y74_hi, vmulq_s16(u_hi, c129));
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
            if constexpr (isBGRA)
            {
                uint8x16x4_t bgra;
                bgra.val[0] = b8;
                bgra.val[1] = g8;
                bgra.val[2] = r8;
                bgra.val[3] = a8;
                vst4q_u8(dstRow + x * 4, bgra);
            }
            else
            {
                uint8x16x4_t rgba;
                rgba.val[0] = r8;
                rgba.val[1] = g8;
                rgba.val[2] = b8;
                rgba.val[3] = a8;
                vst4q_u8(dstRow + x * 4, rgba);
            }
        }

        // Process remaining pixels
        for (; x < width; x += 2)
        {
            int y0 = yRow[x] - 16;
            int y1 = (x + 1 < width) ? yRow[x + 1] - 16 : y0;
            int u = uRow[x / 2] - 128;
            int v = vRow[x / 2] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            if constexpr (isBGRA)
            {
                dstRow[x * 4 + 0] = b0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = r0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width)
                {
                    dstRow[(x + 1) * 4 + 0] = b1;
                    dstRow[(x + 1) * 4 + 1] = g1;
                    dstRow[(x + 1) * 4 + 2] = r1;
                    dstRow[(x + 1) * 4 + 3] = 255;
                }
            }
            else
            {
                dstRow[x * 4 + 0] = r0;
                dstRow[x * 4 + 1] = g0;
                dstRow[x * 4 + 2] = b0;
                dstRow[x * 4 + 3] = 255;

                if (x + 1 < width)
                {
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
                         int width, int height)
{
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;

        // Process 16 pixels at a time using NEON
        for (; x + 16 <= width; x += 16)
        {
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
            int16x8_t y_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(y_vals), vdup_n_u8(16)));
            int16x8_t y_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(y_vals), vdup_n_u8(16)));
            int16x8_t u_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t u_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(u_expanded), vdup_n_u8(128)));
            int16x8_t v_lo = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v_expanded), vdup_n_u8(128)));
            int16x8_t v_hi = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v_expanded), vdup_n_u8(128)));

            // 5. BT.601 conversion constants (same as AVX2)
            int16x8_t c74 = vdupq_n_s16(74);
            int16x8_t c102 = vdupq_n_s16(102);
            int16x8_t c25 = vdupq_n_s16(25);
            int16x8_t c52 = vdupq_n_s16(52);
            int16x8_t c129 = vdupq_n_s16(129);
            int16x8_t c32 = vdupq_n_s16(32);

            // 6. Calculate R, G, B for low 8 pixels
            int16x8_t y74_lo = vmulq_s16(y_lo, c74);
            int16x8_t r_lo = vaddq_s16(y74_lo, vmulq_s16(v_lo, c102));
            r_lo = vshrq_n_s16(vaddq_s16(r_lo, c32), 6);

            int16x8_t g_lo = vsubq_s16(y74_lo, vmulq_s16(u_lo, c25));
            g_lo = vsubq_s16(g_lo, vmulq_s16(v_lo, c52));
            g_lo = vshrq_n_s16(vaddq_s16(g_lo, c32), 6);

            int16x8_t b_lo = vaddq_s16(y74_lo, vmulq_s16(u_lo, c129));
            b_lo = vshrq_n_s16(vaddq_s16(b_lo, c32), 6);

            // 7. Calculate R, G, B for high 8 pixels
            int16x8_t y74_hi = vmulq_s16(y_hi, c74);
            int16x8_t r_hi = vaddq_s16(y74_hi, vmulq_s16(v_hi, c102));
            r_hi = vshrq_n_s16(vaddq_s16(r_hi, c32), 6);

            int16x8_t g_hi = vsubq_s16(y74_hi, vmulq_s16(u_hi, c25));
            g_hi = vsubq_s16(g_hi, vmulq_s16(v_hi, c52));
            g_hi = vshrq_n_s16(vaddq_s16(g_hi, c32), 6);

            int16x8_t b_hi = vaddq_s16(y74_hi, vmulq_s16(u_hi, c129));
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

            // 9. Store RGB24 data using arrays (similar to AVX2 approach)
            alignas(16) uint8_t r_arr[16], g_arr[16], b_arr[16];
            vst1q_u8(r_arr, r8);
            vst1q_u8(g_arr, g8);
            vst1q_u8(b_arr, b8);

            // Store interleaved RGB24 data
            for (int i = 0; i < 16; ++i)
            {
                if constexpr (isBGR)
                {
                    dstRow[(x + i) * 3 + 0] = b_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = r_arr[i];
                }
                else
                {
                    dstRow[(x + i) * 3 + 0] = r_arr[i];
                    dstRow[(x + i) * 3 + 1] = g_arr[i];
                    dstRow[(x + i) * 3 + 2] = b_arr[i];
                }
            }
        }

        // Process remaining pixels
        for (; x < width; x += 2)
        {
            int y0 = yRow[x] - 16;
            int y1 = (x + 1 < width) ? yRow[x + 1] - 16 : y0;
            int u = uRow[x / 2] - 128;
            int v = vRow[x / 2] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            if constexpr (isBGR)
            {
                dstRow[x * 3 + 0] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;

                if (x + 1 < width)
                {
                    dstRow[(x + 1) * 3 + 0] = b1;
                    dstRow[(x + 1) * 3 + 1] = g1;
                    dstRow[(x + 1) * 3 + 2] = r1;
                }
            }
            else
            {
                dstRow[x * 3 + 0] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;

                if (x + 1 < width)
                {
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
                       int width, int height)
{
    nv12ToRgbaColor_neon_imp<true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height);
}

void nv12ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height)
{
    nv12ToRgbaColor_neon_imp<false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height);
}

void nv12ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    _nv12ToRgbColor_neon_imp<true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height);
}

void nv12ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    _nv12ToRgbColor_neon_imp<false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height);
}

void i420ToBgra32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height)
{
    _i420ToRgba_neon_imp<true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height);
}

void i420ToRgba32_neon(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height)
{
    _i420ToRgba_neon_imp<false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height);
}

void i420ToBgr24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    _i420ToRgb_neon_imp<true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height);
}

void i420ToRgb24_neon(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height)
{
    _i420ToRgb_neon_imp<false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height);
}

#endif // ENABLE_NEON_IMP
} // namespace ccap
