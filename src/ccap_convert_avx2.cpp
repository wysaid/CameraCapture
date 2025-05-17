/**
 * @file ccap_convert_avx2.cpp
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#include "ccap_convert_avx2.h"

#include "ccap_convert.h"


namespace ccap
{
bool hasAVX2()
{
#if ENABLE_AVX2_IMP
    static bool s_hasAVX2 = hasAVX2_();
    return s_hasAVX2;
#else
    return false;
#endif
}
#if ENABLE_AVX2_IMP

// 基于 AVX2 加速
void nv12ToBGRA32_AVX2(const uint8_t* srcY, int srcYStride,
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
        for (; x + 16 <= width; x += 16)
        {
            // 1. 加载 16 个 Y
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. 加载 16 字节 UV（8 对）
            __m128i uv_vals = _mm_loadu_si128((const __m128i*)(uvRow + x));

            // 3. 拆分 U/V
            __m128i u8 = _mm_and_si128(uv_vals, _mm_set1_epi16(0x00FF));
            __m128i v8 = _mm_srli_epi16(uv_vals, 8);

            // 4. 打包成 8字节 U/V
            u8 = _mm_packus_epi16(u8, _mm_setzero_si128());
            v8 = _mm_packus_epi16(v8, _mm_setzero_si128());

            // 5. 每个U/V扩展为2个像素
            __m128i u_lo = _mm_unpacklo_epi8(u8, u8);
            __m128i v_lo = _mm_unpacklo_epi8(v8, v8);

            // 6. 拼成16字节
            __m256i u_16 = _mm256_cvtepu8_epi16(u_lo);
            __m256i v_16 = _mm256_cvtepu8_epi16(v_lo);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 7. 偏移
            y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            u_16 = _mm256_sub_epi16(u_16, _mm256_set1_epi16(128));
            v_16 = _mm256_sub_epi16(v_16, _mm256_set1_epi16(128));

            // 8. YUV -> RGB (BT.601), 数据相比 cpu 版有压缩, 精度可能有肉眼不可见的差异
            __m256i c74 = _mm256_set1_epi16(74);
            __m256i c102 = _mm256_set1_epi16(102);
            __m256i c25 = _mm256_set1_epi16(25);
            __m256i c52 = _mm256_set1_epi16(52);
            __m256i c129 = _mm256_set1_epi16(129);

            __m256i y74 = _mm256_mullo_epi16(y_16, c74);

            __m256i r = _mm256_add_epi16(y74, _mm256_mullo_epi16(v_16, c102));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y74, _mm256_mullo_epi16(u_16, c25));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c52));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y74, _mm256_mullo_epi16(u_16, c129));
            b = _mm256_add_epi16(b, _mm256_set1_epi16(32));
            b = _mm256_srai_epi16(b, 6);

            // clamp 0~255
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

#if 0
            // 打包 BGRA32
            alignas(32) uint16_t b_arr[16], g_arr[16], r_arr[16];
            _mm256_store_si256((__m256i*)b_arr, b);
            _mm256_store_si256((__m256i*)g_arr, g);
            _mm256_store_si256((__m256i*)r_arr, r);

            for (int i = 0; i < 16; ++i)
            {
                int idx = (x + i) * 4;
                dstRow[idx + 0] = (uint8_t)b_arr[i];
                dstRow[idx + 1] = (uint8_t)g_arr[i];
                dstRow[idx + 2] = (uint8_t)r_arr[i];
                dstRow[idx + 3] = 255;
            }
#else // 打包 BGRA32（AVX2优化，去掉循环）, 通常性能更好.

            // 先将 16x16bit 压缩成 16x8bit，只用低128位
            __m128i b8 = _mm_packus_epi16(_mm256_castsi256_si128(b), _mm256_extracti128_si256(b, 1));
            __m128i g8 = _mm_packus_epi16(_mm256_castsi256_si128(g), _mm256_extracti128_si256(g, 1));
            __m128i r8 = _mm_packus_epi16(_mm256_castsi256_si128(r), _mm256_extracti128_si256(r, 1));
            __m128i a8 = _mm_set1_epi8((char)255);

            // 按 BGRA 顺序交错打包
            __m128i bg0 = _mm_unpacklo_epi8(b8, g8);      // B0 G0 B1 G1 ...
            __m128i ra0 = _mm_unpacklo_epi8(r8, a8);      // R0 A0 R1 A1 ...
            __m128i bgra0 = _mm_unpacklo_epi16(bg0, ra0); // B0 G0 R0 A0 ...
            __m128i bgra1 = _mm_unpackhi_epi16(bg0, ra0); // B4 G4 R4 A4 ...

            __m128i bg1 = _mm_unpackhi_epi8(b8, g8);
            __m128i ra1 = _mm_unpackhi_epi8(r8, a8);
            __m128i bgra2 = _mm_unpacklo_epi16(bg1, ra1);
            __m128i bgra3 = _mm_unpackhi_epi16(bg1, ra1);

            // 写入 16*4=64 字节，正好16像素
            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 0), bgra0);
            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), bgra1);

            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), bgra2);
            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), bgra3);
#endif
        }

        // 处理剩余像素
        for (; x < width; x += 2)
        {
            int y0 = yRow[x + 0] - 16;
            int y1 = yRow[x + 1] - 16;
            int u = uvRow[x] - 128;
            int v = uvRow[x + 1] - 128;

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

void nv12ToBGR24_AVX2(const uint8_t* srcY, int srcYStride,
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
        for (; x + 16 <= width; x += 16)
        {
            // 1. 加载 16 个 Y
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. 加载 16 字节 UV（8 对）
            __m128i uv_vals = _mm_loadu_si128((const __m128i*)(uvRow + x));

            // 3. 拆分 U/V
            __m128i u8 = _mm_and_si128(uv_vals, _mm_set1_epi16(0x00FF)); // U: 0,2,4...
            __m128i v8 = _mm_srli_epi16(uv_vals, 8);                     // V: 1,3,5...

            // 4. 打包成 8字节 U/V
            u8 = _mm_packus_epi16(u8, _mm_setzero_si128()); // 低8字节是U
            v8 = _mm_packus_epi16(v8, _mm_setzero_si128()); // 低8字节是V

            // 5. 每个U/V扩展为2个像素
            __m128i u_lo = _mm_unpacklo_epi8(u8, u8); // U0,U0,U1,U1,...
            __m128i v_lo = _mm_unpacklo_epi8(v8, v8); // V0,V0,V1,V1,...

            // 6. 拼成16字节
            __m256i u_16 = _mm256_cvtepu8_epi16(u_lo);
            __m256i v_16 = _mm256_cvtepu8_epi16(v_lo);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 7. 偏移
            y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            u_16 = _mm256_sub_epi16(u_16, _mm256_set1_epi16(128));
            v_16 = _mm256_sub_epi16(v_16, _mm256_set1_epi16(128));

            // 8. YUV -> RGB (BT.601), 数据相比 cpu 版有压缩, 精度可能有肉眼不可见的差异
            __m256i c74 = _mm256_set1_epi16(74);
            __m256i c102 = _mm256_set1_epi16(102);
            __m256i c25 = _mm256_set1_epi16(25);
            __m256i c52 = _mm256_set1_epi16(52);
            __m256i c129 = _mm256_set1_epi16(129);

            __m256i y74 = _mm256_mullo_epi16(y_16, c74);

            __m256i r = _mm256_add_epi16(y74, _mm256_mullo_epi16(v_16, c102));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y74, _mm256_mullo_epi16(u_16, c25));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c52));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y74, _mm256_mullo_epi16(u_16, c129));
            b = _mm256_add_epi16(b, _mm256_set1_epi16(32));
            b = _mm256_srai_epi16(b, 6);

            // clamp 0~255
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // 打包 BGR24
            alignas(32) uint16_t b_arr[16], g_arr[16], r_arr[16];
            _mm256_store_si256((__m256i*)b_arr, b);
            _mm256_store_si256((__m256i*)g_arr, g);
            _mm256_store_si256((__m256i*)r_arr, r);

            for (int i = 0; i < 16; ++i)
            {
                dstRow[(x + i) * 3 + 0] = (uint8_t)b_arr[i];
                dstRow[(x + i) * 3 + 1] = (uint8_t)g_arr[i];
                dstRow[(x + i) * 3 + 2] = (uint8_t)r_arr[i];
            }
        }

        // 处理剩余像素
        for (; x < width; x += 2)
        {
            int y0 = yRow[x + 0] - 16;
            int y1 = yRow[x + 1] - 16;
            // 修正UV索引计算
            int u = uvRow[x] - 128;     // U在偶数位置
            int v = uvRow[x + 1] - 128; // V在奇数位置

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

void i420ToBGRA32_AVX2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
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
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16)
        {
            // 1. 加载16个Y
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. 加载8个U/V
            __m128i u8 = _mm_loadl_epi64((const __m128i*)(uRow + x / 2));
            __m128i v8 = _mm_loadl_epi64((const __m128i*)(vRow + x / 2));

            // 3. 每个U/V扩展为2个像素
            __m128i u16 = _mm_unpacklo_epi8(u8, u8); // U0,U0,U1,U1,...
            __m128i v16 = _mm_unpacklo_epi8(v8, v8); // V0,V0,V1,V1,...

            // 4. 拼成16字节
            __m256i u_16 = _mm256_cvtepu8_epi16(u16);
            __m256i v_16 = _mm256_cvtepu8_epi16(v16);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 5. 偏移
            y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            u_16 = _mm256_sub_epi16(u_16, _mm256_set1_epi16(128));
            v_16 = _mm256_sub_epi16(v_16, _mm256_set1_epi16(128));

            // 6. YUV -> RGB (BT.601)
            __m256i c74 = _mm256_set1_epi16(74);
            __m256i c102 = _mm256_set1_epi16(102);
            __m256i c25 = _mm256_set1_epi16(25);
            __m256i c52 = _mm256_set1_epi16(52);
            __m256i c129 = _mm256_set1_epi16(129);

            __m256i y74 = _mm256_mullo_epi16(y_16, c74);

            __m256i r = _mm256_add_epi16(y74, _mm256_mullo_epi16(v_16, c102));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y74, _mm256_mullo_epi16(u_16, c25));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c52));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y74, _mm256_mullo_epi16(u_16, c129));
            b = _mm256_add_epi16(b, _mm256_set1_epi16(32));
            b = _mm256_srai_epi16(b, 6);

            // clamp 0~255
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // 打包 BGRA32
            __m128i b8 = _mm_packus_epi16(_mm256_castsi256_si128(b), _mm256_extracti128_si256(b, 1));
            __m128i g8 = _mm_packus_epi16(_mm256_castsi256_si128(g), _mm256_extracti128_si256(g, 1));
            __m128i r8 = _mm_packus_epi16(_mm256_castsi256_si128(r), _mm256_extracti128_si256(r, 1));
            __m128i a8 = _mm_set1_epi8((char)255);

            __m128i bg0 = _mm_unpacklo_epi8(b8, g8);
            __m128i ra0 = _mm_unpacklo_epi8(r8, a8);
            __m128i bgra0 = _mm_unpacklo_epi16(bg0, ra0);
            __m128i bgra1 = _mm_unpackhi_epi16(bg0, ra0);

            __m128i bg1 = _mm_unpackhi_epi8(b8, g8);
            __m128i ra1 = _mm_unpackhi_epi8(r8, a8);
            __m128i bgra2 = _mm_unpacklo_epi16(bg1, ra1);
            __m128i bgra3 = _mm_unpackhi_epi16(bg1, ra1);

            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 0), bgra0);
            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), bgra1);
            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), bgra2);
            _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), bgra3);
        }

        // 处理剩余像素
        for (; x < width; x += 2)
        {
            int y0 = yRow[x + 0] - 16;
            int y1 = yRow[x + 1] - 16;
            int u = uRow[x / 2] - 128;
            int v = vRow[x / 2] - 128;

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

void i420ToBGR24_AVX2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
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
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16)
        {
            // 1. 加载16个Y
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. 加载8个U/V
            __m128i u8 = _mm_loadl_epi64((const __m128i*)(uRow + x / 2));
            __m128i v8 = _mm_loadl_epi64((const __m128i*)(vRow + x / 2));

            // 3. 每个U/V扩展为2个像素
            __m128i u16 = _mm_unpacklo_epi8(u8, u8); // U0,U0,U1,U1,...
            __m128i v16 = _mm_unpacklo_epi8(v8, v8); // V0,V0,V1,V1,...

            // 4. 拼成16字节
            __m256i u_16 = _mm256_cvtepu8_epi16(u16);
            __m256i v_16 = _mm256_cvtepu8_epi16(v16);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 5. 偏移
            y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            u_16 = _mm256_sub_epi16(u_16, _mm256_set1_epi16(128));
            v_16 = _mm256_sub_epi16(v_16, _mm256_set1_epi16(128));

            // 6. YUV -> RGB (BT.601)
            __m256i c74 = _mm256_set1_epi16(74);
            __m256i c102 = _mm256_set1_epi16(102);
            __m256i c25 = _mm256_set1_epi16(25);
            __m256i c52 = _mm256_set1_epi16(52);
            __m256i c129 = _mm256_set1_epi16(129);

            __m256i y74 = _mm256_mullo_epi16(y_16, c74);

            __m256i r = _mm256_add_epi16(y74, _mm256_mullo_epi16(v_16, c102));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y74, _mm256_mullo_epi16(u_16, c25));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c52));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y74, _mm256_mullo_epi16(u_16, c129));
            b = _mm256_add_epi16(b, _mm256_set1_epi16(32));
            b = _mm256_srai_epi16(b, 6);

            // clamp 0~255
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // 打包 BGR24
            alignas(32) uint16_t b_arr[16], g_arr[16], r_arr[16];
            _mm256_store_si256((__m256i*)b_arr, b);
            _mm256_store_si256((__m256i*)g_arr, g);
            _mm256_store_si256((__m256i*)r_arr, r);

            for (int i = 0; i < 16; ++i)
            {
                dstRow[(x + i) * 3 + 0] = (uint8_t)b_arr[i];
                dstRow[(x + i) * 3 + 1] = (uint8_t)g_arr[i];
                dstRow[(x + i) * 3 + 2] = (uint8_t)r_arr[i];
            }
        }

        // 处理剩余像素
        for (; x < width; x += 2)
        {
            int y0 = yRow[x + 0] - 16;
            int y1 = yRow[x + 1] - 16;
            int u = uRow[x / 2] - 128;
            int v = vRow[x / 2] - 128;

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

#endif // ENABLE_AVX2_IMP
} // namespace ccap
