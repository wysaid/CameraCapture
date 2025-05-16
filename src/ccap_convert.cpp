/**
 * @file ccap_convert.cpp
 * @author wysaid (this@wysaid.org)
 * @brief pixel convert functions for ccap.
 * @date 2025-05
 *
 */

#include "ccap_convert.h"

#include <algorithm>

#if (defined(_MSC_VER) || defined(_WIN32)) && !defined(__arm__) && !defined(__aarch64__) && !defined(_M_ARM) && !defined(_M_ARM64)
#define ENABLE_AVX2_IMP 1
#else
#define ENABLE_AVX2_IMP 0
#endif

#if ENABLE_AVX2_IMP
/// macOS 上直接使用 Accelerate.framework, 暂时不需要单独实现
#include <immintrin.h> // AVX2

#if defined(_MSC_VER)
#include <intrin.h>
inline bool hasAVX2_()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    bool avx = (cpuInfo[2] & (1 << 28)) != 0;
    if (!(osxsave && avx))
        return false;
    // 检查 XGETBV，确认 OS 支持 YMM
    unsigned long long xcrFeatureMask = _xgetbv(0);
    if ((xcrFeatureMask & 0x6) != 0x6)
        return false;
    // 检查 AVX2
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 5)) != 0;
}
#elif defined(__GNUC__) && defined(_WIN32)
#include <cpuid.h>
inline bool hasAVX2_()
{
    unsigned int eax, ebx, ecx, edx;
    // 1. 检查 AVX 和 OSXSAVE
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        return false;
    bool osxsave = (ecx & (1 << 27)) != 0;
    bool avx = (ecx & (1 << 28)) != 0;
    if (!(osxsave && avx))
        return false;
    // 2. 检查 XGETBV
    unsigned int xcr0_lo = 0, xcr0_hi = 0;
#if defined(_XCR_XFEATURE_ENABLED_MASK)
    asm volatile("xgetbv"
                 : "=a"(xcr0_lo), "=d"(xcr0_hi)
                 : "c"(0));
#else
    asm volatile("xgetbv"
                 : "=a"(xcr0_lo), "=d"(xcr0_hi)
                 : "c"(0));
#endif
    if ((xcr0_lo & 0x6) != 0x6)
        return false;
    // 3. 检查 AVX2
    if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx))
        return false;
    return (ebx & (1 << 5)) != 0;
}
#else
inline bool hasAVX2_() { return false; }
#endif

#endif

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
} // namespace ccap

namespace
{ /// 内部调用的一些实现.

inline void yuv2rgb601(int y, int u, int v, int& r, int& g, int& b)
{
    r = (298 * y + 409 * v + 128) >> 8;
    g = (298 * y - 100 * u - 208 * v + 128) >> 8;
    b = (298 * y + 516 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

            dstRow[(x + 0) * 3 + 0] = b0;
            dstRow[(x + 0) * 3 + 1] = g0;
            dstRow[(x + 0) * 3 + 2] = r0;

            dstRow[(x + 1) * 3 + 0] = b1;
            dstRow[(x + 1) * 3 + 1] = g1;
            dstRow[(x + 1) * 3 + 2] = r1;
        }
    }
}

#else
#define nv12ToBGR24_AVX2(...) ((void)0)
#define nv12ToBGRA32_AVX2(...) ((void)0)
#define i420ToBGRA32_AVX2(...) ((void)0)
#define i420ToBGR24_AVX2(...) ((void)0)
#endif
} // namespace

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

void rgba2bgr(const uint8_t* src, int srcStride,
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

void rgba2rgb(const uint8_t* src, int srcStride,
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

void rgb2bgra(const uint8_t* src, int srcStride,
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

void rgb2rgba(const uint8_t* src, int srcStride,
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

void nv12ToBGRA32(const uint8_t* srcY, int srcYStride,
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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

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

void nv12ToBGR24(const uint8_t* srcY, int srcYStride,
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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

            dstRow[x * 3] = b0;
            dstRow[x * 3 + 1] = g0;
            dstRow[x * 3 + 2] = r0;

            dstRow[(x + 1) * 3] = b1;
            dstRow[(x + 1) * 3 + 1] = g1;
            dstRow[(x + 1) * 3 + 2] = r1;
        }
    }
}

void i420ToBGRA32(const uint8_t* srcY, int srcYStride,
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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

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

void i420ToBGR24(const uint8_t* srcY, int srcYStride,
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
            yuv2rgb601(y0, u, v, r0, g0, b0);
            yuv2rgb601(y1, u, v, r1, g1, b1);

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