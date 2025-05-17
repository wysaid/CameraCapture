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

namespace ccap
{
// 交换 R 和 B，G 不变，支持 height < 0 上下翻转
void rgbShuffle(const uint8_t* src, int srcStride,
                uint8_t* dst, int dstStride,
                int width, int height,
                const uint8_t shuffle[3])
{
    if (height < 0)
    {
        height = -height;
        src = src + (height - 1) * srcStride;
        srcStride = -srcStride;
    }
    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        for (int x = 0; x < width; ++x)
        {
            dstRow[0] = srcRow[shuffle[0]];
            dstRow[1] = srcRow[shuffle[1]];
            dstRow[2] = srcRow[shuffle[2]];
            srcRow += 3;
            dstRow += 3;
        }
    }
}

void rgbaShuffle(const uint8_t* src, int srcStride,
                 uint8_t* dst, int dstStride,
                 int width, int height,
                 const uint8_t shuffle[4])
{
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }
    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        for (int x = 0; x < width; ++x)
        {
            dstRow[0] = srcRow[shuffle[0]];
            dstRow[1] = srcRow[shuffle[1]];
            dstRow[2] = srcRow[shuffle[2]];
            dstRow[3] = srcRow[shuffle[3]];
            srcRow += 4;
            dstRow += 4;
        }
    }
}

void rgbaToBgr(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height)
{
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        for (int x = 0; x < width; ++x)
        {
            // RGBA -> BGR (去掉A)
            dstRow[0] = srcRow[2]; // B
            dstRow[1] = srcRow[1]; // G
            dstRow[2] = srcRow[0]; // R
            srcRow += 4;
            dstRow += 3;
        }
    }
}

void rgbaToRgb(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height)
{
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        for (int x = 0; x < width; ++x)
        {
            // RGBA -> RGB (去掉A)
            dstRow[0] = srcRow[2]; // R
            dstRow[1] = srcRow[1]; // G
            dstRow[2] = srcRow[0]; // B
            srcRow += 4;
            dstRow += 3;
        }
    }
}

void rgbToBgra(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height)
{
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        for (int x = 0; x < width; ++x)
        {
            // RGB -> BGRA (添加A)
            dstRow[0] = srcRow[2]; // B
            dstRow[1] = srcRow[1]; // G
            dstRow[2] = srcRow[0]; // R
            dstRow[3] = 255;       // A
            srcRow += 3;
            dstRow += 4;
        }
    }
}

void rgbToRgba(const uint8_t* src, int srcStride,
               uint8_t* dst, int dstStride,
               int width, int height)
{
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        for (int x = 0; x < width; ++x)
        {
            // RGB -> RGBA (添加A)
            dstRow[0] = srcRow[0]; // R
            dstRow[1] = srcRow[1]; // G
            dstRow[2] = srcRow[2]; // B
            dstRow[3] = 255;       // A
            srcRow += 3;
            dstRow += 4;
        }
    }
}

void nv12ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcUV, int srcUVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height)
{
    if (hasAVX2())
    {
        nv12ToBGRA32_AVX2(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height);
        return;
    }

    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRowY = srcY + y * srcYStride;
        const uint8_t* srcRowUV = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        for (int x = 0; x < width; x += 2)
        {
            int y0 = srcRowY[x + 0] - 16;
            int y1 = srcRowY[x + 1] - 16;
            int u = srcRowUV[x] - 128;
            int v = srcRowUV[x + 1] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            dstRow[(x + 0) * 4 + 0] = b0;
            dstRow[(x + 0) * 4 + 1] = g0;
            dstRow[(x + 0) * 4 + 2] = r0;
            dstRow[(x + 0) * 4 + 3] = 255;

            dstRow[(x + 1) * 4 + 0] = b1;
            dstRow[(x + 1) * 4 + 1] = g1;
            dstRow[(x + 1) * 4 + 2] = r1;
            dstRow[(x + 1) * 4 + 3] = 255;
        }
    }
}

void nv12ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcUV, int srcUVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height)
{
    if (hasAVX2())
    {
        nv12ToBGR24_AVX2(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height);
        return;
    }

    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRowY = srcY + y * srcYStride;
        const uint8_t* srcRowUV = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        for (int x = 0; x < width; x += 2)
        {
            int y0 = srcRowY[x + 0] - 16;
            int y1 = srcRowY[x + 1] - 16;
            int u = srcRowUV[x] - 128;
            int v = srcRowUV[x + 1] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            dstRow[x * 3] = b0;
            dstRow[x * 3 + 1] = g0;
            dstRow[x * 3 + 2] = r0;

            dstRow[(x + 1) * 3] = b1;
            dstRow[(x + 1) * 3 + 1] = g1;
            dstRow[(x + 1) * 3 + 2] = r1;
        }
    }
}

void i420ToBgra32(const uint8_t* srcY, int srcYStride,
                  const uint8_t* srcU, int srcUStride,
                  const uint8_t* srcV, int srcVStride,
                  uint8_t* dst, int dstStride,
                  int width, int height)
{
    if (hasAVX2())
    {
        i420ToBGRA32_AVX2(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height);
        return;
    }

    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRowY = srcY + y * srcYStride;
        const uint8_t* srcRowU = srcU + (y / 2) * srcUStride;
        const uint8_t* srcRowV = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        for (int x = 0; x < width; x += 2)
        {
            int y0 = srcRowY[x + 0] - 16;
            int y1 = srcRowY[x + 1] - 16;
            int u = srcRowU[x / 2] - 128;
            int v = srcRowV[x / 2] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            dstRow[(x + 0) * 4 + 0] = b0;
            dstRow[(x + 0) * 4 + 1] = g0;
            dstRow[(x + 0) * 4 + 2] = r0;
            dstRow[(x + 0) * 4 + 3] = 255;

            dstRow[(x + 1) * 4 + 0] = b1;
            dstRow[(x + 1) * 4 + 1] = g1;
            dstRow[(x + 1) * 4 + 2] = r1;
            dstRow[(x + 1) * 4 + 3] = 255;
        }
    }
}

void i420ToBgr24(const uint8_t* srcY, int srcYStride,
                 const uint8_t* srcU, int srcUStride,
                 const uint8_t* srcV, int srcVStride,
                 uint8_t* dst, int dstStride,
                 int width, int height)
{
    if (hasAVX2())
    {
        i420ToBGR24_AVX2(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height);
        return;
    }

    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRowY = srcY + y * srcYStride;
        const uint8_t* srcRowU = srcU + (y / 2) * srcUStride;
        const uint8_t* srcRowV = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        for (int x = 0; x < width; x += 2)
        {
            int y0 = srcRowY[x + 0] - 16;
            int y1 = srcRowY[x + 1] - 16;
            int u = srcRowU[x / 2] - 128;
            int v = srcRowV[x / 2] - 128;

            int r0, g0, b0, r1, g1, b1;
            yuv2rgb601v(y0, u, v, r0, g0, b0);
            yuv2rgb601v(y1, u, v, r1, g1, b1);

            dstRow[(x + 0) * 3 + 0] = b0;
            dstRow[(x + 0) * 3 + 1] = g0;
            dstRow[(x + 0) * 3 + 2] = r0;

            dstRow[(x + 1) * 3 + 0] = b1;
            dstRow[(x + 1) * 3 + 1] = g1;
            dstRow[(x + 1) * 3 + 2] = r1;
        }
    }
}

} // namespace ccap