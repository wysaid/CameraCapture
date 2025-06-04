/**
 * @file ccap_convert.cpp
 * @author wysaid (this@wysaid.org)
 * @brief pixel convert functions for ccap.
 * @date 2025-05
 *
 */

#include "ccap_convert.h"

#include "ccap_convert_avx2.h"

#include <cassert>

//////////////  Common Version //////////////

namespace ccap {
template <int inputChannels, int outputChannels, int swapRB>
void colorShuffle(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height)
{
    static_assert((inputChannels == 3 || inputChannels == 4) && (outputChannels == 3 || outputChannels == 4),
                  "inputChannels and outputChannels must be 3 or 4");

    static_assert(inputChannels != outputChannels || swapRB, "swapRB must be true when inputChannels == outputChannels");

#if ENABLE_AVX2_IMP
    if (hasAVX2()) {
        colorShuffle_avx2<inputChannels, outputChannels, swapRB>(src, srcStride, dst, dstStride, width, height);
        return;
    }
#endif

    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }
    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        for (int x = 0; x < width; ++x) {
            if constexpr (swapRB) {
                dstRow[2] = srcRow[0];
                dstRow[1] = srcRow[1];
                dstRow[0] = srcRow[2];
            }
            else {
                dstRow[0] = srcRow[0];
                dstRow[1] = srcRow[1];
                dstRow[2] = srcRow[2];
            }

            if constexpr (outputChannels == 4) {
                if constexpr (inputChannels == 4)
                    dstRow[3] = srcRow[3]; // BGRA
                else
                    dstRow[3] = 0xff; // RGBA
            }
            srcRow += inputChannels;
            dstRow += outputChannels;
        }
    }
}

template void colorShuffle<4, 4, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle<4, 3, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle<4, 3, false>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle<3, 4, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle<3, 4, false>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle<3, 3, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

///////////// YUV to RGB common functions /////////////

template <bool isBgrColor, bool hasAlpha>
void nv12ToRgb_common(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                      int height, ConvertFlag flag)
{
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    const auto convertFunc = getYuvToRgbFunc((flag & ConvertFlag::BT601) != 0, (flag & ConvertFlag::FullRange) != 0);
    constexpr int channels = hasAlpha ? 4 : 3;

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRowY = srcY + y * srcYStride;
        const uint8_t* srcRowUV = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        for (int x = 0; x < width; x += 2) {
            int y0 = srcRowY[x + 0];
            int y1 = srcRowY[x + 1];
            int u = srcRowUV[x];
            int v = srcRowUV[x + 1];

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBgrColor) {
                dstRow[(x + 0) * channels + 0] = b0;
                dstRow[(x + 0) * channels + 1] = g0;
                dstRow[(x + 0) * channels + 2] = r0;

                dstRow[(x + 1) * channels + 0] = b1;
                dstRow[(x + 1) * channels + 1] = g1;
                dstRow[(x + 1) * channels + 2] = r1;
            }
            else {
                dstRow[(x + 0) * channels + 0] = r0;
                dstRow[(x + 0) * channels + 1] = g0;
                dstRow[(x + 0) * channels + 2] = b0;

                dstRow[(x + 1) * channels + 0] = r1;
                dstRow[(x + 1) * channels + 1] = g1;
                dstRow[(x + 1) * channels + 2] = b1;
            }

            if constexpr (hasAlpha) {
                dstRow[(x + 0) * channels + 3] = 255;
                dstRow[(x + 1) * channels + 3] = 255;
            }
        }
    }
}

template <bool isBgrColor, bool hasAlpha>
void i420ToRgb_common(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag)
{
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    const auto convertFunc = getYuvToRgbFunc((flag & ConvertFlag::BT601) != 0, (flag & ConvertFlag::FullRange) != 0);
    constexpr int channels = hasAlpha ? 4 : 3;

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRowY = srcY + y * srcYStride;
        const uint8_t* srcRowU = srcU + (y / 2) * srcUStride;
        const uint8_t* srcRowV = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        for (int x = 0; x < width; x += 2) {
            int y0 = srcRowY[x + 0];
            int y1 = srcRowY[x + 1];
            int u = srcRowU[x / 2];
            int v = srcRowV[x / 2];

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBgrColor) {
                dstRow[(x + 0) * channels + 0] = b0;
                dstRow[(x + 0) * channels + 1] = g0;
                dstRow[(x + 0) * channels + 2] = r0;

                dstRow[(x + 1) * channels + 0] = b1;
                dstRow[(x + 1) * channels + 1] = g1;
                dstRow[(x + 1) * channels + 2] = r1;
            }
            else {
                dstRow[(x + 0) * channels + 0] = r0;
                dstRow[(x + 0) * channels + 1] = g0;
                dstRow[(x + 0) * channels + 2] = b0;

                dstRow[(x + 1) * channels + 0] = r1;
                dstRow[(x + 1) * channels + 1] = g1;
                dstRow[(x + 1) * channels + 2] = b1;
            }

            if constexpr (hasAlpha) {
                dstRow[(x + 0) * channels + 3] = 255;
                dstRow[(x + 1) * channels + 3] = 255;
            }
        }
    }
}

void nv12ToBgr24(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                 int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        nv12ToBgr24_avx2(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
        return;
    }

    nv12ToRgb_common<true, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void nv12ToRgb24(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                 int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        nv12ToRgb24_avx2(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
        return;
    }

    nv12ToRgb_common<false, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void nv12ToBgra32(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                  int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        nv12ToBgra32_avx2(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
        return;
    }

    nv12ToRgb_common<true, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void nv12ToRgba32(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                  int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        nv12ToRgba32_avx2(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
        return;
    }

    nv12ToRgb_common<false, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void i420ToBgr24(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        i420ToBgr24_avx2(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
        return;
    }

    i420ToRgb_common<true, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToRgb24(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        i420ToRgb24_avx2(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
        return;
    }

    i420ToRgb_common<false, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToBgra32(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        i420ToBgra32_avx2(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
        return;
    }

    i420ToRgb_common<true, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToRgba32(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag)
{
    if (hasAVX2()) {
        i420ToRgba32_avx2(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
        return;
    }

    i420ToRgb_common<false, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

} // namespace ccap