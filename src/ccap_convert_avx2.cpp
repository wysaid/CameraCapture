/**
 * @file ccap_convert_avx2.cpp
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#include "ccap_convert_avx2.h"

#include <cassert>

#ifdef CCAP_HAS_OPENMP
#include <omp.h>

// 设置OpenMP线程数为2（基于性能测试结果）
inline void setOptimalThreadCount() {
    static bool initialized = false;
    if (!initialized) {
        omp_set_num_threads(2);
        initialized = true;
    }
}
#endif

#if ENABLE_AVX2_IMP
/// macOS 上直接使用 Accelerate.framework, 暂时不需要单独实现
#include <immintrin.h> // AVX2

#if defined(_MSC_VER)
#include <intrin.h>
inline bool hasAVX2_() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    bool avx = (cpuInfo[2] & (1 << 28)) != 0;
    if (!(osxsave && avx)) return false;
    // 检查 XGETBV，确认 OS 支持 YMM
    unsigned long long xcrFeatureMask = _xgetbv(0);
    if ((xcrFeatureMask & 0x6) != 0x6) return false;
    // 检查 AVX2
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 5)) != 0;
}
#elif defined(__GNUC__) && (defined(_WIN32) || defined(__APPLE__))
#include <cpuid.h>
inline bool hasAVX2_() {
    unsigned int eax, ebx, ecx, edx;
    // 1. 检查 AVX 和 OSXSAVE
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return false;
    bool osxsave = (ecx & (1 << 27)) != 0;
    bool avx = (ecx & (1 << 28)) != 0;
    if (!(osxsave && avx)) return false;
    // 2. 检查 XGETBV
    unsigned int xcr0_lo = 0, xcr0_hi = 0;
#if defined(_XCR_XFEATURE_ENABLED_MASK)
    asm volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
#else
    asm volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
#endif
    if ((xcr0_lo & 0x6) != 0x6) return false;
    // 3. 检查 AVX2
    if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) return false;
    return (ebx & (1 << 5)) != 0;
}
#else
inline bool hasAVX2_() { return false; }
#endif

#endif

namespace ccap {
bool sEnableAVX2 = true;

void disableAVX2(bool disable) { sEnableAVX2 = !disable; }

bool hasAVX2() {
#if ENABLE_AVX2_IMP
    static bool s_hasAVX2 = hasAVX2_();
    return s_hasAVX2 && sEnableAVX2;
#else
    return false;
#endif
}
#if ENABLE_AVX2_IMP

template <int inputChannels, int outputChannels, int swapRB>
void colorShuffle_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width,
                       int height) { // Implement a general colorShuffle, accelerated by AVX2

    static_assert((inputChannels == 3 || inputChannels == 4) && (outputChannels == 3 || outputChannels == 4),
                  "inputChannels and outputChannels must be 3 or 4");

    static_assert(inputChannels != outputChannels || swapRB, "swapRB must be true when inputChannels == outputChannels");

    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    alignas(32) uint8_t shuffleData[32];
    constexpr uint32_t inputPatchSize = inputChannels == 4 ? 8 : 10;
    constexpr uint32_t outputPatchSize = outputChannels == 4 ? 8 : 10;
    constexpr uint32_t patchSize = inputPatchSize < outputPatchSize ? inputPatchSize : outputPatchSize;

    for (int i = 0; i < patchSize; ++i) {
        auto idx1 = i * outputChannels;
        auto idx2 = i * inputChannels;
        if constexpr (swapRB) {
            shuffleData[idx1] = 2 + idx2;     // B
            shuffleData[idx1 + 1] = 1 + idx2; // G
            shuffleData[idx1 + 2] = 0 + idx2; // R
        } else {
            shuffleData[idx1] = 0 + idx2;     // R
            shuffleData[idx1 + 1] = 1 + idx2; // G
            shuffleData[idx1 + 2] = 2 + idx2; // B
        }

        if constexpr (outputChannels == 4) {
            if constexpr (inputChannels == 4)
                shuffleData[idx1 + 3] = idx2 + 3; // A 永远在最后, 暂时不支持其他情况.
            else
                shuffleData[idx1 + 3] = 0xFF; // no alpha
        }
    }

#if 0
    //  打印一下 shuffleData
    printf("shuffleData: \n");
    for (int i = 0; i < patchSize; ++i)
    {
        for (int j = 0; j < outputChannels; ++j)
        {
            printf("%d ", shuffleData[i * outputChannels + j]);
        }
        printf("\n");
    }

    printf("\n");
#endif

    __m256i shuffle256;                                        // = _mm256_load_si256((const __m256i*)shuffleData);
    __m128i shuffle128;                                        // = _mm_load_si128((__m128i*)shuffleData);
    if constexpr (inputChannels == 4 && outputChannels == 4) { // 只有 4 -> 4 才能使用 256 位的 AVX2 指令
        shuffle256 = _mm256_load_si256((const __m256i*)shuffleData);
    } else {
        shuffle128 = _mm_load_si128((__m128i*)shuffleData);    }

#ifdef CCAP_HAS_OPENMP
    #pragma omp parallel for schedule(static) if(height >= 480)
#endif
    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        uint32_t x = 0;
        while (x + patchSize <= (uint32_t)width) {
            // _mm256_shuffle_epi8 can’t move these bytes across 16-byte lanes of the vector.
            // @see issue <https://stackoverflow.com/questions/77149094/how-to-use-mm256-shuffle-epi8-to-order-elements>
            if constexpr (outputChannels == 4 && inputChannels == 3) { // 3 -> 4, 需要拆分通道
                /// 拆分成 12 + 12, 每次读取 24 字节
                __m128i pixels_lo = _mm_loadu_si128((__m128i*)(srcRow + x * inputChannels));
                __m128i pixels_hi = _mm_loadu_si128((__m128i*)(srcRow + x * inputChannels + 12));

                __m128i result_lo = _mm_shuffle_epi8(pixels_lo, shuffle128);
                __m128i result_hi = _mm_shuffle_epi8(pixels_hi, shuffle128);

                // 创建alpha通道mask，将每个像素的第4个字节（alpha通道）设为0xFF
                // 对于RGBA格式，在小端字节序中，需要在正确的位置设置alpha
                __m128i alpha_mask = _mm_set1_epi32(0xFF000000);
                result_lo = _mm_or_si128(result_lo, alpha_mask);
                result_hi = _mm_or_si128(result_hi, alpha_mask);

                // outputChannels 是 4, patchSize 是 8,  对齐到 4 x 8 字节
                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels), result_lo);
                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels + 16), result_hi);
            } else if constexpr (outputChannels == 3 && inputChannels == 4) { // 4 -> 3
                /// 拆分成 16 + 16, 每次读取 32 字节
                __m128i pixels_lo = _mm_load_si128((__m128i*)(srcRow + x * inputChannels));
                __m128i pixels_hi = _mm_load_si128((__m128i*)(srcRow + x * inputChannels + 16));

                __m128i result_lo = _mm_shuffle_epi8(pixels_lo, shuffle128); // 只有前 12 字节有用
                __m128i result_hi = _mm_shuffle_epi8(pixels_hi, shuffle128); // 只有前 12 字节有用

                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels), result_lo); // 写入了 16 字节，但实际上只有前 12 字节有用
                alignas(16) uint8_t remainBuffer[16];
                _mm_store_si128((__m128i*)remainBuffer, result_hi);         // 暂存一下, 16 字节
                memcpy(dstRow + x * outputChannels + 12, remainBuffer, 12); // 手动补齐, 覆盖多余的 4 字节， 补齐剩下 12 字节, 刚好 24 字节
            } else if constexpr (inputChannels == 3 && outputChannels == 3) { // 3 -> 3
                /// 拆分成 15 + 15, 每次读取 30 字节
                __m128i pixels_lo = _mm_loadu_si128((__m128i*)(srcRow + x * inputChannels));
                __m128i pixels_hi = _mm_loadu_si128((__m128i*)(srcRow + x * inputChannels + 15));

                __m128i result_lo = _mm_shuffle_epi8(pixels_lo, shuffle128); // 只有前 15 字节有用
                __m128i result_hi = _mm_shuffle_epi8(pixels_hi, shuffle128); // 只有前 15 字节有用

                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels), result_lo); // 写入了 16 字节，但实际上只有前 15 字节有用
                alignas(16) uint8_t remainBuffer[16];
                _mm_store_si128((__m128i*)remainBuffer, result_hi);         // 暂存一下, 15 字节
                memcpy(dstRow + x * outputChannels + 15, remainBuffer, 15); // 手动补齐, 覆盖多余的 1 字节， 补齐剩下 15 字节, 刚好 30 字节
            } else {                                                        // 4 -> 4
                __m256i pixels = _mm256_loadu_si256((const __m256i*)(srcRow + x * inputChannels));
                __m256i result = _mm256_shuffle_epi8(pixels, shuffle256);
                _mm256_storeu_si256((__m256i*)(dstRow + x * outputChannels), result);
            }

            x += patchSize;        }        // Handle remaining pixels
        for (; x < (uint32_t)width; ++x) {
            for (int c = 0; c < outputChannels; ++c) {
                if (inputChannels == 3 && c == 3) {
                    dstRow[x * outputChannels + c] = 0xFF; // fill alpha
                } else {
                    dstRow[x * outputChannels + c] = srcRow[x * inputChannels + shuffleData[c]];
                    assert(shuffleData[c] <= 3);
                }
            }
        }
    }
}

template void colorShuffle_avx2<4, 4, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_avx2<4, 3, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_avx2<4, 3, false>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_avx2<3, 4, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_avx2<3, 4, false>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

template void colorShuffle_avx2<3, 3, true>(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height);

inline void getYuvToRgbCoefficients(bool isBT601, bool isFullRange, int& cy, int& cr, int& cgu, int& cgv, int& cb) {
    if (isBT601) {
        if (isFullRange) { // BT.601 Full Range: 256, 351, 86, 179, 443 (divided by 4)
            cy = 64;
            cr = 88;
            cgu = 22;
            cgv = 45;
            cb = 111;
        } else { // BT.601 Video Range: 298, 409, 100, 208, 516 (divided by 4)
            cy = 74;
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
            cy = 74;
            cr = 115;
            cgu = 14;
            cgv = 34;
            cb = 135;
        }
    }
}

template <bool isBGRA>
void nv12ToRgbaColor_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride,
                              int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    bool is601 = (flag & ConvertFlag::BT601) != 0;
    bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    // 根据标志选择系数
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);    __m256i c128 = _mm256_set1_epi16(128);
    __m128i a8 = _mm_set1_epi8((char)255);    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

#ifdef CCAP_HAS_OPENMP
    #pragma omp parallel for schedule(static) if(height >= 480)
#endif
    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
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

            // 7. 动态偏移计算
            // 所有情况下 UV 都要减 128
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            // Y 偏移取决于范围类型
            if (!isFullRange) { // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }

            // Full Range: Y 不变
            __m256i y_scaled = _mm256_mullo_epi16(y_16, c_y);

            __m256i r = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(v_16, c_r));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_gu));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c_gv));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_b));
            b = _mm256_add_epi16(b, _mm256_set1_epi16(32));
            b = _mm256_srai_epi16(b, 6);

            // clamp 0~255
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // 先将 16x16bit 压缩成 16x8bit，只用低128位
            __m128i r8 = _mm_packus_epi16(_mm256_castsi256_si128(r), _mm256_extracti128_si256(r, 1));
            __m128i g8 = _mm_packus_epi16(_mm256_castsi256_si128(g), _mm256_extracti128_si256(g, 1));
            __m128i b8 = _mm_packus_epi16(_mm256_castsi256_si128(b), _mm256_extracti128_si256(b, 1));

            if constexpr (isBGRA) {                           // 按 BGRA 顺序交错打包
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
            } else { // to RGBA
                // 按 RGBA 顺序交错打包
                __m128i rg0 = _mm_unpacklo_epi8(r8, g8);      // R0 G0 R1 G1 ...
                __m128i ba0 = _mm_unpacklo_epi8(b8, a8);      // B0 A0 B1 A1 ...
                __m128i rgba0 = _mm_unpacklo_epi16(rg0, ba0); // R0 G0 B0 A0 ...
                __m128i rgba1 = _mm_unpackhi_epi16(rg0, ba0); // R4 G4 B4 A4 ...

                __m128i rg1 = _mm_unpackhi_epi8(r8, g8);
                __m128i ba1 = _mm_unpackhi_epi8(b8, a8);
                __m128i rgba2 = _mm_unpacklo_epi16(rg1, ba1);
                __m128i rgba3 = _mm_unpackhi_epi16(rg1, ba1);

                // 写入 16*4=64 字节，正好16像素
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 0), rgba0);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), rgba1);

                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), rgba2);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), rgba3);
            }
        }

        for (; x < width; x += 2) {
            int y0 = yRow[x + 0];
            int y1 = yRow[x + 1];
            int u = uvRow[x];
            int v = uvRow[x + 1];

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBGRA) {
                dstRow[(x + 0) * 4 + 0] = b0;
                dstRow[(x + 0) * 4 + 1] = g0;
                dstRow[(x + 0) * 4 + 2] = r0;
                dstRow[(x + 0) * 4 + 3] = 255;

                dstRow[(x + 1) * 4 + 0] = b1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = r1;
                dstRow[(x + 1) * 4 + 3] = 255;
            } else {
                dstRow[(x + 0) * 4 + 0] = r0;
                dstRow[(x + 0) * 4 + 1] = g0;
                dstRow[(x + 0) * 4 + 2] = b0;
                dstRow[(x + 0) * 4 + 3] = 255;

                dstRow[(x + 1) * 4 + 0] = r1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = b1;
                dstRow[(x + 1) * 4 + 3] = 255;            }
        }
    }
}

template <bool isBGR>
void _nv12ToRgbColor_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride,
                              int width, int height, ConvertFlag flag) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    bool is601 = (flag & ConvertFlag::BT601) != 0;
    bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    // 根据标志选择系数
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);

    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

#ifdef CCAP_HAS_OPENMP
    #pragma omp parallel for schedule(static) if(height >= 480)
#endif
    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
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

            // 7. 动态偏移计算
            // 所有情况下 UV 都要减 128
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            // Y 偏移取决于范围类型
            if (!isFullRange) { // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }
            // Full Range: Y 不变

            __m256i y_scaled = _mm256_mullo_epi16(y_16, c_y);

            __m256i r = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(v_16, c_r));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_gu));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c_gv));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_b));
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

            for (int i = 0; i < 16; ++i) {
                if constexpr (isBGR) {
                    dstRow[(x + i) * 3 + 0] = (uint8_t)b_arr[i];
                    dstRow[(x + i) * 3 + 1] = (uint8_t)g_arr[i];
                    dstRow[(x + i) * 3 + 2] = (uint8_t)r_arr[i];
                } else {
                    dstRow[(x + i) * 3 + 0] = (uint8_t)r_arr[i];
                    dstRow[(x + i) * 3 + 1] = (uint8_t)g_arr[i];
                    dstRow[(x + i) * 3 + 2] = (uint8_t)b_arr[i];
                }
            }
        }

        // 处理剩余像素

        for (; x < width; x += 2) {
            int y0 = yRow[x + 0];
            int y1 = yRow[x + 1];
            // 修正UV索引计算
            int u = uvRow[x];     // U在偶数位置
            int v = uvRow[x + 1]; // V在奇数位置

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBGR) {
                dstRow[x * 3] = b0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = r0;

                dstRow[(x + 1) * 3] = b1;
                dstRow[(x + 1) * 3 + 1] = g1;
                dstRow[(x + 1) * 3 + 2] = r1;
            } else {
                dstRow[x * 3] = r0;
                dstRow[x * 3 + 1] = g0;
                dstRow[x * 3 + 2] = b0;

                dstRow[(x + 1) * 3] = r1;
                dstRow[(x + 1) * 3 + 1] = g1;
                dstRow[(x + 1) * 3 + 2] = b1;
            }
        }
    }
}

template <bool isBGRA>
void _i420ToRgba_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                          uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    bool is601 = (flag & ConvertFlag::BT601) != 0;
    bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    // 根据标志选择系数
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);

    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

#ifdef CCAP_HAS_OPENMP
    #pragma omp parallel for schedule(static) if(height >= 480)
#endif
    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
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

            // 5. 动态偏移计算
            // 所有情况下 UV 都要减 128
            u_16 = _mm256_sub_epi16(u_16, _mm256_set1_epi16(128));
            v_16 = _mm256_sub_epi16(v_16, _mm256_set1_epi16(128));

            // Y 偏移取决于范围类型
            if (!isFullRange) { // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }
            // Full Range: Y 不变

            __m256i y_scaled = _mm256_mullo_epi16(y_16, c_y);

            __m256i r = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(v_16, c_r));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_gu));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c_gv));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_b));
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

            if constexpr (isBGRA) {
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
            } else {
                __m128i rg0 = _mm_unpacklo_epi8(r8, g8);
                __m128i ba0 = _mm_unpacklo_epi8(b8, a8);
                __m128i rgba0 = _mm_unpacklo_epi16(rg0, ba0);
                __m128i rgba1 = _mm_unpackhi_epi16(rg0, ba0);

                __m128i rg1 = _mm_unpackhi_epi8(r8, g8);
                __m128i ba1 = _mm_unpackhi_epi8(b8, a8);
                __m128i rgba2 = _mm_unpacklo_epi16(rg1, ba1);
                __m128i rgba3 = _mm_unpackhi_epi16(rg1, ba1);

                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 0), rgba0);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), rgba1);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), rgba2);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), rgba3);
            }
        }

        // 处理剩余像素 (_i420ToRgba_avx2_imp)

        for (; x < width; x += 2) {
            int y0 = yRow[x + 0];
            int y1 = yRow[x + 1];
            int u = uRow[x / 2];
            int v = vRow[x / 2];

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBGRA) {
                dstRow[(x + 0) * 4 + 0] = b0;
                dstRow[(x + 0) * 4 + 1] = g0;
                dstRow[(x + 0) * 4 + 2] = r0;
                dstRow[(x + 0) * 4 + 3] = 255;

                dstRow[(x + 1) * 4 + 0] = b1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = r1;
                dstRow[(x + 1) * 4 + 3] = 255;
            } else {
                dstRow[(x + 0) * 4 + 0] = r0;
                dstRow[(x + 0) * 4 + 1] = g0;
                dstRow[(x + 0) * 4 + 2] = b0;
                dstRow[(x + 0) * 4 + 3] = 255;

                dstRow[(x + 1) * 4 + 0] = r1;
                dstRow[(x + 1) * 4 + 1] = g1;
                dstRow[(x + 1) * 4 + 2] = b1;
                dstRow[(x + 1) * 4 + 3] = 255;
            }
        }
    }
}

template <bool isBGR>
void _i420ToRgb_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                         uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    // 如果 height < 0，则反向写入 dst，src 顺序读取
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    bool is601 = (flag & ConvertFlag::BT601) != 0;
    bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    // 根据标志选择系数
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);

    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

#ifdef CCAP_HAS_OPENMP
    #pragma omp parallel for schedule(static) if(height >= 480)
#endif
    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
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

            // 5. 动态偏移计算
            // 所有情况下 UV 都要减 128
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            // Y 偏移取决于范围类型
            if (!isFullRange) {
                // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }
            // Full Range: Y 不变

            __m256i y_scaled = _mm256_mullo_epi16(y_16, c_y);

            __m256i r = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(v_16, c_r));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(32));
            r = _mm256_srai_epi16(r, 6);

            __m256i g = _mm256_sub_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_gu));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c_gv));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(32));
            g = _mm256_srai_epi16(g, 6);

            __m256i b = _mm256_add_epi16(y_scaled, _mm256_mullo_epi16(u_16, c_b));
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

            for (int i = 0; i < 16; ++i) {
                if constexpr (isBGR) {
                    dstRow[(x + i) * 3 + 0] = (uint8_t)b_arr[i];
                    dstRow[(x + i) * 3 + 1] = (uint8_t)g_arr[i];
                    dstRow[(x + i) * 3 + 2] = (uint8_t)r_arr[i];
                } else {
                    dstRow[(x + i) * 3 + 0] = (uint8_t)r_arr[i];
                    dstRow[(x + i) * 3 + 1] = (uint8_t)g_arr[i];
                    dstRow[(x + i) * 3 + 2] = (uint8_t)b_arr[i];
                }
            }
        }

        // 处理剩余像素
        for (; x < width; x += 2) {
            int y0 = yRow[x + 0];
            int y1 = yRow[x + 1];

            if (!isFullRange) {
                // Video Range: Y - 16
                y0 -= 16;
                y1 -= 16;
            }
            // Full Range: Y 不变

            int u = uRow[x / 2] - 128;
            int v = vRow[x / 2] - 128;

            int r0, g0, b0, r1, g1, b1;

            // 使用预先选择的转换函数
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBGR) {
                dstRow[(x + 0) * 3 + 0] = b0;
                dstRow[(x + 0) * 3 + 1] = g0;
                dstRow[(x + 0) * 3 + 2] = r0;

                dstRow[(x + 1) * 3 + 0] = b1;
                dstRow[(x + 1) * 3 + 1] = g1;
                dstRow[(x + 1) * 3 + 2] = r1;
            } else {
                dstRow[(x + 0) * 3 + 0] = r0;
                dstRow[(x + 0) * 3 + 1] = g0;
                dstRow[(x + 0) * 3 + 2] = b0;

                dstRow[(x + 1) * 3 + 0] = r1;
                dstRow[(x + 1) * 3 + 1] = g1;
                dstRow[(x + 1) * 3 + 2] = b1;
            }
        }
    }
}

// 基于 AVX2 加速
void nv12ToBgra32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                       int height, ConvertFlag flag) {
    nv12ToRgbaColor_avx2_imp<true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void nv12ToRgba32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                       int height, ConvertFlag flag) {
    nv12ToRgbaColor_avx2_imp<false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void nv12ToBgr24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                      int height, ConvertFlag flag) {
    _nv12ToRgbColor_avx2_imp<true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void nv12ToRgb24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                      int height, ConvertFlag flag) {
    _nv12ToRgbColor_avx2_imp<false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, flag);
}

void i420ToBgra32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    _i420ToRgba_avx2_imp<true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToRgba32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    _i420ToRgba_avx2_imp<false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToBgr24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    _i420ToRgb_avx2_imp<true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

void i420ToRgb24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    _i420ToRgb_avx2_imp<false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, flag);
}

#endif // ENABLE_AVX2_IMP
} // namespace ccap
