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
{ // 实现一个 通用的 colorShuffle, 通过 AVX2 加速

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
    { // 输出是 4 通道, 一次处理 8 个像素

        if (inputChannels == 4)
        { /// 可以按 input 的顺序直接处理
            for (int i = 0; i < 32; ++i)
            {
                shuffleData[i] = inputShuffle[i % 4] + (i / 4) * 4;
            }
        }
        else // input 是 3 通道, shuffle 要跳过每个 input 的 alpha
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
    else // 输出是 3 通道, 一次处理 10 个像素
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

            // 这里确保写入不会越过当前行的边界
            if (outputChannels == 3 && xOutput + outputPatchSize == width)
            {
                // 最后10个像素，但不想写入额外2字节，手动复制前30字节
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
        // 处理剩余像素
        for (; xOutput < (uint32_t)width; ++xOutput, ++xInput)
        {
            for (int c = 0; c < outputChannels; ++c)
            {
                if (inputChannels == 3 && c == 3)
                    dstRow[xOutput * outputChannels + c] = 0xFF; // alpha填充
                else
                    dstRow[xOutput * outputChannels + c] = srcRow[xInput * inputChannels + inputShuffle[c]];
            }
        }
    }
}

// NV12 转 BGRA32，AVX2加速
void nv12ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 转 RGBA32，AVX2加速
void nv12ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// NV12 转 BGR24，AVX2加速
void nv12ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// NV12 转 RGB24，AVX2加速
void nv12ToRgb24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcUV, int srcUVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// I420 转 BGRA32，AVX2加速
void i420ToBgra32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// I420 转 RGBA32，AVX2加速
void i420ToRgba32_avx2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcU, int srcUStride,
                       const uint8_t* srcV, int srcVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height);

// I420 转 BGR24，AVX2加速
void i420ToBgr24_avx2(const uint8_t* srcY, int srcYStride,
                      const uint8_t* srcU, int srcUStride,
                      const uint8_t* srcV, int srcVStride,
                      uint8_t* dst, int dstStride,
                      int width, int height);

// I420 转 RGB24，AVX2加速
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