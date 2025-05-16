/**
 * @file CameraCaptureDirectShow.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Implementation for Provider class using DSHOW.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureDirectShow.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <guiddef.h>
#include <immintrin.h> // AVX2
#include <initguid.h>
#include <vector>

#if ENABLE_LIBYUV
#include <libyuv.h>
#endif

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

static bool hasAVX2()
{
    static bool s_hasAVX2 = hasAVX2_();
    return s_hasAVX2;
}

#if _CCAP_LOG_ENABLED_
#include <deque>
#endif

// The following libraries need to be linked
#pragma comment(lib, "strmiids.lib")

/// @see <https://doxygen.reactos.org/d9/dce/structtagVIDEOINFOHEADER2.html>
typedef struct tagVIDEOINFOHEADER2
{
    RECT rcSource;
    RECT rcTarget;
    DWORD dwBitRate;
    DWORD dwBitErrorRate;
    REFERENCE_TIME AvgTimePerFrame;
    DWORD dwInterlaceFlags;
    DWORD dwCopyProtectFlags;
    DWORD dwPictAspectRatioX;
    DWORD dwPictAspectRatioY;
    union
    {
        DWORD dwControlFlags;
        DWORD dwReserved1;

    } DUMMYUNIONNAME;

    DWORD dwReserved2;
    BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER2;

#define AMCONTROL_COLORINFO_PRESENT 0x00000080

#ifndef DXVA_ExtendedFormat_DEFINED
#define DXVA_ExtendedFormat_DEFINED

/// @see <https://learn.microsoft.com/zh-cn/windows-hardware/drivers/ddi/dxva/ns-dxva-_dxva_extendedformat>
typedef struct _DXVA_ExtendedFormat
{
    union
    {
        struct
        {
            UINT SampleFormat : 8;
            UINT VideoChromaSubsampling : 4;
            UINT NominalRange : 3;
            UINT VideoTransferMatrix : 3;
            UINT VideoLighting : 4;
            UINT VideoPrimaries : 5;
            UINT VideoTransferFunction : 5;
        };
        UINT Value;
    };
} DXVA_ExtendedFormat;

#define DXVA_NominalRange_Unknown 0
#define DXVA_NominalRange_Normal 1 // 16-235
#define DXVA_NominalRange_Wide 2   // 0-255
#define DXVA_NominalRange_0_255 2
#define DXVA_NominalRange_16_235 1
#endif

#ifndef MEDIASUBTYPE_I420
DEFINE_GUID(MEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
#endif

using namespace ccap;

namespace
{
constexpr FrameOrientation kDefaultFrameOrientation = FrameOrientation::BottomToTop;

// Release the format block for a media type.
void freeMediaType(AM_MEDIA_TYPE& mt)
{
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        // pUnk should not be used.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}

// Delete a media type structure that was allocated on the heap.
void deleteMediaType(AM_MEDIA_TYPE* pmt)
{
    if (pmt != NULL)
    {
        freeMediaType(*pmt);
        CoTaskMemFree(pmt);
    }
}

struct PixelFormtInfo
{
    GUID subtype;
    const char* name;
    PixelFormat pixelFormat;
};

constexpr const char* unavailableMsg = "ccap unavailable by now";

PixelFormtInfo s_pixelInfoList[] = {
    { MEDIASUBTYPE_MJPG, "MJPG (need decode)", PixelFormat::Unknown },
    { MEDIASUBTYPE_RGB24, "BGR24", PixelFormat::BGR24 },   // RGB24 here is actually BGR order
    { MEDIASUBTYPE_RGB32, "BGRA32", PixelFormat::BGRA32 }, // Same as above
    { MEDIASUBTYPE_NV12, "NV12", PixelFormat::NV12 },
    { MEDIASUBTYPE_I420, "I420", PixelFormat::I420 },
    { MEDIASUBTYPE_IYUV, "IYUV (I420)", PixelFormat::I420 },
    { MEDIASUBTYPE_YUY2, "YUY2", PixelFormat::Unknown },
    { MEDIASUBTYPE_YV12, "YV12", PixelFormat::Unknown },
    { MEDIASUBTYPE_UYVY, "UYVY", PixelFormat::Unknown },
    { MEDIASUBTYPE_RGB565, "RGB565", PixelFormat::Unknown },
    { MEDIASUBTYPE_RGB555, "RGB555", PixelFormat::Unknown },
    { MEDIASUBTYPE_YUYV, "YUYV", PixelFormat::Unknown },
    { MEDIASUBTYPE_YVYU, "YVYU", PixelFormat::Unknown },
    { MEDIASUBTYPE_NV11, "NV11", PixelFormat::Unknown },
    { MEDIASUBTYPE_NV24, "NV24", PixelFormat::Unknown },
    { MEDIASUBTYPE_YVU9, "YVU9", PixelFormat::Unknown },
    { MEDIASUBTYPE_Y411, "Y411", PixelFormat::Unknown },
    { MEDIASUBTYPE_Y41P, "Y41P", PixelFormat::Unknown },
    { MEDIASUBTYPE_CLJR, "CLJR", PixelFormat::Unknown },
    { MEDIASUBTYPE_IF09, "IF09", PixelFormat::Unknown },
    { MEDIASUBTYPE_CPLA, "CPLA", PixelFormat::Unknown },
    { MEDIASUBTYPE_AYUV, "AYUV", PixelFormat::Unknown },
    { MEDIASUBTYPE_AI44, "AI44", PixelFormat::Unknown },
    { MEDIASUBTYPE_IA44, "IA44", PixelFormat::Unknown },
    { MEDIASUBTYPE_IMC1, "IMC1", PixelFormat::Unknown },
    { MEDIASUBTYPE_IMC2, "IMC2", PixelFormat::Unknown },
    { MEDIASUBTYPE_IMC3, "IMC3", PixelFormat::Unknown },
    { MEDIASUBTYPE_IMC4, "IMC4", PixelFormat::Unknown },
    { MEDIASUBTYPE_420O, "420O", PixelFormat::Unknown },
};

PixelFormtInfo findPixelFormatInfo(const GUID& subtype)
{
    for (auto& i : s_pixelInfoList)
    {
        if (subtype == i.subtype)
        {
            return i;
        }
    }
    return { MEDIASUBTYPE_None, "Unknown", PixelFormat::Unknown };
}

struct MediaInfo
{
    DeviceInfo::Resolution resolution;
    PixelFormtInfo pixelFormatInfo;
    std::shared_ptr<AM_MEDIA_TYPE*> mediaType;
};

void printMediaType(AM_MEDIA_TYPE* pmt, const char* prefix)
{
    const GUID& subtype = pmt->subtype;
    PixelFormtInfo info = findPixelFormatInfo(subtype);

    const char* rangeStr = "";
    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;

    auto width = vih->bmiHeader.biWidth;
    auto height = vih->bmiHeader.biHeight;
    double fps = vih->AvgTimePerFrame != 0 ? 10000000.0 / vih->AvgTimePerFrame : 0;

    if (info.pixelFormat & kPixelFormatYUVColorBit)
    {
        if (pmt->formattype == FORMAT_VideoInfo2 && pmt->cbFormat >= sizeof(VIDEOINFOHEADER2))
        {
            VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
            // Check AMCONTROL_COLORINFO_PRESENT
            if (vih2->dwControlFlags & AMCONTROL_COLORINFO_PRESENT)
            { // DXVA_ExtendedFormat follows immediately after VIDEOINFOHEADER2
                BYTE* extFmtPtr = (BYTE*)vih2 + sizeof(VIDEOINFOHEADER2);
                if (pmt->cbFormat >= sizeof(VIDEOINFOHEADER2) + sizeof(DXVA_ExtendedFormat))
                {
                    DXVA_ExtendedFormat* extFmt = (DXVA_ExtendedFormat*)extFmtPtr;
                    if (extFmt->NominalRange == DXVA_NominalRange_0_255)
                    {
                        rangeStr = " (FullRange)";
                    }
                    else if (extFmt->NominalRange == DXVA_NominalRange_16_235)
                    {
                        rangeStr = " (VideoRange)";
                    }
                    else
                    {
                        rangeStr = " (UnknownRange)";
                    }
                }
            }
        }
    }

    printf("%s%ld x %ld  bitcount=%ld  format=%s%s, fps=%g\n", prefix, width, height, vih->bmiHeader.biBitCount, info.name, rangeStr, fps);
    fflush(stdout);
}

bool setupCom()
{
    static bool s_didSetup = false;
    if (!s_didSetup)
    {
        // Initialize COM without performing uninitialization, as other parts may also use COM
        // Use COINIT_APARTMENTTHREADED mode here, as we only use COM in a single thread
        auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        s_didSetup = !(FAILED(hr) && hr != RPC_E_CHANGED_MODE);

        if (!s_didSetup)
        {
            CCAP_LOG_E("ccap: CoInitializeEx failed, hr=0x%08X\n", hr);
        }
    }
    return s_didSetup;
}

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

#if ENABLE_LIBYUV

bool inplaceConvertFrameYUV2YUV(Frame* frame, PixelFormat toFormat, bool verticalFlip, std::vector<uint8_t>& memCache)
{
    /// (NV12/I420) <-> (NV12/I420)
    assert((frame->pixelFormat & kPixelFormatYUVColorBit) != 0 && (toFormat & kPixelFormatYUVColorBit) != 0);
    bool isInputNV12 = pixelFormatInclude(frame->pixelFormat, PixelFormat::NV12);
    bool isOutputNV12 = pixelFormatInclude(toFormat, PixelFormat::NV12);
    bool isInputI420 = pixelFormatInclude(frame->pixelFormat, PixelFormat::I420);
    bool isOutputI420 = pixelFormatInclude(toFormat, PixelFormat::I420);

    assert(!(isInputNV12 && isOutputNV12)); // 相同类型不应该进来
    assert(!(isInputI420 && isOutputI420)); // 相同类型不应该进来
    uint8_t* inputData0 = frame->data[0];
    uint8_t* inputData1 = frame->data[1];
    uint8_t* inputData2 = frame->data[2];
    int stride0 = frame->stride[0];
    int stride1 = frame->stride[1];
    int stride2 = frame->stride[2];
    int width = frame->width;
    int height = frame->height;

    // NV12/I420 都是 YUV420P 格式
    frame->allocator->resize(stride0 * width + (stride1 + stride2) * height / 2);
    frame->data[0] = frame->allocator->data();

    uint8_t* outputData0 = frame->data[0];
    frame->data[1] = outputData0 + stride0 * height;

    if (isInputNV12)
    { /// NV12 -> I420
        frame->stride[1] = stride1 / 2;
        frame->stride[2] = frame->stride[1];
        frame->data[2] = isOutputI420 ? frame->data[1] + stride1 * height / 2 : nullptr;
        frame->pixelFormat = toFormat;

        return libyuv::NV12ToI420(inputData0, stride0,
                                  inputData1, stride1,
                                  outputData0, stride0,
                                  frame->data[1], frame->stride[1],
                                  frame->data[2], frame->stride[2],
                                  width, height) == 0;
    }
    else if (isInputI420)
    { // I420 -> NV12
        frame->stride[1] = stride1 + stride2;
        frame->stride[2] = 0;
        frame->data[2] = nullptr;

        return libyuv::I420ToNV12(inputData0, stride0,
                                  inputData1, stride1,
                                  inputData2, stride2,
                                  frame->data[0], stride0,
                                  frame->data[1], frame->stride[1],
                                  width, height) == 0;
    }

    return false;
}

#else

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

inline void yuv2rgb601(int y, int u, int v, int& r, int& g, int& b)
{
    r = (298 * y + 409 * v + 128) >> 8;
    g = (298 * y - 100 * u - 208 * v + 128) >> 8;
    b = (298 * y + 516 * u + 128) >> 8;
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

// 基于 AVX2 加速
void nv12ToBGRA32_AVX2(const uint8_t* srcY, int srcYStride,
                       const uint8_t* srcUV, int srcUVStride,
                       uint8_t* dst, int dstStride,
                       int width, int height)
{
    // 支持 height < 0 上下翻转
    if (height < 0)
    {
        height = -height;
        dst = dst + (height - 1) * dstStride;
        dstStride = -dstStride;
    }

    // 仅支持 width 为 16 的倍数，否则尾部需补齐
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

            // 2. 加载 16 个 UV（8 对），UV 交错排列
            __m128i uv_vals = _mm_loadu_si128((const __m128i*)(uvRow + x));

            // 拆分 U/V
            __m128i u_vals = _mm_and_si128(uv_vals, _mm_set1_epi16(0x00FF));
            __m128i v_vals = _mm_srli_epi16(uv_vals, 8);

            // 展开 U/V 到 16 个像素（每 2 个像素用同一组 U/V）
            __m128i u_vals_16 = _mm_unpacklo_epi8(u_vals, u_vals);
            __m128i v_vals_16 = _mm_unpacklo_epi8(v_vals, v_vals);
            u_vals_16 = _mm_packus_epi16(u_vals_16, _mm_unpackhi_epi8(u_vals, u_vals));
            v_vals_16 = _mm_packus_epi16(v_vals_16, _mm_unpackhi_epi8(v_vals, v_vals));

            // 3. 转换为 int16
            __m256i y_16 = _mm256_cvtepu8_epi16(y_vals);
            __m256i u_16 = _mm256_cvtepu8_epi16(u_vals_16);
            __m256i v_16 = _mm256_cvtepu8_epi16(v_vals_16);

            // 4. 偏移
            y_16 = _mm256_sub_epi16(y_16, _mm256_set1_epi16(16));
            u_16 = _mm256_sub_epi16(u_16, _mm256_set1_epi16(128));
            v_16 = _mm256_sub_epi16(v_16, _mm256_set1_epi16(128));

            // 5. YUV -> RGB (BT.601)
            // R = (298 * Y + 409 * V + 128) >> 8
            // G = (298 * Y - 100 * U - 208 * V + 128) >> 8
            // B = (298 * Y + 516 * U + 128) >> 8

            __m256i c298 = _mm256_set1_epi16(298);
            __m256i c409 = _mm256_set1_epi16(409);
            __m256i c100 = _mm256_set1_epi16(100);
            __m256i c208 = _mm256_set1_epi16(208);
            __m256i c516 = _mm256_set1_epi16(516);

            __m256i y298 = _mm256_mullo_epi16(y_16, c298);

            __m256i r = _mm256_add_epi16(y298, _mm256_mullo_epi16(v_16, c409));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(128));
            r = _mm256_srai_epi16(r, 8);

            __m256i g = _mm256_sub_epi16(y298, _mm256_mullo_epi16(u_16, c100));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c208));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(128));
            g = _mm256_srai_epi16(g, 8);

            __m256i b = _mm256_add_epi16(y298, _mm256_mullo_epi16(u_16, c516));
            b = _mm256_add_epi16(b, _mm256_set1_epi16(128));
            b = _mm256_srai_epi16(b, 8);

            // clamp 0~255
            __m256i zero = _mm256_setzero_si256();
            __m256i maxv = _mm256_set1_epi16(255);
            r = _mm256_max_epi16(zero, _mm256_min_epi16(r, maxv));
            g = _mm256_max_epi16(zero, _mm256_min_epi16(g, maxv));
            b = _mm256_max_epi16(zero, _mm256_min_epi16(b, maxv));

            // 打包 BGRA
            __m256i a = _mm256_set1_epi16(255);

            // 交错打包
            __m256i bg = _mm256_or_si256(b, _mm256_slli_epi16(g, 8));
            __m256i ra = _mm256_or_si256(r, _mm256_slli_epi16(a, 8));

            __m256i bgra_lo = _mm256_unpacklo_epi16(bg, ra);
            __m256i bgra_hi = _mm256_unpackhi_epi16(bg, ra);

            // 存储
            _mm256_storeu_si256((__m256i*)(dstRow + x * 4), bgra_lo);
            _mm256_storeu_si256((__m256i*)(dstRow + x * 4 + 32), bgra_hi);
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

            // 8. YUV -> RGB (BT.601)
            __m256i c298 = _mm256_set1_epi16(298);
            __m256i c409 = _mm256_set1_epi16(409);
            __m256i c100 = _mm256_set1_epi16(100);
            __m256i c208 = _mm256_set1_epi16(208);
            __m256i c516 = _mm256_set1_epi16(516);

            __m256i y298 = _mm256_mullo_epi16(y_16, c298);

            __m256i r = _mm256_add_epi16(y298, _mm256_mullo_epi16(v_16, c409));
            r = _mm256_add_epi16(r, _mm256_set1_epi16(128));
            r = _mm256_srai_epi16(r, 8);

            __m256i g = _mm256_sub_epi16(y298, _mm256_mullo_epi16(u_16, c100));
            g = _mm256_sub_epi16(g, _mm256_mullo_epi16(v_16, c208));
            g = _mm256_add_epi16(g, _mm256_set1_epi16(128));
            g = _mm256_srai_epi16(g, 8);

            __m256i b = _mm256_add_epi16(y298, _mm256_mullo_epi16(u_16, c516));
            b = _mm256_add_epi16(b, _mm256_set1_epi16(128));
            b = _mm256_srai_epi16(b, 8);

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
            int u = uvRow[x] - 128;
            int v = uvRow[x + 1] - 128;

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

            dstRow[(x + 0) * 3 + 0] = b0;
            dstRow[(x + 0) * 3 + 1] = g0;
            dstRow[(x + 0) * 3 + 2] = r0;

            dstRow[(x + 1) * 3 + 0] = b1;
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

#endif

bool inplaceConvertFrameYUV2BGR(Frame* frame, PixelFormat toFormat, std::vector<uint8_t>& memCache)
{ /// (NV12/I420) -> (BGR24/BGRA32)

    /// TODO: 这里修正一下 toFormat, 只支持 YUV -> (BGR24/BGRA24). 简化一下 SDK 的设计. 后续再完善.

    if (toFormat & kPixelFormatRGBBit)
    { /// 只转换成 BGR, 把 RGB 改成 BGR.
        toFormat = (PixelFormat)(((uint32_t)toFormat & ~(uint32_t)kPixelFormatRGBBit) | (uint32_t)kPixelFormatBGRBit);
    }

    auto inputFormat = frame->pixelFormat;
    assert((inputFormat & kPixelFormatYUVColorBit) != 0 && (toFormat & kPixelFormatYUVColorBit) == 0);
    bool isInputNV12 = pixelFormatInclude(inputFormat, PixelFormat::NV12);
    bool isInputI420 = pixelFormatInclude(inputFormat, PixelFormat::I420);
    bool outputHasAlpha = toFormat & kPixelFormatAlphaColorBit;

    uint8_t* inputData0 = frame->data[0];
    uint8_t* inputData1 = frame->data[1];
    uint8_t* inputData2 = frame->data[2];
    int stride0 = frame->stride[0];
    int stride1 = frame->stride[1];
    int stride2 = frame->stride[2];
    int width = frame->width;
    int height = frame->height;

    auto newLineSize = outputHasAlpha ? frame->width * 4 : (frame->width * 3 + 31) & ~31;

    frame->allocator->resize(newLineSize * width);
    frame->data[0] = frame->allocator->data();
    frame->stride[0] = newLineSize;
    frame->data[1] = nullptr;
    frame->data[2] = nullptr;
    frame->stride[1] = 0;
    frame->stride[2] = 0;
    frame->pixelFormat = toFormat;

    if (isInputNV12)
    { // NV12 -> BGR24, libyuv 里面的 RGB24 实际上是 BGR24

        if (outputHasAlpha)
        {
#if ENABLE_LIBYUV
            return libyuv::NV12ToARGB(inputData0, stride0,
                                      inputData1, stride1,
                                      frame->data[0], newLineSize,
                                      width, height) == 0;
#else
            nv12ToBGRA32(inputData0, stride0,
                         inputData1, stride1,
                         frame->data[0], newLineSize,
                         width, height);
            return true;
#endif
        }
        else
        {
#if ENABLE_LIBYUV
            return libyuv::NV12ToRGB24(inputData0, stride0,
                                       inputData1, stride1,
                                       frame->data[0], newLineSize,
                                       width, height) == 0;
#else
            nv12ToBGR24(inputData0, stride0,
                        inputData1, stride1,
                        frame->data[0], newLineSize,
                        width, height);
            return true;
#endif
        }
    }
    else if (pixelFormatInclude(frame->pixelFormat, PixelFormat::I420))
    { // I420 -> BGR24

        if (outputHasAlpha)
        {
#if ENABLE_LIBYUV
            return libyuv::I420ToARGB(inputData0, stride0,
                                      inputData1, stride1,
                                      inputData2, stride2,
                                      frame->data[0], newLineSize,
                                      width, height) == 0;
#else
            i420ToBGRA32(inputData0, stride0,
                         inputData1, stride1,
                         inputData2, stride2,
                         frame->data[0], newLineSize,
                         width, height);
            return true;
#endif
        }
        else
        {
#if ENABLE_LIBYUV
            return libyuv::I420ToRGB24(inputData0, stride0,
                                       inputData1, stride1,
                                       inputData2, stride2,
                                       frame->data[0], newLineSize,
                                       width, height) == 0;
#else
            i420ToBGR24(inputData0, stride0,
                        inputData1, stride1,
                        inputData2, stride2,
                        frame->data[0], newLineSize,
                        width, height);
            return true;
#endif
        }
    }

    return false;
}

bool inplaceConvertFrameRGB(Frame* frame, PixelFormat toFormat, bool verticalFlip, std::vector<uint8_t>& memCache)
{
    // rgb(a) 互转

    uint8_t* inputBytes = frame->data[0];
    int inputLineSize = frame->stride[0];
    auto outputChannelCount = (toFormat & kPixelFormatAlphaColorBit) ? 4 : 3;
    // Ensure 16/32 byte alignment for best performance
    auto newLineSize = outputChannelCount == 3 ? ((frame->width * 3 + 31) & ~31) : (frame->width * 4);
    auto inputFormat = frame->pixelFormat;

    auto inputChannelCount = (inputFormat & kPixelFormatAlphaColorBit) ? 4 : 3;

    bool isInputRGB = inputFormat & kPixelFormatRGBBit; ///< Not RGB means BGR
    bool isOutputRGB = toFormat & kPixelFormatRGBBit;   ///< Not RGB means BGR
    bool swapRB = isInputRGB != isOutputRGB;            ///< Whether R and B channels need to be swapped

    frame->allocator->resize(newLineSize * frame->height);

    uint8_t* outputBytes = frame->allocator->data();

    frame->stride[0] = newLineSize;
    frame->data[0] = outputBytes;
    frame->pixelFormat = toFormat;

    if (inputChannelCount == outputChannelCount)
    { /// only RGB <-> BGR, RGBA <-> BGRA
        assert(swapRB);
        if (inputChannelCount == 4) // RGBA <-> BGRA
        {
            const uint8_t kShuffleMap[4] = { 2, 1, 0, 3 }; // RGBA->BGRA 或 BGRA->RGBA
#if ENABLE_LIBYUV
            libyuv::ARGBShuffle(inputBytes, inputLineSize, outputBytes, newLineSize, kShuffleMap, frame->width, frame->height * (verticalFlip ? 1 : -1));
#else
            rgbaShuffle(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, frame->height * (verticalFlip ? 1 : -1), kShuffleMap);
#endif
        }
        else // RGB <-> BGR
        {
            const uint8_t kShuffleMap[3] = { 2, 1, 0 }; // RGB->BGR 或 BGR->RGB
            rgbShuffle(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, frame->height * (verticalFlip ? 1 : -1), kShuffleMap);
        }
    }
    else /// Different number of channels, only 4 channels <-> 3 channels
    {
        if (inputChannelCount == 4) // 4 channels -> 3 channels
        {
            if (swapRB)
            { // Possible cases: RGBA->BGR, BGRA->RGB
                rgba2bgr(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, frame->height * (verticalFlip ? 1 : -1));
            }
            else
            { // Possible cases: RGBA->RGB, BGRA->BGR
                rgba2rgb(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, frame->height * (verticalFlip ? 1 : -1));
            }
        }
        else // 3 channels -> 4 channels
        {
            if (swapRB)
            { // Possible cases: BGR->RGBA, RGB->BGRA
                rgb2bgra(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, frame->height * (verticalFlip ? 1 : -1));
            }
            else
            { // Possible cases: BGR->BGRA, RGB->RGBA
                rgb2rgba(inputBytes, inputLineSize, outputBytes, newLineSize, frame->width, frame->height * (verticalFlip ? 1 : -1));
            }
        }
    }
    return true;
}

bool inplaceConvertFrame(Frame* frame, PixelFormat toFormat, bool verticalFlip, std::vector<uint8_t>& memCache)
{
    assert(frame->pixelFormat != toFormat);

    bool isInputYUV = (frame->pixelFormat & kPixelFormatYUVColorBit) != 0;
    bool isOutputYUV = (toFormat & kPixelFormatYUVColorBit) != 0;
    if (isInputYUV || isOutputYUV) // yuv <-> rgb
    {
#if ENABLE_LIBYUV
        if (isInputYUV && isOutputYUV) // yuv <-> yuv
            return inplaceConvertFrameYUV2YUV(frame, toFormat, verticalFlip, memCache);
#endif

        if (isInputYUV) // yuv -> BGR
            return inplaceConvertFrameYUV2BGR(frame, toFormat, memCache);
        return false; // no rgb -> yuv
    }

    return inplaceConvertFrameRGB(frame, toFormat, verticalFlip, memCache);
}

} // namespace

namespace ccap
{
ProviderDirectShow::ProviderDirectShow()
{
    m_frameOrientation = kDefaultFrameOrientation;
#if ENABLE_LIBYUV
    CCAP_LOG_V("ccap: ProviderDirectShow enable libyuv acceleration\n");
#else
    CCAP_LOG_V("ccap: ProviderDirectShow enable AVX2 acceleration: %s\n", hasAVX2() ? "YES" : "NO");
#endif
}

ProviderDirectShow::~ProviderDirectShow()
{
    CCAP_LOG_V("ccap: ProviderDirectShow destructor called\n");

    ProviderDirectShow::close();
}

bool ProviderDirectShow::setup()
{
    m_didSetup = setupCom();
    return m_didSetup;
}

void ProviderDirectShow::enumerateDevices(std::function<bool(IMoniker* moniker, std::string_view)> callback)
{
    if (!setup())
    {
        return;
    }

    // Enumerate video capture devices
    ICreateDevEnum* deviceEnum = nullptr;
    auto result = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&deviceEnum);
    if (FAILED(result))
    {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_SystemDeviceEnum failed, result=0x%08X\n", result);
        return;
    }

    IEnumMoniker* enumerator = nullptr;
    result = deviceEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumerator, 0);
    deviceEnum->Release();
    if (auto failed = FAILED(result); failed || !enumerator)
    {
        if (failed)
        {
            CCAP_LOG_E("ccap: CreateClassEnumerator CLSID_VideoInputDeviceCategory failed, result=0x%08X\n", result);
        }
        else
        {
            CCAP_LOG_E("ccap: No video capture devices found.\n");
        }

        return;
    }

    IMoniker* moniker = nullptr;
    ULONG fetched = 0;
    bool stop = false;
    while (enumerator->Next(1, &moniker, &fetched) == S_OK && !stop)
    {
        IPropertyBag* propertyBag = nullptr;
        result = moniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&propertyBag);
        if (SUCCEEDED(result))
        {
            VARIANT nameVariant;
            VariantInit(&nameVariant);
            result = propertyBag->Read(L"FriendlyName", &nameVariant, 0);
            if (SUCCEEDED(result))
            {
                char deviceName[256] = { 0 };
                WideCharToMultiByte(CP_UTF8, 0, nameVariant.bstrVal, -1, deviceName, 256, nullptr, nullptr);
                stop = callback && callback(moniker, deviceName);
            }
            VariantClear(&nameVariant);
            propertyBag->Release();
        }
        moniker->Release();
    }
    enumerator->Release();
}

ProviderDirectShow::MediaInfo::~MediaInfo()
{
    for (auto& mediaType : mediaTypes)
    {
        deleteMediaType(mediaType);
    }

    if (streamConfig)
    {
        streamConfig->Release();
    }
}

std::unique_ptr<ProviderDirectShow::MediaInfo> ProviderDirectShow::enumerateMediaInfo(std::function<bool(AM_MEDIA_TYPE* mediaType, const char* name, PixelFormat pixelFormat, const DeviceInfo::Resolution& resolution)> callback)
{
    auto mediaInfo = std::make_unique<MediaInfo>();
    auto& streamConfig = mediaInfo->streamConfig;
    auto& mediaTypes = mediaInfo->mediaTypes;
    auto& videoMediaTypes = mediaInfo->videoMediaTypes;
    HRESULT hr = m_captureBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, IID_IAMStreamConfig, (void**)&streamConfig);
    if (SUCCEEDED(hr) && streamConfig)
    {
        int capabilityCount = 0, capabilitySize = 0;
        streamConfig->GetNumberOfCapabilities(&capabilityCount, &capabilitySize);

        std::vector<BYTE> capabilityData(capabilitySize);
        mediaTypes.reserve(capabilityCount);
        videoMediaTypes.reserve(capabilityCount);
        for (int i = 0; i < capabilityCount; ++i)
        {
            AM_MEDIA_TYPE* mediaType{};
            if (SUCCEEDED(streamConfig->GetStreamCaps(i, &mediaType, capabilityData.data())))
            {
                if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
                {
                    videoMediaTypes.emplace_back(mediaType);
                    if (callback)
                    {
                        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mediaType->pbFormat;
                        auto info = findPixelFormatInfo(mediaType->subtype);
                        if (callback(mediaType, info.name, info.pixelFormat, { (uint32_t)vih->bmiHeader.biWidth, (uint32_t)vih->bmiHeader.biHeight }))
                        {
                            break; // stop enumeration when returning true
                        }
                    }
                }
            }

            if (mediaType != nullptr)
            {
                mediaTypes.emplace_back(mediaType);
            }
        }
    }

    if (mediaTypes.empty())
    {
        mediaInfo = nullptr;
    }

    return mediaInfo;
}

std::vector<std::string> ProviderDirectShow::findDeviceNames()
{
    if (!m_allDeviceNames.empty())
    {
        return m_allDeviceNames;
    }

    enumerateDevices([&](IMoniker* moniker, std::string_view name) {
        // Try to bind device, check if available
        IBaseFilter* filter = nullptr;
        HRESULT hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&filter);
        if (SUCCEEDED(hr) && filter)
        {
            m_allDeviceNames.emplace_back(name.data(), name.size());
            filter->Release();
        }
        else
        {
            CCAP_LOG_I("ccap: \"%s\" is not a valid video capture device, removed\n", name.data());
        }
        // Unavailable devices are not added to the list
        return false; // Continue enumeration
    });

    { // Place virtual camera names at the end
        std::string_view keywords[] = {
            "obs",
            "virtual",
            "fake",
        };
        std::stable_sort(m_allDeviceNames.begin(), m_allDeviceNames.end(), [&](const std::string& name1, const std::string& name2) {
            std::string copyName1(name1.size(), '\0');
            std::string copyName2(name2.size(), '\0');
            std::transform(name1.begin(), name1.end(), copyName1.begin(), ::tolower);
            std::transform(name2.begin(), name2.end(), copyName2.begin(), ::tolower);
            int64_t index1 = std::find_if(std::begin(keywords), std::end(keywords),
                                          [&](std::string_view keyword) {
                                              return copyName1.find(keyword) != std::string::npos;
                                          }) -
                std::begin(keywords);
            if (index1 == std::size(keywords))
            {
                index1 = -1;
            }

            int64_t index2 = std::find_if(std::begin(keywords), std::end(keywords), [&](std::string_view keyword) {
                                 return copyName2.find(keyword) != std::string::npos;
                             }) -
                std::begin(keywords);
            if (index2 == std::size(keywords))
            {
                index2 = -1;
            }
            return index1 < index2;
        });
    }

    return m_allDeviceNames;
}

bool ProviderDirectShow::buildGraph()
{
    HRESULT hr = S_OK;

    // Create Filter Graph
    hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_graph);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_FilterGraph failed, hr=0x%08X\n", hr);
        return false;
    }

    // Create Capture Graph Builder
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&m_captureBuilder);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_CaptureGraphBuilder2 failed, hr=0x%08X\n", hr);
        return false;
    }
    m_captureBuilder->SetFiltergraph(m_graph);

    // Add device filter to the graph
    hr = m_graph->AddFilter(m_deviceFilter, L"Video Capture");
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: AddFilter Video Capture failed, hr=0x%08X\n", hr);
        return false;
    }
    return true;
}

bool ProviderDirectShow::setGrabberOutputSubtype(GUID subtype)
{
    if (m_sampleGrabber)
    {
        AM_MEDIA_TYPE mt;
        ZeroMemory(&mt, sizeof(mt));
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = subtype;
        mt.formattype = FORMAT_VideoInfo;
        HRESULT hr = m_sampleGrabber->SetMediaType(&mt);
        if (SUCCEEDED(hr))
            return false;

        CCAP_LOG_E("ccap: SetMediaType failed, hr=0x%lx\n", hr);
    }

    return false;
}

bool ProviderDirectShow::createStream()
{
    // Create SampleGrabber
    HRESULT hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_sampleGrabberFilter);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_SampleGrabber failed, hr=0x%08X\n", hr);
        return false;
    }

    hr = m_sampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&m_sampleGrabber);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: QueryInterface ISampleGrabber failed, hr=0x%08X\n", hr);
        return false;
    }

    if (auto mediaInfo = enumerateMediaInfo(nullptr))
    {
        // Desired resolution
        const int desiredWidth = m_frameProp.width;
        const int desiredHeight = m_frameProp.height;
        double closestDistance = 1.e9;

        auto& videoTypes = mediaInfo->videoMediaTypes;
        auto& streamConfig = mediaInfo->streamConfig;
        std::vector<AM_MEDIA_TYPE*> matchedTypes;
        std::vector<AM_MEDIA_TYPE*> bestMatchedTypes;

        for (auto* mediaType : videoTypes)
        {
            VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
            if (desiredWidth <= videoHeader->bmiHeader.biWidth && desiredHeight <= videoHeader->bmiHeader.biHeight)
            {
                matchedTypes.emplace_back(mediaType);
                if (verboseLogEnabled())
                    printMediaType(mediaType, "> ");
            }
            else
            {
                if (verboseLogEnabled())
                    printMediaType(mediaType, "  ");
            }
        }

        if (matchedTypes.empty())
        {
            CCAP_LOG_W("ccap: No suitable resolution found, using the closest one instead.\n");
            matchedTypes = videoTypes;
        }

        for (auto* mediaType : matchedTypes)
        {
            if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
            {
                VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                double width = static_cast<double>(videoHeader->bmiHeader.biWidth);
                double height = static_cast<double>(videoHeader->bmiHeader.biHeight);
                double distance = std::abs((width - desiredWidth) + std::abs(height - desiredHeight));
                if (distance < closestDistance)
                {
                    closestDistance = distance;
                    bestMatchedTypes = { mediaType };
                }
                else if (std::abs(distance - closestDistance) < 1e-5)
                {
                    bestMatchedTypes.emplace_back(mediaType);
                }
            }
        }

        if (!bestMatchedTypes.empty())
        { // Resolution is closest, now try to select a suitable format.

            auto preferredPixelFormat = m_frameProp.pixelFormat;
            AM_MEDIA_TYPE* mediaType = nullptr;

            // When format is YUV, only one suitable format can be found
            auto rightFormat = std::find_if(bestMatchedTypes.begin(), bestMatchedTypes.end(), [&](AM_MEDIA_TYPE* mediaType) {
                auto pixFormatInfo = findPixelFormatInfo(mediaType->subtype);
                return pixFormatInfo.pixelFormat == preferredPixelFormat || (!(preferredPixelFormat & kPixelFormatYUVColorBit) && pixFormatInfo.subtype == MEDIASUBTYPE_MJPG);
            });

            if (rightFormat != bestMatchedTypes.end())
            {
                mediaType = *rightFormat;
            }

            if (mediaType == nullptr)
            {
                mediaType = bestMatchedTypes[0];
            }

            VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
            m_frameProp.width = videoHeader->bmiHeader.biWidth;
            m_frameProp.height = videoHeader->bmiHeader.biHeight;
            m_frameProp.fps = 10000000.0 / videoHeader->AvgTimePerFrame;
            auto pixFormatInfo = findPixelFormatInfo(mediaType->subtype);
            auto subtype = mediaType->subtype;

            if (subtype == MEDIASUBTYPE_MJPG)
            {
                m_cameraPixelFormat = (m_frameProp.pixelFormat & kPixelFormatAlphaColorBit) ? PixelFormat::RGBA32 : PixelFormat::BGR24;
                subtype = (m_frameProp.pixelFormat & kPixelFormatAlphaColorBit) ? MEDIASUBTYPE_RGB32 : MEDIASUBTYPE_RGB24;
            }
            else
            {
                m_cameraPixelFormat = pixFormatInfo.pixelFormat;
            }

            setGrabberOutputSubtype(subtype);
            auto setFormatResult = streamConfig->SetFormat(mediaType);

            if (SUCCEEDED(setFormatResult))
            {
                if (ccap::infoLogEnabled())
                {
                    printMediaType(mediaType, "ccap: SetFormat succeeded: ");
                }
            }
            else
            {
                CCAP_LOG_E("ccap: SetFormat failed, result=0x%lx\n", setFormatResult);
            }
        }
    }

    // Add SampleGrabber to the Graph
    hr = m_graph->AddFilter(m_sampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: AddFilter Sample Grabber failed, result=0x%lx\n", hr);
        return false;
    }

    hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&m_dstNullFilter));
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_NullRenderer failed, result=0x%lx\n", hr);
        return false;
    }

    hr = m_graph->AddFilter(m_dstNullFilter, L"NullRenderer");
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: AddFilter NullRenderer failed, result=0x%lx\n", hr);
        return hr;
    }

    hr = m_captureBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_deviceFilter, m_sampleGrabberFilter, m_dstNullFilter);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: RenderStream failed, result=0x%lx\n", hr);
        return false;
    }

    {
        IMediaFilter* pMediaFilter = 0;
        hr = m_graph->QueryInterface(IID_IMediaFilter, (void**)&pMediaFilter);
        if (FAILED(hr))
        {
            CCAP_LOG_E("ccap: QueryInterface IMediaFilter failed, result=0x%lx\n", hr);
        }
        else
        {
            pMediaFilter->SetSyncSource(NULL);
            pMediaFilter->Release();
        }
    }
    // Get and verify the current media type
    {
        AM_MEDIA_TYPE mt;
        hr = m_sampleGrabber->GetConnectedMediaType(&mt);
        if (SUCCEEDED(hr))
        {
            CCAP_LOG_V("ccap: Connected media type: %s\n", findPixelFormatInfo(mt.subtype).name);
            freeMediaType(mt);
        }
        else
        {
            CCAP_LOG_E("ccap: GetConnectedMediaType failed, hr=0x%lx\n", hr);
            return false;
        }
    }

    // Set SampleGrabber callback
    m_sampleGrabber->SetBufferSamples(TRUE);
    m_sampleGrabber->SetOneShot(FALSE);
    m_sampleGrabber->SetCallback(this, 0); // 0 = SampleCB

    return true;
}

bool ProviderDirectShow::open(std::string_view deviceName)
{
    if (m_isOpened && m_mediaControl)
    {
        CCAP_LOG_E("ccap: Camera already opened, please close it first.\n");
        return false;
    }

    bool found = false;

    enumerateDevices([&](IMoniker* moniker, std::string_view name) {
        if (deviceName.empty() || deviceName == name)
        {
            auto hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&m_deviceFilter);
            if (SUCCEEDED(hr))
            {
                CCAP_LOG_V("ccap: Using video capture device: %s\n", name.data());
                m_deviceName = name;
                found = true;
                return true; // stop enumeration when returning true
            }
            else
            {
                if (!deviceName.empty())
                {
                    CCAP_LOG_E("ccap: \"%s\" is not a valid video capture device, bind failed, result=%x\n", deviceName.data(), hr);
                    return true; // stop enumeration when returning true
                }

                CCAP_LOG_I("ccap: bind \"%s\" failed(result=%x), try next device...\n", name.data(), hr);
            }
        }
        // continue enumerating when returning false
        return false;
    });

    if (!found)
    {
        CCAP_LOG_E("ccap: No video capture device: %s\n", deviceName.empty() ? unavailableMsg : deviceName.data());
        return false;
    }

    CCAP_LOG_I("ccap: Found video capture device: %s\n", m_deviceName.c_str());

    if (!buildGraph())
    {
        return false;
    }

    if (!createStream())
    {
        return false;
    }

    // Retrieve IMediaControl
    HRESULT hr = m_graph->QueryInterface(IID_IMediaControl, (void**)&m_mediaControl);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: QueryInterface IMediaControl failed, result=0x%lx\n", hr);
        return false;
    }

    { // Remove the `ActiveMovie Window`.
        IVideoWindow* videoWindow = nullptr;
        hr = m_graph->QueryInterface(IID_IVideoWindow, (LPVOID*)&videoWindow);
        if (FAILED(hr))
        {
            CCAP_LOG_E("ccap: QueryInterface IVideoWindow failed, result=0x%lx\n", hr);
            return hr;
        }
        videoWindow->put_AutoShow(false);
    }

    CCAP_LOG_V("ccap: Graph built successfully.\n");

    m_isOpened = true;
    m_isRunning = false;
    m_frameIndex = 0;
    return true;
}

HRESULT STDMETHODCALLTYPE ProviderDirectShow::SampleCB(double sampleTime, IMediaSample* mediaSample)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    auto newFrame = getFreeFrame();
    if (!newFrame)
    {
        if (!newFrame)
        {
            CCAP_LOG_W("ccap: Frame pool is full, a new frame skipped...\n");
        }
        return S_OK;
    }

    // Get sample data
    BYTE* sampleData = nullptr;
    if (auto hr = mediaSample->GetPointer(&sampleData); FAILED(hr))
    {
        CCAP_LOG_E("ccap: GetPointer failed, hr=0x%lx\n", hr);
        return S_OK;
    }

    bool fixTimestamp = m_firstFrameArrived && sampleTime == 0.0;

    if (!m_firstFrameArrived)
    {
        m_firstFrameArrived = true;
        m_startTime = std::chrono::steady_clock::now();
        AM_MEDIA_TYPE mt;
        HRESULT hr = m_sampleGrabber->GetConnectedMediaType(&mt);
        if (SUCCEEDED(hr))
        {
            VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.pbFormat;
            m_frameProp.width = vih->bmiHeader.biWidth;
            m_frameProp.height = vih->bmiHeader.biHeight;
            m_frameProp.fps = 10000000.0 / vih->AvgTimePerFrame;
            auto info = findPixelFormatInfo(mt.subtype);
            if (info.pixelFormat != PixelFormat::Unknown)
            {
                m_cameraPixelFormat = info.pixelFormat;
            }

            if (verboseLogEnabled())
            {
                printMediaType(&mt, "ccap: First frame media type: ");
            }

            freeMediaType(mt); // Remember to free after use
        }
    }

    if (fixTimestamp)
    { // sampleTime is wrong, implement it yourself. This often happens when using virtual cameras.
        newFrame->timestamp = (std::chrono::steady_clock::now() - m_startTime).count();
    }
    else
    {
        newFrame->timestamp = static_cast<uint64_t>(sampleTime * 1e9);
    }

    uint32_t bufferLen = mediaSample->GetActualDataLength();
    bool isYUV = (m_cameraPixelFormat & kPixelFormatYUVColorBit);
    auto defaultOrientation = isYUV ? FrameOrientation::TopToBottom : FrameOrientation::BottomToTop;

    newFrame->sizeInBytes = bufferLen;
    newFrame->pixelFormat = m_cameraPixelFormat;
    newFrame->width = m_frameProp.width;
    newFrame->height = m_frameProp.height;
    newFrame->orientation = isYUV ? FrameOrientation::TopToBottom : m_frameOrientation;

    bool shouldFlip = newFrame->orientation != defaultOrientation;
    bool shouldConvert = m_cameraPixelFormat != m_frameProp.pixelFormat;
    bool zeroCopy = !shouldConvert && !shouldFlip;

    if (isYUV)
    {
        // Zero-copy, directly reference sample data
        newFrame->data[0] = sampleData;
        newFrame->data[1] = sampleData + m_frameProp.width * m_frameProp.height;

        newFrame->stride[0] = m_frameProp.width;

        if (pixelFormatInclude(m_cameraPixelFormat, PixelFormat::I420))
        {
            newFrame->stride[1] = m_frameProp.width / 2;
            newFrame->stride[2] = m_frameProp.width / 2;

            newFrame->data[2] = sampleData + m_frameProp.width * m_frameProp.height * 5 / 4;
        }
        else
        {
            newFrame->stride[1] = m_frameProp.width;
            newFrame->stride[2] = 0;
            newFrame->data[2] = nullptr;
        }

        assert(newFrame->stride[0] * newFrame->height + newFrame->stride[1] * newFrame->height / 2 + newFrame->stride[2] * newFrame->height / 2 <= bufferLen);
    }
    else
    {
        auto stride = m_frameProp.width * (m_cameraPixelFormat & kPixelFormatAlphaColorBit ? 4 : 3);
        newFrame->stride[0] = ((stride + 3) / 4) * 4; // 4-byte aligned
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;

        // Zero-copy, directly reference sample data
        newFrame->data[0] = sampleData;
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;

        assert(newFrame->stride[0] * newFrame->height <= bufferLen);
    }

    if (!zeroCopy)
    { /// 如果执行 convert 失败, 则回退到使用 sampleData, 需要继续走 zeroCopy 的逻辑

        if (!newFrame->allocator)
        {
            newFrame->allocator = m_allocatorFactory ? m_allocatorFactory() : std::make_shared<DefaultAllocator>();
        }

        if (verboseLogEnabled())
        {
            std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
            uint64_t duration = 0;

            zeroCopy = !inplaceConvertFrame(newFrame.get(), m_frameProp.pixelFormat, shouldFlip, m_memCache);

            double durInMs = (std::chrono::steady_clock::now() - startTime).count() / 1.e6;
#ifdef DEBUG
            constexpr const char* mode = "(using Debug mode)";
#else
            constexpr const char* mode = "(using Release mode)";
#endif

            CCAP_LOG_V("ccap: inplaceConvertFrame %s, requested pixel format: %s, actual pixel format: %s, cost time %s: %g(ms)\n", zeroCopy ? "failed" : "succeeded", pixelFormatToString(m_frameProp.pixelFormat).data(), pixelFormatToString(m_cameraPixelFormat).data(), mode, durInMs);
        }
        else
        {
            zeroCopy = inplaceConvertFrame(newFrame.get(), m_frameProp.pixelFormat, shouldFlip, m_memCache);
        }
    }

    if (zeroCopy)
    {
        mediaSample->AddRef(); // Ensure data lifecycle
        auto manager = std::make_shared<FakeFrame>([newFrame, mediaSample]() mutable {
            newFrame = nullptr;
            mediaSample->Release();
        });

        newFrame = std::shared_ptr<Frame>(manager, newFrame.get());
    }

    newFrame->frameIndex = m_frameIndex++;

    if (ccap::verboseLogEnabled())
    { // Usually camera interfaces are not called from multiple threads, and verbose log is for debugging, so no lock here.
        static uint64_t s_lastFrameTime;
        static std::deque<uint64_t> s_durations;

        if (s_lastFrameTime != 0)
        {
            auto dur = newFrame->timestamp - s_lastFrameTime;
            s_durations.emplace_back(dur);
        }

        s_lastFrameTime = newFrame->timestamp;

        // use a window of 30 frames to calculate the fps
        if (s_durations.size() > 30)
        {
            s_durations.pop_front();
        }

        double fps = 0.0;

        if (!s_durations.empty())
        {
            double sum = 0.0;
            for (auto& d : s_durations)
            {
                sum += d / 1e9f;
            }
            fps = std::round(s_durations.size() / sum * 10) / 10.0;
        }

        CCAP_LOG_V("ccap: New frame available: %lux%lu, bytes %lu, Data address: %p, fps: %g\n", newFrame->width, newFrame->height, newFrame->sizeInBytes, newFrame->data[0], fps);
    }

    newFrameAvailable(std::move(newFrame));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderDirectShow::BufferCB(double SampleTime, BYTE* pBuffer, long BufferLen)
{
    CCAP_LOG_E("ccap: BufferCB called, SampleTime: %f, BufferLen: %ld\n", SampleTime, BufferLen);
    // This callback is not used in this implementation
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderDirectShow::QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject)
{
    static constexpr const IID IID_ISampleGrabberCB = { 0x0579154A, 0x2B53, 0x4994, { 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85 } };

    if (riid == IID_IUnknown)
    {
        *ppvObject = static_cast<IUnknown*>(this);
    }
    else if (riid == IID_ISampleGrabberCB)
    {
        *ppvObject = static_cast<ISampleGrabberCB*>(this);
    }
    else
    {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE ProviderDirectShow::AddRef()
{ // Using smart pointers for management, reference counting implementation is not needed
    return S_OK;
}

ULONG STDMETHODCALLTYPE ProviderDirectShow::Release()
{ // same as AddRef
    return S_OK;
}

bool ProviderDirectShow::isOpened() const
{
    return m_isOpened;
}

std::optional<DeviceInfo> ProviderDirectShow::getDeviceInfo() const
{
    std::optional<DeviceInfo> info;
    bool hasMJPG = false;

    const_cast<ProviderDirectShow*>(this)->enumerateMediaInfo([&](AM_MEDIA_TYPE* mediaType, const char* name, PixelFormat pixelFormat, const DeviceInfo::Resolution& resolution) {
        if (!info)
        {
            info.emplace();
            info->deviceName = m_deviceName;
        }

        auto& pixFormats = info->supportedPixelFormats;
        if (pixelFormat != PixelFormat::Unknown)
        {
            pixFormats.emplace_back(pixelFormat);
        }
        else if (mediaType->subtype == MEDIASUBTYPE_MJPG)
        { // Supports MJPEG format, can be decoded to BGR24 and other formats
            hasMJPG = true;
        }
        info->supportedResolutions.push_back(resolution);
        return false; // continue enumerating
    });

    if (info)
    {
        auto& resolutions = info->supportedResolutions;
        std::sort(resolutions.begin(), resolutions.end(), [](const DeviceInfo::Resolution& a, const DeviceInfo::Resolution& b) {
            return a.width * a.height < b.width * b.height;
        });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end(), [](const DeviceInfo::Resolution& a, const DeviceInfo::Resolution& b) {
                              return a.width == b.width && a.height == b.height;
                          }),
                          resolutions.end());

        auto& pixFormats = info->supportedPixelFormats;

        if (hasMJPG)
        {
            pixFormats.emplace_back(PixelFormat::BGR24);
            pixFormats.emplace_back(PixelFormat::BGRA32);
            pixFormats.emplace_back(PixelFormat::RGB24);
            pixFormats.emplace_back(PixelFormat::RGBA32);
        }
        std::sort(pixFormats.begin(), pixFormats.end());
        pixFormats.erase(std::unique(pixFormats.begin(), pixFormats.end()), pixFormats.end());
    }

    return info;
}

void ProviderDirectShow::close()
{
    CCAP_LOG_V("ccap: ProviderDirectShow close called\n");

    if (m_isRunning)
    {
        stop();
    }

    if (m_sampleGrabber != nullptr)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_sampleGrabber->SetCallback(nullptr, 0); // 0 = SampleCB
        m_sampleGrabber->SetBufferSamples(FALSE);
    }

    if (m_mediaControl)
    {
        m_mediaControl->Release();
        m_mediaControl = nullptr;
    }
    if (m_sampleGrabber)
    {
        m_sampleGrabber->Release();
        m_sampleGrabber = nullptr;
    }
    if (m_sampleGrabberFilter)
    {
        m_sampleGrabberFilter->Release();
        m_sampleGrabberFilter = nullptr;
    }
    if (m_deviceFilter)
    {
        m_deviceFilter->Release();
        m_deviceFilter = nullptr;
    }
    if (m_dstNullFilter)
    {
        m_dstNullFilter->Release();
        m_dstNullFilter = nullptr;
    }
    if (m_captureBuilder)
    {
        m_captureBuilder->Release();
        m_captureBuilder = nullptr;
    }
    if (m_graph)
    {
        m_graph->Release();
        m_graph = nullptr;
    }
    m_isOpened = false;
    m_isRunning = false;

    CCAP_LOG_V("ccap: Camera closed.\n");
}

bool ProviderDirectShow::start()
{
    if (!m_isOpened)
        return false;
    if (!m_isRunning && m_mediaControl)
    {
        HRESULT hr = m_mediaControl->Run();
        m_isRunning = !FAILED(hr);
        if (!m_isRunning)
        {
            CCAP_LOG_E("ccap: IMediaControl->Run() failed, hr=0x%08X\n", hr);
        }
        else
        {
            CCAP_LOG_V("ccap: IMediaControl->Run() succeeded.\n");
        }
    }
    return m_isRunning;
}

void ProviderDirectShow::stop()
{
    CCAP_LOG_V("ccap: ProviderDirectShow stop called\n");

    if (m_grabFrameWaiting)
    {
        CCAP_LOG_V("ccap: Frame waiting stopped\n");

        m_grabFrameWaiting = false;
        m_frameCondition.notify_all();
    }

    if (m_isRunning && m_mediaControl)
    {
        m_mediaControl->Stop();
        m_isRunning = false;

        CCAP_LOG_V("ccap: IMediaControl->Stop() succeeded.\n");
    }
}

bool ProviderDirectShow::isStarted() const
{
    return m_isRunning && m_mediaControl;
}

ProviderImp* createProviderDirectShow()
{
    return new ProviderDirectShow();
}

} // namespace ccap
#endif
