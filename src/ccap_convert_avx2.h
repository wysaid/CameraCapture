/**
 * @file ccap_convert_avx2.h.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_CONVERT_AVX2_H
#define CCAP_CONVERT_AVX2_H

#include <cstdint>

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
bool hasAVX2();

#if ENABLE_AVX2_IMP

template <int inputChannels, int outputChannels>
void colorShuffle_avx2(const uint8_t* src, int srcStride,
                       uint8_t* dst, int dstStride,
                       int width, int height, const uint8_t inputShuffle[])
{ // Implement a general colorShuffle, accelerated by AVX2

    static_assert((inputChannels == 3 || inputChannels == 4) &&
                      (outputChannels == 3 || outputChannels == 4),
                  "inputChannels and outputChannels must be 3 or 4");

    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    alignas(32) uint8_t shuffleData[32];
    constexpr uint32_t inputPatchSize = inputChannels == 4 ? 8 : 10;
    constexpr uint32_t outputPatchSize = outputChannels == 4 ? 8 : 10;

    if constexpr (outputChannels == 4)
    { // Output is 4 channels, process 8 pixels at a time

        if (inputChannels == 4)
        { /// Can process directly in input order
            for (int i = 0; i < 32; ++i)
            {
                shuffleData[i] = inputShuffle[i % 4] + (i / 4) * 4;
            }
        }
        else // Input is 3 channels, shuffle skips alpha for each input
        {
            for (int i = 0; i < 8; ++i)
            {
                int idx = i * 4;
                int inputIdx = i * 3;
                shuffleData[idx] = inputShuffle[0] + inputIdx;
                shuffleData[idx + 1] = inputShuffle[1] + inputIdx;
                shuffleData[idx + 2] = inputShuffle[2] + inputIdx;
                shuffleData[idx + 3] = 0xFF; // no alpha
            }
        }
    }
    else // Output is 3 channels, process 10 pixels at a time
    {
        for (int i = 0; i < 30; ++i)
        {
            shuffleData[i] = inputShuffle[i % 3] + (i / 3) * 3;
        }
        shuffleData[30] = 0xFF; // extra bit
        shuffleData[31] = 0xFF; // extra bit
    }

    __m256i shuffle = _mm256_load_si256((const __m256i*)shuffleData);

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + y * dstStride;
        uint32_t xOutput = 0, xInput = 0;
        while (xOutput + outputPatchSize <= (uint32_t)width)
        {
            __m256i pixels = _mm256_loadu_si256((const __m256i*)(srcRow + xInput * inputChannels));
            __m256i result = _mm256_shuffle_epi8(pixels, shuffle);

            // Ensure no out-of-bounds write for the current row
            if (outputChannels == 3 && xOutput + outputPatchSize == width)
            {
                // Last 10 pixels, avoid writing extra 2 bytes, manually copy first 30 bytes
                alignas(32) uint8_t buffer[32];
                _mm256_store_si256((__m256i*)buffer, result);
                memcpy(dstRow + xOutput * outputChannels, buffer, 30);
            }
            else
            {
                _mm256_storeu_si256((__m256i*)(dstRow + xOutput * outputChannels), result);
            }

            xOutput += outputPatchSize;
            xInput += inputPatchSize;
        }
        // Handle remaining pixels
        for (; xOutput < (uint32_t)width; ++xOutput, ++xInput)
        {
            for (int c = 0; c < outputChannels; ++c)
            {
                if (inputChannels == 3 && c == 3)
                    dstRow[xOutput * outputChannels + c] = 0xFF; // fill alpha
                else
                    dstRow[xOutput * outputChannels + c] = srcRow[xInput * inputChannels + inputShuffle[c]];
            }
        }
    }
}

// NV12 to BGRA32, AVX2 accelerated
void nv12ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 to RGBA32, AVX2 accelerated
void nv12ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 to BGR24, AVX2 accelerated
void nv12ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// NV12 to RGB24, AVX2 accelerated
void nv12ToRgb24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// I420 to BGRA32, AVX2 accelerated
void i420ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// I420 to RGBA32, AVX2 accelerated
void i420ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// I420 to BGR24, AVX2 accelerated
void i420ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// I420 to RGB24, AVX2 accelerated
void i420ToRgb24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);
#else

#define nv12ToBgr24_avx2(...) assert(0 && "AVX2 not supported")
#define nv12ToRgb24_avx2(...) assert(0 && "AVX2 not supported")
#define nv12ToBgra32_avx2(...) assert(0 && "AVX2 not supported")
#define nv12ToRgba32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToBgra32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToRgba32_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToBgr24_avx2(...) assert(0 && "AVX2 not supported")
#define i420ToRgb24_avx2(...) assert(0 && "AVX2 not supported")

#endif

} // namespace ccap

#endif // CCAP_CONVERT_AVX2_H