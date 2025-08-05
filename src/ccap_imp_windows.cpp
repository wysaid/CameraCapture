/**
 * @file ccap_imp_windows.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Implementation for Provider class using DSHOW.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#if defined(__GNUC__) || defined(__clang__)
/// On Windows, keep MSVC format warnings, but ignore GCC/Clang format warnings,
/// because some practices recommended on MSVC will trigger warnings on GCC/Clang,
/// and it's not really necessary to change them.
#pragma GCC diagnostic ignored "-Wformat"
#endif

#include "ccap_imp_windows.h"

#include "ccap_convert.h"
#include "ccap_convert_frame.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <guiddef.h>
#include <immintrin.h> // AVX2
#include <initguid.h>
#include <vector>

#if _CCAP_LOG_ENABLED_
#include <deque>
#endif

// The following libraries need to be linked
#pragma comment(lib, "strmiids.lib")

/// @see <https://doxygen.reactos.org/d9/dce/structtagVIDEOINFOHEADER2.html>
typedef struct tagVIDEOINFOHEADER2 {
    RECT rcSource;
    RECT rcTarget;
    DWORD dwBitRate;
    DWORD dwBitErrorRate;
    REFERENCE_TIME AvgTimePerFrame;
    DWORD dwInterlaceFlags;
    DWORD dwCopyProtectFlags;
    DWORD dwPictAspectRatioX;
    DWORD dwPictAspectRatioY;
    union {
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
typedef struct _DXVA_ExtendedFormat {
    union {
        struct {
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

namespace {
constexpr FrameOrientation kDefaultFrameOrientation = FrameOrientation::BottomToTop;

// Release the format block for a media type.
void freeMediaType(AM_MEDIA_TYPE& mt) {
    if (mt.cbFormat != 0) {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL) {
        // pUnk should not be used.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}

// Delete a media type structure that was allocated on the heap.
void deleteMediaType(AM_MEDIA_TYPE* pmt) {
    if (pmt != NULL) {
        freeMediaType(*pmt);
        CoTaskMemFree(pmt);
    }
}

struct PixelFormtInfo {
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
};

PixelFormtInfo findPixelFormatInfo(const GUID& subtype) {
    for (auto& i : s_pixelInfoList) {
        if (subtype == i.subtype) {
            return i;
        }
    }
    return { MEDIASUBTYPE_None, "Unknown", PixelFormat::Unknown };
}

struct MediaInfo {
    DeviceInfo::Resolution resolution;
    PixelFormtInfo pixelFormatInfo;
    std::shared_ptr<AM_MEDIA_TYPE*> mediaType;
};

void printMediaType(AM_MEDIA_TYPE* pmt, const char* prefix) {
    const GUID& subtype = pmt->subtype;
    PixelFormtInfo info = findPixelFormatInfo(subtype);

    const char* rangeStr = "";
    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;

    auto width = vih->bmiHeader.biWidth;
    auto height = vih->bmiHeader.biHeight;
    double fps = vih->AvgTimePerFrame != 0 ? 10000000.0 / vih->AvgTimePerFrame : 0;

    if (info.pixelFormat & kPixelFormatYUVColorBit) {
        if (pmt->formattype == FORMAT_VideoInfo2 && pmt->cbFormat >= sizeof(VIDEOINFOHEADER2)) {
            VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
            // Check AMCONTROL_COLORINFO_PRESENT
            if (vih2->dwControlFlags & AMCONTROL_COLORINFO_PRESENT) { // DXVA_ExtendedFormat follows immediately after VIDEOINFOHEADER2
                BYTE* extFmtPtr = (BYTE*)vih2 + sizeof(VIDEOINFOHEADER2);
                if (pmt->cbFormat >= sizeof(VIDEOINFOHEADER2) + sizeof(DXVA_ExtendedFormat)) {
                    DXVA_ExtendedFormat* extFmt = (DXVA_ExtendedFormat*)extFmtPtr;
                    if (extFmt->NominalRange == DXVA_NominalRange_0_255) {
                        rangeStr = " (FullRange)";
                    } else if (extFmt->NominalRange == DXVA_NominalRange_16_235) {
                        rangeStr = " (VideoRange)";
                    } else {
                        rangeStr = " (UnknownRange)";
                    }
                }
            }
        }
    }

    printf("%s%ld x %ld  bitcount=%ld  format=%s%s, fps=%g\n", prefix, width, height, vih->bmiHeader.biBitCount, info.name, rangeStr, fps);
    fflush(stdout);
}

bool setupCom() {
    static bool s_didSetup = false;
    if (!s_didSetup) {
        // Initialize COM without performing uninitialization, as other parts may also use COM
        // Use COINIT_APARTMENTTHREADED mode here, as we only use COM in a single thread
        auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        s_didSetup = !(FAILED(hr) && hr != RPC_E_CHANGED_MODE);

        if (!s_didSetup) {
            CCAP_LOG_E("ccap: CoInitializeEx failed, hr=0x%08X\n", hr);
        }
    }
    return s_didSetup;
}

#if ENABLE_LIBYUV

bool inplaceConvertFrameYUV2YUV(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip) {
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
    int height = verticalFlip ? -frame->height : frame->height;

    // NV12/I420 都是 YUV420P 格式
    frame->allocator->resize(stride0 * frame->height + (stride1 + stride2) * frame->height / 2);
    frame->data[0] = frame->allocator->data();

    uint8_t* outputData0 = frame->data[0];
    frame->data[1] = outputData0 + stride0 * frame->height;

    if (isInputNV12) { /// NV12 -> I420
        frame->stride[1] = stride1 / 2;
        frame->stride[2] = frame->stride[1];
        frame->data[2] = isOutputI420 ? frame->data[1] + stride1 * frame->height / 2 : nullptr;
        frame->pixelFormat = toFormat;

        return libyuv::NV12ToI420(inputData0, stride0, inputData1, stride1, outputData0, stride0, frame->data[1], frame->stride[1],
                                  frame->data[2], frame->stride[2], width, height) == 0;
    } else if (isInputI420) { // I420 -> NV12
        frame->stride[1] = stride1 + stride2;
        frame->stride[2] = 0;
        frame->data[2] = nullptr;

        return libyuv::I420ToNV12(inputData0, stride0, inputData1, stride1, inputData2, stride2, frame->data[0], stride0, frame->data[1],
                                  frame->stride[1], width, height) == 0;
    }

    return false;
}

#endif

} // namespace

namespace ccap {
ProviderDirectShow::ProviderDirectShow() {
    m_frameOrientation = kDefaultFrameOrientation;
#if ENABLE_LIBYUV
    CCAP_LOG_V("ccap: ProviderDirectShow enable libyuv acceleration\n");
#else
    CCAP_LOG_V("ccap: ProviderDirectShow enable AVX2 acceleration: %s\n", hasAVX2() ? "YES" : "NO");
#endif
}

ProviderDirectShow::~ProviderDirectShow() {
    CCAP_LOG_V("ccap: ProviderDirectShow destructor called\n");

    ProviderDirectShow::close();
}

bool ProviderDirectShow::setup() {
    m_didSetup = setupCom();
    return m_didSetup;
}

void ProviderDirectShow::enumerateDevices(std::function<bool(IMoniker* moniker, std::string_view)> callback) {
    if (!setup()) {
        return;
    }

    // Enumerate video capture devices
    ICreateDevEnum* deviceEnum = nullptr;
    auto result = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&deviceEnum);
    if (FAILED(result)) {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_SystemDeviceEnum failed, result=0x%08X\n", result);
        return;
    }

    IEnumMoniker* enumerator = nullptr;
    result = deviceEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumerator, 0);
    deviceEnum->Release();
    if (auto failed = FAILED(result); failed || !enumerator) {
        if (failed) {
            CCAP_LOG_E("ccap: CreateClassEnumerator CLSID_VideoInputDeviceCategory failed, result=0x%08X\n", result);
        } else {
            CCAP_LOG_E("ccap: No video capture devices found.\n");
        }

        return;
    }

    IMoniker* moniker = nullptr;
    ULONG fetched = 0;
    bool stop = false;
    while (enumerator->Next(1, &moniker, &fetched) == S_OK && !stop) {
        IPropertyBag* propertyBag = nullptr;
        result = moniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&propertyBag);
        if (SUCCEEDED(result)) {
            VARIANT nameVariant;
            VariantInit(&nameVariant);
            result = propertyBag->Read(L"FriendlyName", &nameVariant, 0);
            if (SUCCEEDED(result)) {
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

ProviderDirectShow::MediaInfo::~MediaInfo() {
    for (auto& mediaType : mediaTypes) {
        deleteMediaType(mediaType);
    }

    if (streamConfig) {
        streamConfig->Release();
    }
}

std::unique_ptr<ProviderDirectShow::MediaInfo> ProviderDirectShow::enumerateMediaInfo(
    std::function<bool(AM_MEDIA_TYPE* mediaType, const char* name, PixelFormat pixelFormat, const DeviceInfo::Resolution& resolution)>
        callback) {
    auto mediaInfo = std::make_unique<MediaInfo>();
    auto& streamConfig = mediaInfo->streamConfig;
    auto& mediaTypes = mediaInfo->mediaTypes;
    auto& videoMediaTypes = mediaInfo->videoMediaTypes;
    HRESULT hr = m_captureBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, IID_IAMStreamConfig,
                                                 (void**)&streamConfig);
    if (SUCCEEDED(hr) && streamConfig) {
        int capabilityCount = 0, capabilitySize = 0;
        streamConfig->GetNumberOfCapabilities(&capabilityCount, &capabilitySize);

        std::vector<BYTE> capabilityData(capabilitySize);
        mediaTypes.reserve(capabilityCount);
        videoMediaTypes.reserve(capabilityCount);
        for (int i = 0; i < capabilityCount; ++i) {
            AM_MEDIA_TYPE* mediaType{};
            if (SUCCEEDED(streamConfig->GetStreamCaps(i, &mediaType, capabilityData.data()))) {
                if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat) {
                    videoMediaTypes.emplace_back(mediaType);
                    if (callback) {
                        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mediaType->pbFormat;
                        auto info = findPixelFormatInfo(mediaType->subtype);
                        if (callback(mediaType, info.name, info.pixelFormat,
                                     { (uint32_t)vih->bmiHeader.biWidth, (uint32_t)vih->bmiHeader.biHeight })) {
                            break; // stop enumeration when returning true
                        }
                    }
                }
            }

            if (mediaType != nullptr) {
                mediaTypes.emplace_back(mediaType);
            }
        }
    }

    if (mediaTypes.empty()) {
        mediaInfo = nullptr;
    }

    return mediaInfo;
}

std::vector<std::string> ProviderDirectShow::findDeviceNames() {
    if (!m_allDeviceNames.empty()) {
        return m_allDeviceNames;
    }

    enumerateDevices([&](IMoniker* moniker, std::string_view name) {
        // Try to bind device, check if available
        IBaseFilter* filter = nullptr;
        HRESULT hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&filter);
        if (SUCCEEDED(hr) && filter) {
            m_allDeviceNames.emplace_back(name.data(), name.size());
            filter->Release();
        } else {
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
                                          [&](std::string_view keyword) { return copyName1.find(keyword) != std::string::npos; }) -
                std::begin(keywords);
            if (index1 == std::size(keywords)) {
                index1 = -1;
            }

            int64_t index2 = std::find_if(std::begin(keywords), std::end(keywords),
                                          [&](std::string_view keyword) { return copyName2.find(keyword) != std::string::npos; }) -
                std::begin(keywords);
            if (index2 == std::size(keywords)) {
                index2 = -1;
            }
            return index1 < index2;
        });
    }

    return m_allDeviceNames;
}

bool ProviderDirectShow::buildGraph() {
    HRESULT hr = S_OK;

    // Create Filter Graph
    hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_graph);
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_FilterGraph failed, hr=0x%08X\n", hr);
        return false;
    }

    // Create Capture Graph Builder
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&m_captureBuilder);
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_CaptureGraphBuilder2 failed, hr=0x%08X\n", hr);
        return false;
    }
    m_captureBuilder->SetFiltergraph(m_graph);

    // Add device filter to the graph
    hr = m_graph->AddFilter(m_deviceFilter, L"Video Capture");
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: AddFilter Video Capture failed, hr=0x%08X\n", hr);
        return false;
    }
    return true;
}

bool ProviderDirectShow::setGrabberOutputSubtype(GUID subtype) {
    if (m_sampleGrabber) {
        AM_MEDIA_TYPE mt;
        ZeroMemory(&mt, sizeof(mt));
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = subtype;
        mt.formattype = FORMAT_VideoInfo;
        HRESULT hr = m_sampleGrabber->SetMediaType(&mt);
        if (SUCCEEDED(hr)) return false;

        CCAP_LOG_E("ccap: SetMediaType failed, hr=0x%lx\n", hr);
    }

    return false;
}

bool ProviderDirectShow::createStream() {
    // Create SampleGrabber
    HRESULT hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_sampleGrabberFilter);
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_SampleGrabber failed, hr=0x%08X\n", hr);
        return false;
    }

    hr = m_sampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&m_sampleGrabber);
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: QueryInterface ISampleGrabber failed, hr=0x%08X\n", hr);
        return false;
    }

    if (auto mediaInfo = enumerateMediaInfo(nullptr)) {
        // Desired resolution
        const int desiredWidth = m_frameProp.width;
        const int desiredHeight = m_frameProp.height;
        double closestDistance = 1.e9;

        auto& videoTypes = mediaInfo->videoMediaTypes;
        auto& streamConfig = mediaInfo->streamConfig;
        std::vector<AM_MEDIA_TYPE*> matchedTypes;
        std::vector<AM_MEDIA_TYPE*> bestMatchedTypes;

        for (auto* mediaType : videoTypes) {
            VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
            if (desiredWidth <= videoHeader->bmiHeader.biWidth && desiredHeight <= videoHeader->bmiHeader.biHeight) {
                matchedTypes.emplace_back(mediaType);
                if (verboseLogEnabled()) printMediaType(mediaType, "> ");
            } else {
                if (verboseLogEnabled()) printMediaType(mediaType, "  ");
            }
        }

        if (matchedTypes.empty()) {
            CCAP_LOG_W("ccap: No suitable resolution found, using the closest one instead.\n");
            matchedTypes = videoTypes;
        }

        for (auto* mediaType : matchedTypes) {
            if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat) {
                VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                double width = static_cast<double>(videoHeader->bmiHeader.biWidth);
                double height = static_cast<double>(videoHeader->bmiHeader.biHeight);
                double distance = std::abs((width - desiredWidth) + std::abs(height - desiredHeight));
                if (distance < closestDistance) {
                    closestDistance = distance;
                    bestMatchedTypes = { mediaType };
                } else if (std::abs(distance - closestDistance) < 1e-5) {
                    bestMatchedTypes.emplace_back(mediaType);
                }
            }
        }

        if (!bestMatchedTypes.empty()) { // Resolution is closest, now try to select a suitable format.

            auto preferredPixelFormat = m_frameProp.cameraPixelFormat != PixelFormat::Unknown ? m_frameProp.cameraPixelFormat :
                                                                                                m_frameProp.outputPixelFormat;
            AM_MEDIA_TYPE* mediaType = nullptr;

            // When format is YUV, only one suitable format can be found
            auto rightFormat = std::find_if(bestMatchedTypes.begin(), bestMatchedTypes.end(), [&](AM_MEDIA_TYPE* mediaType) {
                auto pixFormatInfo = findPixelFormatInfo(mediaType->subtype);
                return pixFormatInfo.pixelFormat == preferredPixelFormat ||
                    (!(preferredPixelFormat & kPixelFormatYUVColorBit) && pixFormatInfo.subtype == MEDIASUBTYPE_MJPG);
            });

            if (rightFormat != bestMatchedTypes.end()) {
                mediaType = *rightFormat;
            }

            if (mediaType == nullptr) {
                mediaType = bestMatchedTypes[0];
            }

            VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
            m_frameProp.width = videoHeader->bmiHeader.biWidth;
            m_frameProp.height = videoHeader->bmiHeader.biHeight;
            m_frameProp.fps = 10000000.0 / videoHeader->AvgTimePerFrame;
            auto pixFormatInfo = findPixelFormatInfo(mediaType->subtype);
            auto subtype = mediaType->subtype;

            if (subtype == MEDIASUBTYPE_MJPG) {
                if (m_frameProp.cameraPixelFormat != PixelFormat::BGRA32) {
                    CCAP_LOG_V("ccap: MJPG format, internal format is not set to BGRA32, select BGR24\n");
                    m_frameProp.cameraPixelFormat = PixelFormat::BGR24;
                }
                subtype = (m_frameProp.cameraPixelFormat == PixelFormat::BGRA32) ? MEDIASUBTYPE_RGB32 : MEDIASUBTYPE_RGB24;
            } else {
                m_frameProp.cameraPixelFormat = pixFormatInfo.pixelFormat;
            }

            setGrabberOutputSubtype(subtype);
            auto setFormatResult = streamConfig->SetFormat(mediaType);

            if (SUCCEEDED(setFormatResult)) {
                if (ccap::infoLogEnabled()) {
                    printMediaType(mediaType, "ccap: SetFormat succeeded: ");
                }
            } else {
                CCAP_LOG_E("ccap: SetFormat failed, result=0x%lx\n", setFormatResult);
            }
        }
    }

    // Add SampleGrabber to the Graph
    hr = m_graph->AddFilter(m_sampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: AddFilter Sample Grabber failed, result=0x%lx\n", hr);
        return false;
    }

    hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&m_dstNullFilter));
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: CoCreateInstance CLSID_NullRenderer failed, result=0x%lx\n", hr);
        return false;
    }

    hr = m_graph->AddFilter(m_dstNullFilter, L"NullRenderer");
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: AddFilter NullRenderer failed, result=0x%lx\n", hr);
        return hr;
    }

    hr = m_captureBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_deviceFilter, m_sampleGrabberFilter, m_dstNullFilter);
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: RenderStream failed, result=0x%lx\n", hr);
        return false;
    }

    {
        IMediaFilter* pMediaFilter = 0;
        hr = m_graph->QueryInterface(IID_IMediaFilter, (void**)&pMediaFilter);
        if (FAILED(hr)) {
            CCAP_LOG_E("ccap: QueryInterface IMediaFilter failed, result=0x%lx\n", hr);
        } else {
            pMediaFilter->SetSyncSource(NULL);
            pMediaFilter->Release();
        }
    }
    // Get and verify the current media type
    {
        AM_MEDIA_TYPE mt;
        hr = m_sampleGrabber->GetConnectedMediaType(&mt);
        if (SUCCEEDED(hr)) {
            CCAP_LOG_V("ccap: Connected media type: %s\n", findPixelFormatInfo(mt.subtype).name);
            freeMediaType(mt);
        } else {
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

bool ProviderDirectShow::open(std::string_view deviceName) {
    if (m_isOpened && m_mediaControl) {
        CCAP_LOG_E("ccap: Camera already opened, please close it first.\n");
        return false;
    }

    bool found = false;

    enumerateDevices([&](IMoniker* moniker, std::string_view name) {
        if (deviceName.empty() || deviceName == name) {
            auto hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&m_deviceFilter);
            if (SUCCEEDED(hr)) {
                CCAP_LOG_V("ccap: Using video capture device: %s\n", name.data());
                m_deviceName = name;
                found = true;
                return true; // stop enumeration when returning true
            } else {
                if (!deviceName.empty()) {
                    CCAP_LOG_E("ccap: \"%s\" is not a valid video capture device, bind failed, result=%x\n", deviceName.data(), hr);
                    return true; // stop enumeration when returning true
                }

                CCAP_LOG_I("ccap: bind \"%s\" failed(result=%x), try next device...\n", name.data(), hr);
            }
        }
        // continue enumerating when returning false
        return false;
    });

    if (!found) {
        CCAP_LOG_E("ccap: No video capture device: %s\n", deviceName.empty() ? unavailableMsg : deviceName.data());
        return false;
    }

    CCAP_LOG_I("ccap: Found video capture device: %s\n", m_deviceName.c_str());

    if (!buildGraph()) {
        return false;
    }

    if (!createStream()) {
        return false;
    }

    // Retrieve IMediaControl
    HRESULT hr = m_graph->QueryInterface(IID_IMediaControl, (void**)&m_mediaControl);
    if (FAILED(hr)) {
        CCAP_LOG_E("ccap: QueryInterface IMediaControl failed, result=0x%lx\n", hr);
        return false;
    }

    { // Remove the `ActiveMovie Window`.
        IVideoWindow* videoWindow = nullptr;
        hr = m_graph->QueryInterface(IID_IVideoWindow, (LPVOID*)&videoWindow);
        if (FAILED(hr)) {
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

HRESULT STDMETHODCALLTYPE ProviderDirectShow::SampleCB(double sampleTime, IMediaSample* mediaSample) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    auto newFrame = getFreeFrame();
    if (!newFrame) {
        CCAP_LOG_W("ccap: VideoFrame pool is full, a new frame skipped...\n");
        return S_OK;
    }

    // Get sample data
    BYTE* sampleData = nullptr;
    if (auto hr = mediaSample->GetPointer(&sampleData); FAILED(hr)) {
        CCAP_LOG_E("ccap: GetPointer failed, hr=0x%lx\n", hr);
        return S_OK;
    }

    bool fixTimestamp = m_firstFrameArrived && sampleTime == 0.0;

    if (!m_firstFrameArrived) {
        m_firstFrameArrived = true;
        m_startTime = std::chrono::steady_clock::now();
        AM_MEDIA_TYPE mt;
        HRESULT hr = m_sampleGrabber->GetConnectedMediaType(&mt);
        if (SUCCEEDED(hr)) {
            VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.pbFormat;
            m_frameProp.width = vih->bmiHeader.biWidth;
            m_frameProp.height = vih->bmiHeader.biHeight;
            m_frameProp.fps = 10000000.0 / vih->AvgTimePerFrame;
            auto info = findPixelFormatInfo(mt.subtype);
            if (info.pixelFormat != PixelFormat::Unknown) {
                m_frameProp.cameraPixelFormat = info.pixelFormat;
            }

            if (verboseLogEnabled()) {
                printMediaType(&mt, "ccap: First frame media type: ");
            }

            freeMediaType(mt); // Remember to free after use
        }
    }

    if (fixTimestamp) { // sampleTime is wrong, implement it yourself. This often happens when using virtual cameras.
        newFrame->timestamp = (std::chrono::steady_clock::now() - m_startTime).count();
    } else {
        newFrame->timestamp = static_cast<uint64_t>(sampleTime * 1e9);
    }

    uint32_t bufferLen = mediaSample->GetActualDataLength();
    bool isInputYUV = (m_frameProp.cameraPixelFormat & kPixelFormatYUVColorBit);
    bool isOutputYUV = (m_frameProp.outputPixelFormat & kPixelFormatYUVColorBit);
    auto inputOrientation = isInputYUV ? FrameOrientation::TopToBottom : FrameOrientation::BottomToTop;

    newFrame->pixelFormat = m_frameProp.cameraPixelFormat;
    newFrame->width = m_frameProp.width;
    newFrame->height = m_frameProp.height;
    newFrame->orientation = isOutputYUV ? FrameOrientation::TopToBottom : m_frameOrientation;
    newFrame->nativeHandle = mediaSample;

    bool shouldFlip = newFrame->orientation != inputOrientation && !isOutputYUV;
    bool shouldConvert = m_frameProp.cameraPixelFormat != m_frameProp.outputPixelFormat;
    bool zeroCopy = !shouldConvert && !shouldFlip;

    if (isInputYUV) {
        // Zero-copy, directly reference sample data
        newFrame->data[0] = sampleData;
        newFrame->data[1] = sampleData + m_frameProp.width * m_frameProp.height;

        newFrame->stride[0] = m_frameProp.width;

        if (pixelFormatInclude(m_frameProp.cameraPixelFormat, PixelFormat::I420)) {
            newFrame->stride[1] = m_frameProp.width / 2;
            newFrame->stride[2] = m_frameProp.width / 2;

            newFrame->data[2] = sampleData + m_frameProp.width * m_frameProp.height * 5 / 4;
        } else {
            newFrame->stride[1] = m_frameProp.width;
            newFrame->stride[2] = 0;
            newFrame->data[2] = nullptr;
        }

        assert(newFrame->stride[0] * newFrame->height + newFrame->stride[1] * newFrame->height / 2 +
                   newFrame->stride[2] * newFrame->height / 2 <=
               bufferLen);
    } else {
        auto stride = m_frameProp.width * (m_frameProp.cameraPixelFormat & kPixelFormatAlphaColorBit ? 4 : 3);
        newFrame->stride[0] = ((stride + 3) / 4) * 4; // 4-byte aligned
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;

        // Zero-copy, directly reference sample data
        newFrame->data[0] = sampleData;
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;

        assert(newFrame->stride[0] * newFrame->height <= bufferLen);
    }

    if (!zeroCopy) { // If convert fails, fallback to using sampleData, need to continue with zeroCopy logic

        if (!newFrame->allocator) {
            newFrame->allocator = m_allocatorFactory ? m_allocatorFactory() : std::make_shared<DefaultAllocator>();
        }

        if (verboseLogEnabled()) {
#ifdef DEBUG
            constexpr const char* mode = "(Debug)";
#else
            constexpr const char* mode = "(Release)";
#endif

            std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

            zeroCopy = !inplaceConvertFrame(newFrame.get(), m_frameProp.outputPixelFormat, shouldFlip);

            double durInMs = (std::chrono::steady_clock::now() - startTime).count() / 1.e6;
            static double s_allCostTime = 0;
            static double s_frames = 0;

            if (s_frames > 60) {
                s_allCostTime = 0;
                s_frames = 0;
            }

            s_allCostTime += durInMs;
            ++s_frames;

            CCAP_LOG_V(
                "ccap: inplaceConvertFrame requested pixel format: %s, actual pixel format: %s, flip: %s, cost time %s: (cur %g ms, avg %g ms)\n",
                pixelFormatToString(m_frameProp.outputPixelFormat).data(), pixelFormatToString(m_frameProp.cameraPixelFormat).data(),
                shouldFlip ? "YES" : "NO", mode, durInMs, s_allCostTime / s_frames);
        } else {
            zeroCopy = !inplaceConvertFrame(newFrame.get(), m_frameProp.outputPixelFormat, shouldFlip);
        }

        newFrame->sizeInBytes = newFrame->stride[0] * newFrame->height + (newFrame->stride[1] + newFrame->stride[2]) * newFrame->height / 2;
    }

    if (zeroCopy) {
        // Conversion may fail. If conversion fails, fall back to zero-copy mode.
        // In this case, the returned format is the original camera input format.
        newFrame->sizeInBytes = bufferLen;

        mediaSample->AddRef(); // Ensure data lifecycle
        auto manager = std::make_shared<FakeFrame>([newFrame, mediaSample]() mutable {
            newFrame = nullptr;
            mediaSample->Release();
        });

        newFrame = std::shared_ptr<VideoFrame>(manager, newFrame.get());
    }

    newFrame->frameIndex = m_frameIndex++;

    if (ccap::verboseLogEnabled()) { // Usually camera interfaces are not called from multiple threads, and verbose log is for debugging, so
                                     // no lock here.
        static uint64_t s_lastFrameTime;
        static std::deque<uint64_t> s_durations;

        if (s_lastFrameTime != 0) {
            auto dur = newFrame->timestamp - s_lastFrameTime;
            s_durations.emplace_back(dur);
        }

        s_lastFrameTime = newFrame->timestamp;

        // use a window of 30 frames to calculate the fps
        if (s_durations.size() > 30) {
            s_durations.pop_front();
        }

        double fps = 0.0;

        if (!s_durations.empty()) {
            double sum = 0.0;
            for (auto& d : s_durations) {
                sum += d / 1e9f;
            }
            fps = std::round(s_durations.size() / sum * 10) / 10.0;
        }

        CCAP_LOG_V("ccap: New frame available: %lux%lu, bytes %lu, Data address: %p, fps: %g\n", newFrame->width, newFrame->height,
                   newFrame->sizeInBytes, newFrame->data[0], fps);
    }

    newFrameAvailable(std::move(newFrame));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderDirectShow::BufferCB(double SampleTime, BYTE* pBuffer, long BufferLen) {
    CCAP_LOG_E("ccap: BufferCB called, SampleTime: %f, BufferLen: %ld\n", SampleTime, BufferLen);
    // This callback is not used in this implementation
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderDirectShow::QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) {
    static constexpr const IID IID_ISampleGrabberCB = { 0x0579154A, 0x2B53, 0x4994, { 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85 } };

    if (riid == IID_IUnknown) {
        *ppvObject = static_cast<IUnknown*>(this);
    } else if (riid == IID_ISampleGrabberCB) {
        *ppvObject = static_cast<ISampleGrabberCB*>(this);
    } else {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE
    ProviderDirectShow::AddRef() { // Using smart pointers for management, reference counting implementation is not needed
    return S_OK;
}

ULONG STDMETHODCALLTYPE ProviderDirectShow::Release() { // same as AddRef
    return S_OK;
}

bool ProviderDirectShow::isOpened() const { return m_isOpened; }

std::optional<DeviceInfo> ProviderDirectShow::getDeviceInfo() const {
    std::optional<DeviceInfo> info;
    bool hasMJPG = false;

    const_cast<ProviderDirectShow*>(this)->enumerateMediaInfo(
        [&](AM_MEDIA_TYPE* mediaType, const char* name, PixelFormat pixelFormat, const DeviceInfo::Resolution& resolution) {
            if (!info) {
                info.emplace();
                info->deviceName = m_deviceName;
            }

            auto& pixFormats = info->supportedPixelFormats;
            if (pixelFormat != PixelFormat::Unknown) {
                pixFormats.emplace_back(pixelFormat);
            } else if (mediaType->subtype == MEDIASUBTYPE_MJPG) { // Supports MJPEG format, can be decoded to BGR24 and other formats
                hasMJPG = true;
            }
            info->supportedResolutions.push_back(resolution);
            return false; // continue enumerating
        });

    if (info) {
        auto& resolutions = info->supportedResolutions;
        std::sort(resolutions.begin(), resolutions.end(),
                  [](const DeviceInfo::Resolution& a, const DeviceInfo::Resolution& b) { return a.width * a.height < b.width * b.height; });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end(),
                                      [](const DeviceInfo::Resolution& a, const DeviceInfo::Resolution& b) {
                                          return a.width == b.width && a.height == b.height;
                                      }),
                          resolutions.end());

        auto& pixFormats = info->supportedPixelFormats;

        if (hasMJPG) {
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

void ProviderDirectShow::close() {
    CCAP_LOG_V("ccap: ProviderDirectShow close called\n");

    if (m_isRunning) {
        stop();
    }

    if (m_sampleGrabber != nullptr) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_sampleGrabber->SetCallback(nullptr, 0); // 0 = SampleCB
        m_sampleGrabber->SetBufferSamples(FALSE);
    }

    if (m_mediaControl) {
        m_mediaControl->Release();
        m_mediaControl = nullptr;
    }
    if (m_sampleGrabber) {
        m_sampleGrabber->Release();
        m_sampleGrabber = nullptr;
    }
    if (m_sampleGrabberFilter) {
        m_sampleGrabberFilter->Release();
        m_sampleGrabberFilter = nullptr;
    }
    if (m_deviceFilter) {
        m_deviceFilter->Release();
        m_deviceFilter = nullptr;
    }
    if (m_dstNullFilter) {
        m_dstNullFilter->Release();
        m_dstNullFilter = nullptr;
    }
    if (m_captureBuilder) {
        m_captureBuilder->Release();
        m_captureBuilder = nullptr;
    }
    if (m_graph) {
        m_graph->Release();
        m_graph = nullptr;
    }
    m_isOpened = false;
    m_isRunning = false;

    CCAP_LOG_V("ccap: Camera closed.\n");
}

bool ProviderDirectShow::start() {
    if (!m_isOpened) return false;
    if (!m_isRunning && m_mediaControl) {
        HRESULT hr = m_mediaControl->Run();
        m_isRunning = !FAILED(hr);
        if (!m_isRunning) {
            CCAP_LOG_E("ccap: IMediaControl->Run() failed, hr=0x%08X\n", hr);
        } else {
            CCAP_LOG_V("ccap: IMediaControl->Run() succeeded.\n");
        }
    }
    return m_isRunning;
}

void ProviderDirectShow::stop() {
    CCAP_LOG_V("ccap: ProviderDirectShow stop called\n");

    if (m_grabFrameWaiting) {
        CCAP_LOG_V("ccap: VideoFrame waiting stopped\n");

        m_grabFrameWaiting = false;
        m_frameCondition.notify_all();
    }

    if (m_isRunning && m_mediaControl) {
        m_mediaControl->Stop();
        m_isRunning = false;

        CCAP_LOG_V("ccap: IMediaControl->Stop() succeeded.\n");
    }
}

bool ProviderDirectShow::isStarted() const { return m_isRunning && m_mediaControl; }

ProviderImp* createProviderDirectShow() { return new ProviderDirectShow(); }

} // namespace ccap
#endif
