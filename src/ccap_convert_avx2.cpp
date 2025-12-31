/**
 * @file ccap_convert_avx2.cpp
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#include "ccap_convert_avx2.h"

#include <cassert>
#include <cstring>

#if ENABLE_AVX2_IMP
/// On macOS, use Accelerate.framework directly, no separate implementation needed for now

// Add target attribute support for GCC/MinGW
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#define AVX2_TARGET __attribute__((target("avx2,fma")))
#else
#define AVX2_TARGET
#endif

#include <immintrin.h> // AVX2

#if defined(_MSC_VER)
#include <intrin.h>
inline bool hasAVX2_() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    bool avx = (cpuInfo[2] & (1 << 28)) != 0;
    if (!(osxsave && avx)) return false;
    // Check XGETBV to confirm OS supports YMM
    unsigned long long xcrFeatureMask = _xgetbv(0);
    if ((xcrFeatureMask & 0x6) != 0x6) return false;
    // Check AVX2
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 5)) != 0;
}
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
inline bool hasAVX2_() {
    unsigned int eax, ebx, ecx, edx;

    // 1. Check basic CPUID support
    if (!__get_cpuid(0, &eax, &ebx, &ecx, &edx)) return false;
    if (eax < 1) return false; // Need support for CPUID function 1

    // 2. Check AVX and OSXSAVE
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return false;
    bool osxsave = (ecx & (1 << 27)) != 0;
    bool avx = (ecx & (1 << 28)) != 0;
    if (!(osxsave && avx)) return false;

    // 3. Check XGETBV to confirm OS supports YMM
    // Only safe to call XGETBV when OSXSAVE is true
    unsigned int xcr0_lo = 0, xcr0_hi = 0;
    asm volatile("xgetbv"
                 : "=a"(xcr0_lo), "=d"(xcr0_hi)
                 : "c"(0));
    if ((xcr0_lo & 0x6) != 0x6) return false; // Both XMM and YMM states must be saved

    // 4. Check extended feature support
    if (!__get_cpuid(0, &eax, &ebx, &ecx, &edx)) return false;
    if (eax < 7) return false; // Need support for CPUID function 7

    // 5. Check AVX2
    if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) return false;
    return (ebx & (1 << 5)) != 0; // AVX2 bit
}
#else
inline bool hasAVX2_() { return false; }
#endif

#endif

namespace ccap {
bool sEnableAVX2 = true;

bool enableAVX2(bool enable) {
    sEnableAVX2 = enable;
    return hasAVX2(); // Re-check AVX2 support
}

bool hasAVX2() {
#if ENABLE_AVX2_IMP
    static bool s_hasAVX2 = hasAVX2_();
    return s_hasAVX2;
#else
    return false;
#endif
}

bool canUseAVX2() {
    return hasAVX2() && sEnableAVX2;
}

const char* getAVX2SupportInfo() {
#if ENABLE_AVX2_IMP
    static const char* info = nullptr;
    if (info == nullptr) {
        if (hasAVX2()) {
            if (sEnableAVX2) {
                info = "AVX2: Hardware supported and enabled";
            } else {
                info = "AVX2: Hardware supported but disabled by software";
            }
        } else {
            info = "AVX2: Not supported by hardware or OS";
        }
    }
    return info;
#else
    return "AVX2: Disabled at compile time";
#endif
}

#if ENABLE_AVX2_IMP

template <int inputChannels, int outputChannels, int swapRB>
AVX2_TARGET void colorShuffle_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width,
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
    constexpr uint32_t inputPatchSize = inputChannels == 4 ? 8 : (inputChannels == 3 && outputChannels == 3 ? 5 : 10);
    constexpr uint32_t outputPatchSize = outputChannels == 4 ? 8 : (inputChannels == 3 && outputChannels == 3 ? 5 : 10);
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
                shuffleData[idx1 + 3] = idx2 + 3; // A is always at the end, other cases not supported for now.
            else
                shuffleData[idx1 + 3] = 0xFF; // no alpha
        }
    }

#if 0
    // Print shuffleData for debugging
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
    if constexpr (inputChannels == 4 && outputChannels == 4) { // Only 4 -> 4 can use 256-bit AVX2 instructions
        shuffle256 = _mm256_load_si256((const __m256i*)shuffleData);
    } else {
        shuffle128 = _mm_load_si128((__m128i*)shuffleData);
    }

    // Different cases require different boundary conditions to avoid reading beyond allocated memory:
    // - 3->4: reads 16 bytes from x*3+12, needs x*3+27 < width*3, i.e., x+9 < width
    // - 3->3: reads 16 bytes from x*3, needs x*3+15 < width*3, i.e., x+5 < width
    // - 4->3: reads 16 bytes from x*4+16, needs x*4+31 < width*4, i.e., x+8 <= width
    // - 4->4: reads 32 bytes from x*4, needs x*4+31 < width*4, i.e., x+8 <= width
    constexpr uint32_t loopBoundary = (inputChannels == 3 && outputChannels == 4) ? (patchSize + 2) :
        (inputChannels == 3 && outputChannels == 3)                               ? (patchSize + 1) :
                                                                                    patchSize;

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        uint32_t x = 0;

        while (x + loopBoundary <= (uint32_t)width) {
            // _mm256_shuffle_epi8 can’t move these bytes across 16-byte lanes of the vector.
            // @see issue <https://stackoverflow.com/questions/77149094/how-to-use-mm256-shuffle-epi8-to-order-elements>
            if constexpr (outputChannels == 4 && inputChannels == 3) { // 3 -> 4, need to split channels
                /// Split into 12 + 12, reading 24 bytes each time
                __m128i pixels_lo = _mm_loadu_si128((__m128i*)(srcRow + x * inputChannels));
                __m128i pixels_hi = _mm_loadu_si128((__m128i*)(srcRow + x * inputChannels + 12));

                __m128i result_lo = _mm_shuffle_epi8(pixels_lo, shuffle128);
                __m128i result_hi = _mm_shuffle_epi8(pixels_hi, shuffle128);

                // Create alpha channel mask, set the 4th byte (alpha channel) of each pixel to 0xFF
                // For RGBA format in little-endian, need to set alpha at correct position
                __m128i alpha_mask = _mm_set1_epi32(0xFF000000);
                result_lo = _mm_or_si128(result_lo, alpha_mask);
                result_hi = _mm_or_si128(result_hi, alpha_mask);

                // outputChannels is 4, patchSize is 8, align to 4 x 8 bytes
                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels), result_lo);
                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels + 16), result_hi);
            } else if constexpr (outputChannels == 3 && inputChannels == 4) { // 4 -> 3
                /// Split into 16 + 16, reading 32 bytes each time
                __m128i pixels_lo = _mm_load_si128((__m128i*)(srcRow + x * inputChannels));
                __m128i pixels_hi = _mm_load_si128((__m128i*)(srcRow + x * inputChannels + 16));

                __m128i result_lo = _mm_shuffle_epi8(pixels_lo, shuffle128); // Only the first 12 bytes are useful
                __m128i result_hi = _mm_shuffle_epi8(pixels_hi, shuffle128); // Only the first 12 bytes are useful

                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels), result_lo); // Write 16 bytes, but only the first 12 bytes are useful
                alignas(16) uint8_t remainBuffer[16];
                _mm_store_si128((__m128i*)remainBuffer, result_hi);           // Temporarily store, 16 bytes
                memcpy(dstRow + x * outputChannels + 12, remainBuffer, 12);   // Manual alignment, overwrite extra 4 bytes, fill remaining 12 bytes, exactly 24 bytes
            } else if constexpr (inputChannels == 3 && outputChannels == 3) { // 3 -> 3
                /// Process 5 pixels at a time (15 bytes), reading 16 bytes each time
                __m128i pixels = _mm_loadu_si128((__m128i*)(srcRow + x * inputChannels));

                __m128i result = _mm_shuffle_epi8(pixels, shuffle128); // Only the first 15 bytes are useful

                _mm_storeu_si128((__m128i*)(dstRow + x * outputChannels), result); // Write 16 bytes, but only the first 15 bytes are useful
            } else {                                                               // 4 -> 4
                __m256i pixels = _mm256_loadu_si256((const __m256i*)(srcRow + x * inputChannels));
                __m256i result = _mm256_shuffle_epi8(pixels, shuffle256);
                _mm256_storeu_si256((__m256i*)(dstRow + x * outputChannels), result);
            }

            x += patchSize;
        }
        // Handle remaining pixels
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

template <bool isBGRA, bool isFullRange>
AVX2_TARGET void nv12ToRgbaColor_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride,
                                          int width, int height, bool is601) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Select coefficients based on flags
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);
    __m128i a8 = _mm_set1_epi8((char)255);

    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. Load 16 bytes UV (8 pairs)
            __m128i uv_vals = _mm_loadu_si128((const __m128i*)(uvRow + x));

            // 3. Split U/V
            __m128i u8 = _mm_and_si128(uv_vals, _mm_set1_epi16(0x00FF));
            __m128i v8 = _mm_srli_epi16(uv_vals, 8);

            // 4. Pack into 8-byte U/V
            u8 = _mm_packus_epi16(u8, _mm_setzero_si128());
            v8 = _mm_packus_epi16(v8, _mm_setzero_si128());

            // 5. Expand each U/V to 2 pixels
            __m128i u_lo = _mm_unpacklo_epi8(u8, u8);
            __m128i v_lo = _mm_unpacklo_epi8(v8, v8);

            // 6. Combine into 16 bytes
            __m256i u_16 = _mm256_cvtepu8_epi16(u_lo);
            __m256i v_16 = _mm256_cvtepu8_epi16(v_lo);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 7. Dynamic offset calculation
            // UV always subtract 128 in all cases
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            // Y offset depends on range type
            if constexpr (!isFullRange) { // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }

            // Full Range: Y remains unchanged
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

            // Clamp to 0~255
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // First compress 16x16bit to 16x8bit, only use lower 128 bits
            __m128i r8 = _mm_packus_epi16(_mm256_castsi256_si128(r), _mm256_extracti128_si256(r, 1));
            __m128i g8 = _mm_packus_epi16(_mm256_castsi256_si128(g), _mm256_extracti128_si256(g, 1));
            __m128i b8 = _mm_packus_epi16(_mm256_castsi256_si128(b), _mm256_extracti128_si256(b, 1));

            if constexpr (isBGRA) {                           // Interleave pack in BGRA order
                __m128i bg0 = _mm_unpacklo_epi8(b8, g8);      // B0 G0 B1 G1 ...
                __m128i ra0 = _mm_unpacklo_epi8(r8, a8);      // R0 A0 R1 A1 ...
                __m128i bgra0 = _mm_unpacklo_epi16(bg0, ra0); // B0 G0 R0 A0 ...
                __m128i bgra1 = _mm_unpackhi_epi16(bg0, ra0); // B4 G4 R4 A4 ...

                __m128i bg1 = _mm_unpackhi_epi8(b8, g8);
                __m128i ra1 = _mm_unpackhi_epi8(r8, a8);
                __m128i bgra2 = _mm_unpacklo_epi16(bg1, ra1);
                __m128i bgra3 = _mm_unpackhi_epi16(bg1, ra1);

                // Write 16*4=64 bytes, exactly 16 pixels
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 0), bgra0);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), bgra1);

                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), bgra2);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), bgra3);
            } else { // to RGBA
                // Interleave pack in RGBA order
                __m128i rg0 = _mm_unpacklo_epi8(r8, g8);      // R0 G0 R1 G1 ...
                __m128i ba0 = _mm_unpacklo_epi8(b8, a8);      // B0 A0 B1 A1 ...
                __m128i rgba0 = _mm_unpacklo_epi16(rg0, ba0); // R0 G0 B0 A0 ...
                __m128i rgba1 = _mm_unpackhi_epi16(rg0, ba0); // R4 G4 B4 A4 ...

                __m128i rg1 = _mm_unpackhi_epi8(r8, g8);
                __m128i ba1 = _mm_unpackhi_epi8(b8, a8);
                __m128i rgba2 = _mm_unpacklo_epi16(rg1, ba1);
                __m128i rgba3 = _mm_unpackhi_epi16(rg1, ba1);

                // Write 16*4=64 bytes, exactly 16 pixels
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 0), rgba0);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), rgba1);

                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), rgba2);
                _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), rgba3);
            }
        }

        for (; x < width; x += 2) {
            int y0 = yRow[x];
            int y1 = yRow[x + 1];
            int u = uvRow[x];
            int v = uvRow[x + 1];

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

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

template <bool isBGR, bool isFullRange>
AVX2_TARGET void _nv12ToRgbColor_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride,
                                          int width, int height, bool is601) {
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Select coefficients based on flags
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);

    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uvRow = srcUV + (y / 2) * srcUVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. Load 16 bytes UV (8 pairs)
            __m128i uv_vals = _mm_loadu_si128((const __m128i*)(uvRow + x));

            // 3. Split U/V
            __m128i u8 = _mm_and_si128(uv_vals, _mm_set1_epi16(0x00FF)); // U: 0,2,4...
            __m128i v8 = _mm_srli_epi16(uv_vals, 8);                     // V: 1,3,5...

            // 4. Pack into 8-byte U/V
            u8 = _mm_packus_epi16(u8, _mm_setzero_si128()); // Lower 8 bytes are U
            v8 = _mm_packus_epi16(v8, _mm_setzero_si128()); // Lower 8 bytes are V

            // 5. Expand each U/V to 2 pixels
            __m128i u_lo = _mm_unpacklo_epi8(u8, u8); // U0,U0,U1,U1,...
            __m128i v_lo = _mm_unpacklo_epi8(v8, v8); // V0,V0,V1,V1,...

            // 6. Combine into 16 bytes
            __m256i u_16 = _mm256_cvtepu8_epi16(u_lo);
            __m256i v_16 = _mm256_cvtepu8_epi16(v_lo);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 7. Dynamic offset calculation
            // UV always subtract 128 in all cases
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            // Y offset depends on range type
            if constexpr (!isFullRange) { // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }
            // Full Range: Y remains unchanged

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

            // Pack BGR24
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

        // Handle remaining pixels

        for (; x < width; x += 2) {
            int y0 = yRow[x + 0];
            int y1 = yRow[x + 1];
            // Correct UV index calculation
            int u = uvRow[x];     // U at even position
            int v = uvRow[x + 1]; // V at odd position

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

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

template <bool isBGRA, bool isFullRange>
AVX2_TARGET void _i420ToRgba_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                                      uint8_t* dst, int dstStride, int width, int height, bool is601) {
    // If height < 0, write dst in reverse order while reading src in normal order
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Select coefficients based on flags
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);

    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. Load 8 U/V values
            __m128i u8 = _mm_loadl_epi64((const __m128i*)(uRow + x / 2));
            __m128i v8 = _mm_loadl_epi64((const __m128i*)(vRow + x / 2));

            // 3. Expand each U/V to 2 pixels
            __m128i u16 = _mm_unpacklo_epi8(u8, u8); // U0,U0,U1,U1,...
            __m128i v16 = _mm_unpacklo_epi8(v8, v8); // V0,V0,V1,V1,...

            // 4. Combine into 16 bytes
            __m256i u_16 = _mm256_cvtepu8_epi16(u16);
            __m256i v_16 = _mm256_cvtepu8_epi16(v16);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 5. Dynamic offset calculation
            // UV always subtract 128 in all cases
            u_16 = _mm256_sub_epi16(u_16, _mm256_set1_epi16(128));
            v_16 = _mm256_sub_epi16(v_16, _mm256_set1_epi16(128));

            // Y offset depends on range type
            if constexpr (!isFullRange) { // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }
            // Full Range: Y remains unchanged

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

            // Pack BGRA32
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

        // Handle remaining pixels (_i420ToRgba_avx2_imp)

        for (; x < width; x += 2) {
            int y0 = yRow[x + 0];
            int y1 = yRow[x + 1];
            int u = uRow[x / 2];
            int v = vRow[x / 2];

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

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

template <bool isBGR, bool isFullRange>
AVX2_TARGET void _i420ToRgb_avx2_imp(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                                     uint8_t* dst, int dstStride, int width, int height, bool is601) {
    // If height < 0, write dst in reverse order while reading src in normal order
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Select coefficients based on flags
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);

    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

    for (int y = 0; y < height; ++y) {
        const uint8_t* yRow = srcY + y * srcYStride;
        const uint8_t* uRow = srcU + (y / 2) * srcUStride;
        const uint8_t* vRow = srcV + (y / 2) * srcVStride;
        uint8_t* dstRow = dst + y * dstStride;

        int x = 0;
        for (; x + 16 <= width; x += 16) {
            // 1. Load 16 Y values
            __m128i y_vals = _mm_loadu_si128((const __m128i*)(yRow + x));

            // 2. Load 8 U/V values
            __m128i u8 = _mm_loadl_epi64((const __m128i*)(uRow + x / 2));
            __m128i v8 = _mm_loadl_epi64((const __m128i*)(vRow + x / 2));

            // 3. Expand each U/V to 2 pixels
            __m128i u16 = _mm_unpacklo_epi8(u8, u8); // U0,U0,U1,U1,...
            __m128i v16 = _mm_unpacklo_epi8(v8, v8); // V0,V0,V1,V1,...

            // 4. Combine into 16 bytes
            __m256i u_16 = _mm256_cvtepu8_epi16(u16);
            __m256i v_16 = _mm256_cvtepu8_epi16(v16);
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);

            // 5. Dynamic offset calculation
            // UV always subtract 128 in all cases
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            // Y offset depends on range type
            if constexpr (!isFullRange) {
                // Video Range: Y - 16
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }
            // Full Range: Y remains unchanged

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

            // Pack BGR24
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

        // Handle remaining pixels
        for (; x < width; x += 2) {
            int y0 = yRow[x + 0];
            int y1 = yRow[x + 1];
            int u = uRow[x / 2];
            int v = vRow[x / 2];

            int r0, g0, b0, r1, g1, b1;

            // Use pre-selected conversion function
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

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

// AVX2-based acceleration
AVX2_TARGET
void nv12ToBgra32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                       int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        nv12ToRgbaColor_avx2_imp<true, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        nv12ToRgbaColor_avx2_imp<true, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void nv12ToRgba32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                       int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        nv12ToRgbaColor_avx2_imp<false, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        nv12ToRgbaColor_avx2_imp<false, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void nv12ToBgr24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                      int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        _nv12ToRgbColor_avx2_imp<true, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        _nv12ToRgbColor_avx2_imp<true, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void nv12ToRgb24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcUV, int srcUVStride, uint8_t* dst, int dstStride, int width,
                      int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        _nv12ToRgbColor_avx2_imp<false, true>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    } else {
        _nv12ToRgbColor_avx2_imp<false, false>(srcY, srcYStride, srcUV, srcUVStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void i420ToBgra32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        _i420ToRgba_avx2_imp<true, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgba_avx2_imp<true, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void i420ToRgba32_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        _i420ToRgba_avx2_imp<false, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgba_avx2_imp<false, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void i420ToBgr24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        _i420ToRgb_avx2_imp<true, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgb_avx2_imp<true, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void i420ToRgb24_avx2(const uint8_t* srcY, int srcYStride, const uint8_t* srcU, int srcUStride, const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        _i420ToRgb_avx2_imp<false, true>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    } else {
        _i420ToRgb_avx2_imp<false, false>(srcY, srcYStride, srcU, srcUStride, srcV, srcVStride, dst, dstStride, width, height, is601);
    }
}

///////////// YUYV/UYVY to RGB functions /////////////

template <bool isBgrColor, bool hasAlpha, bool isFullRange>
AVX2_TARGET void yuyvToRgb_avx2_imp(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, bool is601) {
    // If height < 0, write dst in reverse order while reading src in normal order
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Select coefficients based on flags
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);
    __m128i a8 = _mm_set1_epi8((char)255);

    constexpr int channels = hasAlpha ? 4 : 3;
    const int vectorWidth = 16; // Process 16 pixels (32 bytes YUYV data)
    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        int x = 0;

        // AVX2 optimized processing, handle 16 pixels each time (32 bytes YUYV data)
        for (; x + vectorWidth <= width; x += vectorWidth) {
            // 1. Load 32 bytes YUYV data (16 pixels = 32 bytes)
            __m256i yuyv_data = _mm256_loadu_si256((const __m256i*)(srcRow + x * 2));

            // 2. Use shuffle to separate YUYV components directly
            // YUYV format: Y0 U0 Y1 V0 Y2 U1 Y3 V1 ...
            // Use correct shuffle mask, considering AVX2 lane limitations

            // Create correct shuffle mask (each lane works independently, index range 0-15)
            __m256i shuffle_y = _mm256_setr_epi8(
                0, 2, 4, 6, 8, 10, 12, 14,      // Lane 0: extract Y0,Y1,Y2,Y3,Y4,Y5,Y6,Y7
                -1, -1, -1, -1, -1, -1, -1, -1, // Lane 0: padding area
                0, 2, 4, 6, 8, 10, 12, 14,      // Lane 1: extract Y8,Y9,Y10,Y11,Y12,Y13,Y14,Y15
                -1, -1, -1, -1, -1, -1, -1, -1  // Lane 1: padding area
            );

            // U component: at positions 1,5,9,13..., each U corresponds to two Y (4:2:2 subsampling)
            __m256i shuffle_u = _mm256_setr_epi8(
                1, 1, 5, 5, 9, 9, 13, 13,       // Lane 0: U0,U0,U1,U1,U2,U2,U3,U3
                -1, -1, -1, -1, -1, -1, -1, -1, // Lane 0: padding area
                1, 1, 5, 5, 9, 9, 13, 13,       // Lane 1: U4,U4,U5,U5,U6,U6,U7,U7
                -1, -1, -1, -1, -1, -1, -1, -1  // Lane 1: padding area
            );

            // V component: at positions 3,7,11,15..., each V corresponds to two Y (4:2:2 subsampling)
            __m256i shuffle_v = _mm256_setr_epi8(
                3, 3, 7, 7, 11, 11, 15, 15,     // Lane 0: V0,V0,V1,V1,V2,V2,V3,V3
                -1, -1, -1, -1, -1, -1, -1, -1, // Lane 0: padding area
                3, 3, 7, 7, 11, 11, 15, 15,     // Lane 1: V4,V4,V5,V5,V6,V6,V7,V7
                -1, -1, -1, -1, -1, -1, -1, -1  // Lane 1: padding area
            );

            // Execute shuffle separation
            __m256i y_shuffled = _mm256_shuffle_epi8(yuyv_data, shuffle_y);
            __m256i u_shuffled = _mm256_shuffle_epi8(yuyv_data, shuffle_u);
            __m256i v_shuffled = _mm256_shuffle_epi8(yuyv_data, shuffle_v);

            // Extract valid data (lower 64 bits contain actual data, upper 64 bits are padded with -1)
            __m128i y_lo = _mm256_castsi256_si128(y_shuffled);      // Y values from Lane 0
            __m128i y_hi = _mm256_extracti128_si256(y_shuffled, 1); // Y values from Lane 1
            __m128i u_lo = _mm256_castsi256_si128(u_shuffled);      // U values from Lane 0
            __m128i u_hi = _mm256_extracti128_si256(u_shuffled, 1); // U values from Lane 1
            __m128i v_lo = _mm256_castsi256_si128(v_shuffled);      // V values from Lane 0
            __m128i v_hi = _mm256_extracti128_si256(v_shuffled, 1); // V values from Lane 1

            // Merge data from two lanes, only take valid first 8 bytes
            __m128i y_final = _mm_unpacklo_epi64(y_lo, y_hi); // Y0-Y7 + Y8-Y15
            __m128i u_final = _mm_unpacklo_epi64(u_lo, u_hi); // U0,U0,U1,U1...U3,U3 + U4,U4,U5,U5...U7,U7
            __m128i v_final = _mm_unpacklo_epi64(v_lo, v_hi); // V0,V0,V1,V1...V3,V3 + V4,V4,V5,V5...V7,V7

            // Convert to 16-bit integers
            __m256i y_16 = _mm256_cvtepu8_epi16(y_final);
            __m256i u_16 = _mm256_cvtepu8_epi16(u_final);
            __m256i v_16 = _mm256_cvtepu8_epi16(v_final);

            // 3. YUV offset processing
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            if constexpr (!isFullRange) {
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }

            // 4. YUV to RGB conversion
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

            // 5. Clamp to 0-255 range
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // 6. Convert to 8-bit and pack output
            __m128i r8 = _mm_packus_epi16(_mm256_castsi256_si128(r), _mm256_extracti128_si256(r, 1));
            __m128i g8 = _mm_packus_epi16(_mm256_castsi256_si128(g), _mm256_extracti128_si256(g, 1));
            __m128i b8 = _mm_packus_epi16(_mm256_castsi256_si128(b), _mm256_extracti128_si256(b, 1));

            // 7. Store according to output format
            if constexpr (hasAlpha) {
                if constexpr (isBgrColor) {
                    // BGRA format
                    __m128i bg0 = _mm_unpacklo_epi8(b8, g8);
                    __m128i ra0 = _mm_unpacklo_epi8(r8, a8);
                    __m128i bgra0 = _mm_unpacklo_epi16(bg0, ra0);
                    __m128i bgra1 = _mm_unpackhi_epi16(bg0, ra0);

                    __m128i bg1 = _mm_unpackhi_epi8(b8, g8);
                    __m128i ra1 = _mm_unpackhi_epi8(r8, a8);
                    __m128i bgra2 = _mm_unpacklo_epi16(bg1, ra1);
                    __m128i bgra3 = _mm_unpackhi_epi16(bg1, ra1);

                    _mm_storeu_si128((__m128i*)(dstRow + x * 4), bgra0);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), bgra1);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), bgra2);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), bgra3);
                } else {
                    // RGBA format
                    __m128i rg0 = _mm_unpacklo_epi8(r8, g8);
                    __m128i ba0 = _mm_unpacklo_epi8(b8, a8);
                    __m128i rgba0 = _mm_unpacklo_epi16(rg0, ba0);
                    __m128i rgba1 = _mm_unpackhi_epi16(rg0, ba0);

                    __m128i rg1 = _mm_unpackhi_epi8(r8, g8);
                    __m128i ba1 = _mm_unpackhi_epi8(b8, a8);
                    __m128i rgba2 = _mm_unpacklo_epi16(rg1, ba1);
                    __m128i rgba3 = _mm_unpackhi_epi16(rg1, ba1);

                    _mm_storeu_si128((__m128i*)(dstRow + x * 4), rgba0);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), rgba1);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), rgba2);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), rgba3);
                }
            } else {
                // RGB24 or BGR24 format - use scalar storage to avoid complex 3-byte packing
                uint8_t r_vals[16], g_vals[16], b_vals[16];
                _mm_storeu_si128((__m128i*)r_vals, r8);
                _mm_storeu_si128((__m128i*)g_vals, g8);
                _mm_storeu_si128((__m128i*)b_vals, b8);

                for (int i = 0; i < 16 && (x + i) < width; ++i) {
                    if constexpr (isBgrColor) {
                        dstRow[(x + i) * 3 + 0] = b_vals[i];
                        dstRow[(x + i) * 3 + 1] = g_vals[i];
                        dstRow[(x + i) * 3 + 2] = r_vals[i];
                    } else {
                        dstRow[(x + i) * 3 + 0] = r_vals[i];
                        dstRow[(x + i) * 3 + 1] = g_vals[i];
                        dstRow[(x + i) * 3 + 2] = b_vals[i];
                    }
                }
            }
        }

        // Handle remaining pixels (scalar implementation)
        for (; x < width; x += 2) {
            if (x + 1 >= width) break; // YUYV needs to be processed in pairs

            // YUYV format: Y0 U0 Y1 V0 (4 bytes for 2 pixels)
            int baseIdx = x * 2;
            int y0 = srcRow[baseIdx + 0]; // Y0
            int u = srcRow[baseIdx + 1];  // U0
            int y1 = srcRow[baseIdx + 2]; // Y1
            int v = srcRow[baseIdx + 3];  // V0

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBgrColor) {
                dstRow[x * channels + 0] = b0;
                dstRow[x * channels + 1] = g0;
                dstRow[x * channels + 2] = r0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * channels + 0] = b1;
                    dstRow[(x + 1) * channels + 1] = g1;
                    dstRow[(x + 1) * channels + 2] = r1;
                }
            } else {
                dstRow[x * channels + 0] = r0;
                dstRow[x * channels + 1] = g0;
                dstRow[x * channels + 2] = b0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * channels + 0] = r1;
                    dstRow[(x + 1) * channels + 1] = g1;
                    dstRow[(x + 1) * channels + 2] = b1;
                }
            }

            if constexpr (hasAlpha) {
                dstRow[x * channels + 3] = 255;
                if (x + 1 < width) {
                    dstRow[(x + 1) * channels + 3] = 255;
                }
            }
        }
    }
}

template <bool isBgrColor, bool hasAlpha, bool isFullRange>
AVX2_TARGET void uyvyToRgb_avx2_imp(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, bool is601) {
    // If height < 0, write dst in reverse order while reading src in normal order
    if (height < 0) {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // Select coefficients based on flags
    int cy, cr, cgu, cgv, cb;
    getYuvToRgbCoefficients(is601, isFullRange, cy, cr, cgu, cgv, cb);

    __m256i c_y = _mm256_set1_epi16(cy);
    __m256i c_r = _mm256_set1_epi16(cr);
    __m256i c_gu = _mm256_set1_epi16(cgu);
    __m256i c_gv = _mm256_set1_epi16(cgv);
    __m256i c_b = _mm256_set1_epi16(cb);

    __m256i c128 = _mm256_set1_epi16(128);
    __m128i a8 = _mm_set1_epi8((char)255);

    constexpr int channels = hasAlpha ? 4 : 3;
    const int vectorWidth = 16; // Process 16 pixels (32 bytes UYVY data)
    YuvToRgbFunc convertFunc = getYuvToRgbFunc(is601, isFullRange);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        int x = 0;

        // AVX2 optimized processing, handle 16 pixels each time (32 bytes UYVY data)
        for (; x + vectorWidth <= width; x += vectorWidth) {
            // 1. Load 32 bytes UYVY data (16 pixels = 32 bytes)
            __m256i uyvy_data = _mm256_loadu_si256((const __m256i*)(srcRow + x * 2));

            // 2. Use shuffle to separate UYVY components directly
            // UYVY format: U0 Y0 V0 Y1 U1 Y2 V1 Y3 ...
            // Use correct shuffle mask, considering AVX2 lane limitations

            // Create correct shuffle mask (each lane works independently, index range 0-15)
            // For UYVY, Y is at position 1,3,5,7...
            __m256i shuffle_y = _mm256_setr_epi8(
                1, 3, 5, 7, 9, 11, 13, 15,      // Lane 0: Extract Y0,Y1,Y2,Y3,Y4,Y5,Y6,Y7
                -1, -1, -1, -1, -1, -1, -1, -1, // Lane 0: Padding area
                1, 3, 5, 7, 9, 11, 13, 15,      // Lane 1: Extract Y8,Y9,Y10,Y11,Y12,Y13,Y14,Y15
                -1, -1, -1, -1, -1, -1, -1, -1  // Lane 1: Padding area
            );

            // U component: at position 0,4,8,12..., each U corresponds to two Y (4:2:2 subsampling)
            __m256i shuffle_u = _mm256_setr_epi8(
                0, 0, 4, 4, 8, 8, 12, 12,       // Lane 0: U0,U0,U1,U1,U2,U2,U3,U3
                -1, -1, -1, -1, -1, -1, -1, -1, // Lane 0: Padding area
                0, 0, 4, 4, 8, 8, 12, 12,       // Lane 1: U4,U4,U5,U5,U6,U6,U7,U7
                -1, -1, -1, -1, -1, -1, -1, -1  // Lane 1: Padding area
            );

            // V component: at position 2,6,10,14..., each V corresponds to two Y (4:2:2 subsampling)
            __m256i shuffle_v = _mm256_setr_epi8(
                2, 2, 6, 6, 10, 10, 14, 14,     // Lane 0: V0,V0,V1,V1,V2,V2,V3,V3
                -1, -1, -1, -1, -1, -1, -1, -1, // Lane 0: Padding area
                2, 2, 6, 6, 10, 10, 14, 14,     // Lane 1: V4,V4,V5,V5,V6,V6,V7,V7
                -1, -1, -1, -1, -1, -1, -1, -1  // Lane 1: Padding area
            );

            // Execute shuffle separation
            __m256i y_shuffled = _mm256_shuffle_epi8(uyvy_data, shuffle_y);
            __m256i u_shuffled = _mm256_shuffle_epi8(uyvy_data, shuffle_u);
            __m256i v_shuffled = _mm256_shuffle_epi8(uyvy_data, shuffle_v);

            // Extract valid data (lower 64 bits contain actual data, upper 64 bits are padded with -1)
            __m128i y_lo = _mm256_castsi256_si128(y_shuffled);      // Y values from Lane 0
            __m128i y_hi = _mm256_extracti128_si256(y_shuffled, 1); // Y values from Lane 1
            __m128i u_lo = _mm256_castsi256_si128(u_shuffled);      // U values from Lane 0
            __m128i u_hi = _mm256_extracti128_si256(u_shuffled, 1); // U values from Lane 1
            __m128i v_lo = _mm256_castsi256_si128(v_shuffled);      // V values from Lane 0
            __m128i v_hi = _mm256_extracti128_si256(v_shuffled, 1); // V values from Lane 1

            // Merge data from two lanes, only take valid first 8 bytes
            __m128i y_final = _mm_unpacklo_epi64(y_lo, y_hi); // Y0-Y7 + Y8-Y15
            __m128i u_final = _mm_unpacklo_epi64(u_lo, u_hi); // U0,U0,U1,U1...U3,U3 + U4,U4,U5,U5...U7,U7
            __m128i v_final = _mm_unpacklo_epi64(v_lo, v_hi); // V0,V0,V1,V1...V3,V3 + V4,V4,V5,V5...V7,V7

            // Convert to 16-bit integers
            __m256i y_16 = _mm256_cvtepu8_epi16(y_final);
            __m256i u_16 = _mm256_cvtepu8_epi16(u_final);
            __m256i v_16 = _mm256_cvtepu8_epi16(v_final);

            // 3. YUV offset processing
            u_16 = _mm256_sub_epi16(u_16, c128);
            v_16 = _mm256_sub_epi16(v_16, c128);

            if constexpr (!isFullRange) {
                y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            }

            // 4. YUV to RGB conversion
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

            // 5. Clamp to 0-255 range
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // 6. Convert to 8-bit and pack output
            __m128i r8 = _mm_packus_epi16(_mm256_castsi256_si128(r), _mm256_extracti128_si256(r, 1));
            __m128i g8 = _mm_packus_epi16(_mm256_castsi256_si128(g), _mm256_extracti128_si256(g, 1));
            __m128i b8 = _mm_packus_epi16(_mm256_castsi256_si128(b), _mm256_extracti128_si256(b, 1));

            // 7. Store according to output format
            if constexpr (hasAlpha) {
                if constexpr (isBgrColor) {
                    // BGRA format
                    __m128i bg0 = _mm_unpacklo_epi8(b8, g8);
                    __m128i ra0 = _mm_unpacklo_epi8(r8, a8);
                    __m128i bgra0 = _mm_unpacklo_epi16(bg0, ra0);
                    __m128i bgra1 = _mm_unpackhi_epi16(bg0, ra0);

                    __m128i bg1 = _mm_unpackhi_epi8(b8, g8);
                    __m128i ra1 = _mm_unpackhi_epi8(r8, a8);
                    __m128i bgra2 = _mm_unpacklo_epi16(bg1, ra1);
                    __m128i bgra3 = _mm_unpackhi_epi16(bg1, ra1);

                    _mm_storeu_si128((__m128i*)(dstRow + x * 4), bgra0);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), bgra1);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), bgra2);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), bgra3);
                } else {
                    // RGBA format
                    __m128i rg0 = _mm_unpacklo_epi8(r8, g8);
                    __m128i ba0 = _mm_unpacklo_epi8(b8, a8);
                    __m128i rgba0 = _mm_unpacklo_epi16(rg0, ba0);
                    __m128i rgba1 = _mm_unpackhi_epi16(rg0, ba0);

                    __m128i rg1 = _mm_unpackhi_epi8(r8, g8);
                    __m128i ba1 = _mm_unpackhi_epi8(b8, a8);
                    __m128i rgba2 = _mm_unpacklo_epi16(rg1, ba1);
                    __m128i rgba3 = _mm_unpackhi_epi16(rg1, ba1);

                    _mm_storeu_si128((__m128i*)(dstRow + x * 4), rgba0);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 16), rgba1);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 32), rgba2);
                    _mm_storeu_si128((__m128i*)(dstRow + x * 4 + 48), rgba3);
                }
            } else {
                // RGB24 or BGR24 format - use scalar storage to avoid complex 3-byte packing
                uint8_t r_vals[16], g_vals[16], b_vals[16];
                _mm_storeu_si128((__m128i*)r_vals, r8);
                _mm_storeu_si128((__m128i*)g_vals, g8);
                _mm_storeu_si128((__m128i*)b_vals, b8);

                for (int i = 0; i < 16 && (x + i) < width; ++i) {
                    if constexpr (isBgrColor) {
                        dstRow[(x + i) * 3 + 0] = b_vals[i];
                        dstRow[(x + i) * 3 + 1] = g_vals[i];
                        dstRow[(x + i) * 3 + 2] = r_vals[i];
                    } else {
                        dstRow[(x + i) * 3 + 0] = r_vals[i];
                        dstRow[(x + i) * 3 + 1] = g_vals[i];
                        dstRow[(x + i) * 3 + 2] = b_vals[i];
                    }
                }
            }
        }

        // Handle remaining pixels (scalar implementation)
        for (; x < width; x += 2) {
            if (x + 1 >= width) break; // UYVY requires paired processing

            // UYVY format: U0 Y0 V0 Y1 (4 bytes for 2 pixels)
            int baseIdx = x * 2;
            int u = srcRow[baseIdx + 0];  // U0
            int y0 = srcRow[baseIdx + 1]; // Y0
            int v = srcRow[baseIdx + 2];  // V0
            int y1 = srcRow[baseIdx + 3]; // Y1

            int r0, g0, b0, r1, g1, b1;
            convertFunc(y0, u, v, r0, g0, b0);
            convertFunc(y1, u, v, r1, g1, b1);

            if constexpr (isBgrColor) {
                dstRow[x * channels + 0] = b0;
                dstRow[x * channels + 1] = g0;
                dstRow[x * channels + 2] = r0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * channels + 0] = b1;
                    dstRow[(x + 1) * channels + 1] = g1;
                    dstRow[(x + 1) * channels + 2] = r1;
                }
            } else {
                dstRow[x * channels + 0] = r0;
                dstRow[x * channels + 1] = g0;
                dstRow[x * channels + 2] = b0;

                if (x + 1 < width) {
                    dstRow[(x + 1) * channels + 0] = r1;
                    dstRow[(x + 1) * channels + 1] = g1;
                    dstRow[(x + 1) * channels + 2] = b1;
                }
            }

            if constexpr (hasAlpha) {
                dstRow[x * channels + 3] = 255;
                if (x + 1 < width) {
                    dstRow[(x + 1) * channels + 3] = 255;
                }
            }
        }
    }
}

// YUYV conversion functions
AVX2_TARGET
void yuyvToBgr24_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        yuyvToRgb_avx2_imp<true, false, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        yuyvToRgb_avx2_imp<true, false, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void yuyvToRgb24_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        yuyvToRgb_avx2_imp<false, false, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        yuyvToRgb_avx2_imp<false, false, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void yuyvToBgra32_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        yuyvToRgb_avx2_imp<true, true, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        yuyvToRgb_avx2_imp<true, true, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void yuyvToRgba32_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        yuyvToRgb_avx2_imp<false, true, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        yuyvToRgb_avx2_imp<false, true, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

// UYVY conversion functions
AVX2_TARGET
void uyvyToBgr24_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        uyvyToRgb_avx2_imp<true, false, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        uyvyToRgb_avx2_imp<true, false, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void uyvyToRgb24_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        uyvyToRgb_avx2_imp<false, false, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        uyvyToRgb_avx2_imp<false, false, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void uyvyToBgra32_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        uyvyToRgb_avx2_imp<true, true, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        uyvyToRgb_avx2_imp<true, true, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

AVX2_TARGET
void uyvyToRgba32_avx2(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int width, int height, ConvertFlag flag) {
    const bool is601 = (flag & ConvertFlag::BT601) != 0;
    const bool isFullRange = (flag & ConvertFlag::FullRange) != 0;

    if (isFullRange) {
        uyvyToRgb_avx2_imp<false, true, true>(src, srcStride, dst, dstStride, width, height, is601);
    } else {
        uyvyToRgb_avx2_imp<false, true, false>(src, srcStride, dst, dstStride, width, height, is601);
    }
}

#endif // ENABLE_AVX2_IMP
} // namespace ccap
