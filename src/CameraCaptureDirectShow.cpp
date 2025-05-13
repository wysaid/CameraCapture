/**
 * @file CameraCaptureDirectShow.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Implementation for ProviderDirectShow class using MSMF.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureDirectShow.h"

#include <algorithm>
#include <chrono>
#include <guiddef.h>
#include <initguid.h>
#include <iostream>
#include <vector>

#if _CCAP_LOG_ENABLED_
#include <deque>
#endif

// 需要链接以下库
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
    { MEDIASUBTYPE_RGB24, "BGR24", PixelFormat::BGR24 },   /// 这里的 RGB24 实际上是 BGR 顺序
    { MEDIASUBTYPE_RGB32, "BGRA32", PixelFormat::BGRA32 }, /// 同上
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

    printf("%s%ld x %ld  bitcount=%ld  format=%s%s\n", prefix, vih->bmiHeader.biWidth, vih->bmiHeader.biHeight, vih->bmiHeader.biBitCount, info.name, rangeStr);
    fflush(stdout);
}

} // namespace

namespace ccap
{

ProviderDirectShow::ProviderDirectShow() = default;

ProviderDirectShow::~ProviderDirectShow()
{
    ProviderDirectShow::close();
}

bool ProviderDirectShow::setup()
{
    if (!m_didSetup)
    {
        // Initialize COM without performing uninitialization, as other parts may also use COM
        // Use COINIT_APARTMENTTHREADED mode here, as we only use COM in a single thread
        auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_didSetup = !(FAILED(hr) && hr != RPC_E_CHANGED_MODE);

        if (!m_didSetup)
        {
            if (ccap::errorLogEnabled())
            {
                std::cerr << "ccap: CoInitializeEx failed, hr=0x" << std::hex << hr << std::endl;
            }
        }
    }

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
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoCreateInstance CLSID_SystemDeviceEnum failed, result=0x" << std::hex << result << std::endl;
        }
        return;
    }

    IEnumMoniker* enumerator = nullptr;
    result = deviceEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumerator, 0);
    deviceEnum->Release();
    if (FAILED(result) || !enumerator)
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CreateClassEnumerator CLSID_VideoInputDeviceCategory failed, result=0x" << std::hex << result << std::endl;
            if (result == S_OK)
            {
                std::cerr << "ccap: No video capture devices found." << std::endl;
            }
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

void ProviderDirectShow::enumerateMediaInfo(std::function<bool(AM_MEDIA_TYPE* mediaType, const char* name, PixelFormat pixelFormat, const DeviceInfo::Resolution& resolution)> callback)
{
    IAMStreamConfig* streamConfig = nullptr;
    HRESULT hr = m_captureBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, IID_IAMStreamConfig, (void**)&streamConfig);
    if (SUCCEEDED(hr) && streamConfig)
    {
        int capabilityCount = 0, capabilitySize = 0;
        streamConfig->GetNumberOfCapabilities(&capabilityCount, &capabilitySize);

        std::vector<BYTE> capabilityData(capabilitySize);
        for (int i = 0; i < capabilityCount; ++i)
        {
            AM_MEDIA_TYPE* mediaType{};
            if (SUCCEEDED(streamConfig->GetStreamCaps(i, &mediaType, capabilityData.data())))
            {
                if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
                {
                    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mediaType->pbFormat;
                    auto info = findPixelFormatInfo(mediaType->subtype);
                    if (callback(mediaType, info.name, info.pixelFormat, { (uint32_t)vih->bmiHeader.biWidth, (uint32_t)vih->bmiHeader.biHeight }))
                    {
                        break; // stop enumeration when returning true
                    }
                }
            }

            if (mediaType != nullptr)
            {
                deleteMediaType(mediaType);
            }
        }

        streamConfig->Release();
    }
}

std::vector<std::string> ProviderDirectShow::findDeviceNames()
{
    if (!m_allDeviceNames.empty())
    {
        return m_allDeviceNames;
    }

    enumerateDevices([&](IMoniker* moniker, std::string_view name) {
        // 尝试绑定设备，判断是否可用
        IBaseFilter* filter = nullptr;
        HRESULT hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&filter);
        if (SUCCEEDED(hr) && filter)
        {
            m_allDeviceNames.emplace_back(name.data(), name.size());
            filter->Release();
        }
        else if (infoLogEnabled())
        {
            fprintf(stderr, "ccap: \"%s\" is not a valid video capture device, removed\n", name.data());
        }
        // 不可用设备不加入列表
        return false; // 继续枚举
    });

    { /// Place virtual camera names at the end
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
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoCreateInstance CLSID_FilterGraph failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    // Create Capture Graph Builder
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&m_captureBuilder);
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoCreateInstance CLSID_CaptureGraphBuilder2 failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }
    m_captureBuilder->SetFiltergraph(m_graph);

    // Add device filter to the graph
    hr = m_graph->AddFilter(m_deviceFilter, L"Video Capture");
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: AddFilter Video Capture failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }
    return true;
}

bool ProviderDirectShow::createStream()
{
    // Create SampleGrabber
    HRESULT hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_sampleGrabberFilter);
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoCreateInstance CLSID_SampleGrabber failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    hr = m_sampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&m_sampleGrabber);
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: QueryInterface ISampleGrabber failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    // Set the media type for the SampleGrabber
    {
        AM_MEDIA_TYPE mt;
        ZeroMemory(&mt, sizeof(mt));
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = GUID_NULL; // MEDIASUBTYPE_NV12;
        mt.formattype = GUID_NULL;
        hr = m_sampleGrabber->SetMediaType(&mt);
        if (FAILED(hr))
        {
            if (ccap::errorLogEnabled())
            {
                std::cerr << "ccap: SetMediaType failed, hr=0x" << std::hex << hr << std::endl;
            }
            return false;
        }
    }

    // Add SampleGrabber to the Graph
    hr = m_graph->AddFilter(m_sampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: AddFilter Sample Grabber failed, result=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    { // Retrieve supported resolutions from the camera device
        IAMStreamConfig* streamConfig = nullptr;
        hr = m_captureBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, IID_IAMStreamConfig, (void**)&streamConfig);
        if (SUCCEEDED(hr) && streamConfig)
        {
            int capabilityCount = 0, capabilitySize = 0;
            streamConfig->GetNumberOfCapabilities(&capabilityCount, &capabilitySize);
            if (verboseLogEnabled())
            {
                printf("ccap: (%s) Found %d supported resolutions.\n", m_deviceName.c_str(), capabilityCount);
            }

            std::vector<BYTE> capabilityData(capabilitySize);

            // Desired resolution
            const int desiredWidth = m_frameProp.width;
            const int desiredHeight = m_frameProp.height;
            double closestDistance = 1.e9;
            AM_MEDIA_TYPE* bestMatchType = nullptr;
            std::vector<AM_MEDIA_TYPE*> mediaTypes(capabilityCount);
            std::vector<AM_MEDIA_TYPE*> videoTypes;
            std::vector<AM_MEDIA_TYPE*> matchedTypes;
            videoTypes.reserve(capabilityCount);
            matchedTypes.reserve(capabilityCount);

            // auto allMediaInfo = getAllSupportedMediaInfo(m_captureBuilder, m_deviceFilter);

            for (int i = 0; i < capabilityCount; ++i)
            {
                auto& mediaType = mediaTypes[i];
                if (SUCCEEDED(streamConfig->GetStreamCaps(i, &mediaType, capabilityData.data())))
                {
                    if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
                    {
                        VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                        if (desiredWidth <= videoHeader->bmiHeader.biWidth && desiredHeight <= videoHeader->bmiHeader.biHeight)
                        {
                            matchedTypes.emplace_back(mediaType);
                            if (verboseLogEnabled())
                            {
                                printMediaType(mediaType, "> ");
                            }
                        }
                        else
                        {
                            if (verboseLogEnabled())
                            {
                                printMediaType(mediaType, "  ");
                            }
                        }

                        videoTypes.emplace_back(mediaType);
                    }
                }
            }

            if (matchedTypes.empty())
            {
                if (ccap::warningLogEnabled())
                {
                    std::cerr << "ccap: No suitable resolution found, using the closest one instead." << std::endl;
                }
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
                    if (distance < closestDistance && mediaType->subtype == MEDIASUBTYPE_I420)
                    {
                        closestDistance = distance;
                        bestMatchType = mediaType;
                    }
                }
            }

            if (bestMatchType != nullptr)
            {
                auto* mediaType = bestMatchType;
                if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
                {
                    VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                    m_frameProp.width = videoHeader->bmiHeader.biWidth;
                    m_frameProp.height = videoHeader->bmiHeader.biHeight;
                    m_frameProp.fps = 10000000.0 / videoHeader->AvgTimePerFrame;
                    m_frameProp.pixelFormat = PixelFormat::I420; // Assume RGB24 format
                    auto setFormatResult = streamConfig->SetFormat(mediaType);
                    if (FAILED(setFormatResult))
                    {
                        if (ccap::errorLogEnabled())
                        {
                            std::cerr << "ccap: SetFormat failed, result=0x" << std::hex << setFormatResult << std::endl;
                        }
                    }
                    else if (ccap::infoLogEnabled())
                    {
                        printMediaType(mediaType, "ccap: SetFormat succeeded: ");
                    }
                }
            }

            for (auto* mediaType : mediaTypes)
            {
                if (mediaType != nullptr)
                {
                    deleteMediaType(mediaType);
                }
            }

            streamConfig->Release();
        }
    }

    hr = m_captureBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, m_sampleGrabberFilter, nullptr);

    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: RenderStream failed, result=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    // 获取当前媒体类型验证
    {
        AM_MEDIA_TYPE mt;
        hr = m_sampleGrabber->GetConnectedMediaType(&mt);
        if (SUCCEEDED(hr))
        {
            if (verboseLogEnabled())
            {
                // 输出媒体类型信息
                auto info = findPixelFormatInfo(mt.subtype);
                fprintf(stderr, "ccap: Connected media type: %s\n", info.name);
            }
            freeMediaType(mt);
        }
        else
        {
            if (ccap::errorLogEnabled())
            {
                std::cerr << "ccap: GetConnectedMediaType failed, hr=0x" << std::hex << hr << std::endl;
            }
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
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: Camera already opened, please close it first." << std::endl;
        }
        return false;
    }

    bool found = false;

    enumerateDevices([&](IMoniker* moniker, std::string_view name) {
        if (deviceName.empty() || deviceName == name)
        {
            auto hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&m_deviceFilter);
            if (SUCCEEDED(hr))
            {
                m_deviceName = name;
                if (ccap::verboseLogEnabled())
                {
                    std::cout << "ccap: Using video capture device: " << name << std::endl;
                }
                found = true;
                return true; // stop enumeration when returning true
            }
            else
            {
                if (ccap::errorLogEnabled())
                {
                    if (!deviceName.empty())
                    {
                        fprintf(stderr, "ccap: \"%s\" is not a valid video capture device, bind failed, result=%x\n", deviceName.data(), hr);
                        return true; // stop enumeration when returning true
                    }
                }

                if (infoLogEnabled())
                {
                    fprintf(stderr, "ccap: bind \"%s\" failed(result=%x), try next device...\n", name.data(), hr);
                }
            }
        }
        // continue enumerating when returning false
        return false;
    });

    if (!found)
    {
        if (ccap::errorLogEnabled())
        {
            fprintf(stderr, "ccap: No video capture device: %s\n", deviceName.empty() ? "" : deviceName.data());
        }
        return false;
    }

    if (infoLogEnabled())
    {
        fprintf(stderr, "ccap: Found video capture device: %s\n", m_deviceName.c_str());
    }

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
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: QueryInterface IMediaControl failed, result=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    { // Remove the `ActiveMovie Window`.
        IVideoWindow* videoWindow = nullptr;
        hr = m_graph->QueryInterface(IID_IVideoWindow, (LPVOID*)&videoWindow);
        if (FAILED(hr))
        {
            if (ccap::errorLogEnabled())
            {
                std::cerr << "ccap: QueryInterface IVideoWindow failed, result=0x" << std::hex << hr << std::endl;
            }
            return hr;
        }
        videoWindow->put_AutoShow(false);
    }

    if (ccap::verboseLogEnabled())
    {
        std::cout << "ccap: Camera opened successfully." << std::endl;
    }

    m_isOpened = true;
    m_isRunning = false;
    m_frameIndex = 0;
    return true;
}

HRESULT STDMETHODCALLTYPE ProviderDirectShow::SampleCB(double sampleTime, IMediaSample* mediaSample)
{
    auto newFrame = getFreeFrame();
    if (!newFrame)
    {
        if (!newFrame && ccap::warningLogEnabled())
        {
            std::cerr << "ccap: Frame pool is full, a new frame skipped..." << std::endl;
        }
        return S_OK;
    }

    // Get sample data
    BYTE* sampleData = nullptr;
    if (FAILED(mediaSample->GetPointer(&sampleData)))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: GetPointer failed." << std::endl;
        }
        return S_OK;
    }

    long bufferLen = mediaSample->GetActualDataLength();

    // Convert to nanoseconds
    newFrame->timestamp = static_cast<uint64_t>(sampleTime * 1e9);

    // Zero-copy, directly reference sample data
    newFrame->sizeInBytes = bufferLen;
    newFrame->pixelFormat = PixelFormat::I420; // Assume RGB24 format
    newFrame->width = m_frameProp.width;
    newFrame->height = m_frameProp.height;
    newFrame->stride[0] = m_frameProp.width * 3; // BGR24, 3 bytes per pixel
    newFrame->stride[1] = 0;
    newFrame->stride[2] = 0;
    newFrame->data[0] = sampleData;
    newFrame->data[1] = nullptr;
    newFrame->data[2] = nullptr;

    mediaSample->AddRef(); // Ensure data lifecycle
    auto manager = std::make_shared<FakeFrame>([newFrame, mediaSample]() mutable {
        newFrame = nullptr;
        mediaSample->Release();
    });

    newFrame->frameIndex = m_frameIndex++;
    auto fakeFrame = std::shared_ptr<Frame>(manager, newFrame.get());
    newFrame = fakeFrame;

    if (ccap::verboseLogEnabled())
    { /// 通常不会多线程调用相机接口, 而且 verbose 日志只是用于调试, 所以这里不加锁.
        static uint64_t s_lastFrameTime;
        static std::deque<uint64_t> s_durations;

        if (s_lastFrameTime != 0)
        {
            auto dur = newFrame->timestamp - s_lastFrameTime;
            s_durations.emplace_back(dur);
        }

        s_lastFrameTime = newFrame->timestamp;

        /// use a window of 30 frames to calculate the fps
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
        printf("ccap: New frame available: %lux%lu, bytes %lu, Data address: %p, fps: %g\n", newFrame->width, newFrame->height, newFrame->sizeInBytes, newFrame->data[0], fps);
    }

    newFrameAvailable(std::move(newFrame));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderDirectShow::BufferCB(double SampleTime, BYTE* pBuffer, long BufferLen)
{
    fprintf(stderr, "ccap: BufferCB called, SampleTime: %f, BufferLen: %ld\n", SampleTime, BufferLen);

    // This callback is not used in this implementation
    return S_OK;
}

void ProviderDirectShow::fetchNewFrame()
{ /// 参考 SampleCB， 尝试主动获取一帧数据
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
        auto pixFormat = findPixelFormatInfo(mediaType->subtype);
        auto& pixFormats = info->supportedPixelFormats;
        if (pixelFormat != PixelFormat::Unknown)
        {
            pixFormats.emplace_back(pixFormat.pixelFormat);
        }
        else if (mediaType->subtype == MEDIASUBTYPE_MJPG)
        { /// 支持 MJPEG 格式, 可以解码为 BGR24 等格式
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
    if (m_mediaControl)
    {
        m_mediaControl->Stop();
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

    if (ccap::verboseLogEnabled())
    {
        std::cout << "ccap: Camera closed." << std::endl;
    }
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
            if (ccap::errorLogEnabled())
            {
                std::cerr << "ccap: IMediaControl->Run() failed, hr=0x" << std::hex << hr << std::endl;
            }
        }
        else
        {
            if (ccap::verboseLogEnabled())
            {
                std::cout << "ccap: Camera started." << std::endl;
            }
        }
    }
    return m_isRunning;
}

void ProviderDirectShow::stop()
{
    if (m_isRunning && m_mediaControl)
    {
        m_mediaControl->Stop();
        m_isRunning = false;

        if (ccap::verboseLogEnabled())
        {
            std::cout << "ccap: Camera stopped." << std::endl;
        }
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
