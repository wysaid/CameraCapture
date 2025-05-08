/**
 * @file CameraCaptureWin.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Implementation for ProviderWin class using MSMF.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureWin.h"

#include <chrono>
#include <iostream>
#include <vector>

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
#define DXVA_NominalRange_Wide 2 // 0-255
#define DXVA_NominalRange_0_255 2
#define DXVA_NominalRange_16_235 1
#endif

namespace ccap
{
ProviderWin::ProviderWin() = default;

ProviderWin::~ProviderWin()
{
    ProviderWin::close();
}

static void printMediaType(AM_MEDIA_TYPE* pmt, const char* prefix)
{
    const GUID& subtype = pmt->subtype;
    const char* fmt = "Unknown";
    if (subtype == MEDIASUBTYPE_RGB24)
        fmt = "RGB24";
    else if (subtype == MEDIASUBTYPE_RGB32)
        fmt = "RGB32";
    else if (subtype == MEDIASUBTYPE_RGB565)
        fmt = "RGB565";
    else if (subtype == MEDIASUBTYPE_RGB555)
        fmt = "RGB555";
    else if (subtype == MEDIASUBTYPE_YUY2)
        fmt = "YUY2";
    else if (subtype == MEDIASUBTYPE_YV12)
        fmt = "YV12";
    else if (subtype == MEDIASUBTYPE_UYVY)
        fmt = "UYVY";
    else if (subtype == MEDIASUBTYPE_MJPG)
        fmt = "MJPG";
    else if (subtype == MEDIASUBTYPE_YVYU)
        fmt = "YVYU";
    else if (subtype == MEDIASUBTYPE_NV12)
        fmt = "NV12";

    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;

    const char* rangeStr = "";
    if (pmt->formattype == FORMAT_VideoInfo2 && pmt->cbFormat >= sizeof(VIDEOINFOHEADER2))
    {
        VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
        // Check AMCONTROL_COLORINFO_PRESENT
        if (vih2->dwControlFlags & AMCONTROL_COLORINFO_PRESENT)
        {
            // DXVA_ExtendedFormat follows immediately after VIDEOINFOHEADER2
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

    printf("%s%ld x %ld  bitcount=%ld  format=%s%s\n", prefix, vih->bmiHeader.biWidth, vih->bmiHeader.biHeight, vih->bmiHeader.biBitCount, fmt, rangeStr);
}

std::vector<std::string> ProviderWin::findDeviceNames()
{
    std::vector<std::string> deviceNames;
    enumerateDevices([&](IMoniker* moniker, std::string_view name) {
        deviceNames.emplace_back(name.data(), name.size());
        return false; // continue enumerating
    });
    return deviceNames;
}

bool ProviderWin::setup()
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

void ProviderWin::enumerateDevices(std::function<bool(IMoniker* moniker, std::string_view)> callback)
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

bool ProviderWin::open(std::string_view deviceName)
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
                if (ccap::verboseLogEnabled())
                {
                    std::cout << "ccap: Using video capture device: " << name << std::endl;
                }
                found = true;
                return true; // stop enumeration when returning true
            }
        }
        // continue enumerating when returning false
        return false;
    });

    if (!found)
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: No matching video capture device found." << std::endl;
        }
        return false;
    }

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

    // Create SampleGrabber
    hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_sampleGrabberFilter);
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
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(mt));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    mt.formattype = FORMAT_VideoInfo;
    hr = m_sampleGrabber->SetMediaType(&mt);
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: SetMediaType failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
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
            if (ccap::infoLogEnabled())
            {
                std::cout << "ccap: Found " << capabilityCount << " supported resolutions." << std::endl;
            }

            std::vector<BYTE> capabilityData(capabilitySize);

            // Desired resolution
            const int desiredWidth = m_frameProp.width;
            const int desiredHeight = m_frameProp.height;
            double closestDistance = 1.e9;
            int bestMatchIndex = -1;
            std::vector<AM_MEDIA_TYPE*> mediaTypes(capabilityCount);
            std::vector<int> videoIndices;
            std::vector<int> matchedIndices;
            videoIndices.reserve(capabilityCount);
            matchedIndices.reserve(capabilityCount);

            for (int i = 0; i < capabilityCount; i++)
            {
                auto& mediaType = mediaTypes[i];
                if (SUCCEEDED(streamConfig->GetStreamCaps(i, &mediaType, capabilityData.data())))
                {
                    if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
                    {
                        if (ccap::infoLogEnabled())
                        {
                            printMediaType(mediaType, "  ");
                        }

                        VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                        if (desiredWidth <= videoHeader->bmiHeader.biWidth && desiredHeight <= videoHeader->bmiHeader.biHeight)
                        {
                            matchedIndices.emplace_back(i);
                        }

                        videoIndices.emplace_back(i);
                    }
                }
            }

            if (matchedIndices.empty())
            {
                if (ccap::warningLogEnabled())
                {
                    std::cerr << "ccap: No suitable resolution found, using the closest one instead." << std::endl;
                }
                matchedIndices = videoIndices;
            }

            for (int index : matchedIndices)
            {
                auto& mediaType = mediaTypes[index];
                if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
                {
                    VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                    double width = static_cast<double>(videoHeader->bmiHeader.biWidth);
                    double height = static_cast<double>(videoHeader->bmiHeader.biHeight);
                    double distance = std::abs((width - desiredWidth) + std::abs(height - desiredHeight));
                    if (distance < closestDistance)
                    {
                        closestDistance = distance;
                        bestMatchIndex = index;
                    }
                }
            }

            if (bestMatchIndex >= 0)
            {
                auto& mediaType = mediaTypes[bestMatchIndex];
                if (mediaType->formattype == FORMAT_VideoInfo && mediaType->pbFormat)
                {
                    VIDEOINFOHEADER* videoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
                    m_frameProp.width = videoHeader->bmiHeader.biWidth;
                    m_frameProp.height = videoHeader->bmiHeader.biHeight;
                    m_frameProp.fps = 10000000.0 / videoHeader->AvgTimePerFrame;
                    m_frameProp.pixelFormat = PixelFormat::BGR24; // Assume RGB24 format
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
                    if (mediaType->cbFormat != 0)
                    {
                        CoTaskMemFree((PVOID)mediaType->pbFormat);
                        mediaType->cbFormat = 0;
                        mediaType->pbFormat = nullptr;
                    }
                    CoTaskMemFree(mediaType);
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

    // Set SampleGrabber callback
    m_sampleGrabber->SetBufferSamples(TRUE);
    m_sampleGrabber->SetOneShot(FALSE);
    m_sampleGrabber->SetCallback(this, 0); // 0 = SampleCB

    // Retrieve IMediaControl
    hr = m_graph->QueryInterface(IID_IMediaControl, (void**)&m_mediaControl);
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

HRESULT STDMETHODCALLTYPE ProviderWin::SampleCB(double sampleTime, IMediaSample* mediaSample)
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
    newFrame->pixelFormat = PixelFormat::BGR24; // Assume RGB24 format
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
    {
        static uint64_t s_lastFrameTime = 0;
        static int s_frameCounter{};
        static double s_duration{};
        static double s_fps{ 0.0 };

        if (s_lastFrameTime != 0)
        {
            s_duration += (newFrame->timestamp - s_lastFrameTime) / 1.e9;
        }
        s_lastFrameTime = newFrame->timestamp;
        ++s_frameCounter;

        if (s_duration > 0.5 || s_frameCounter >= 30)
        {
            auto newFps = s_frameCounter / s_duration;
            constexpr double alpha = 0.8; // Smoothing factor, smaller value means smoother
            s_fps = alpha * newFps + (1.0 - alpha) * s_fps;
            s_frameCounter = 0;
            s_duration = 0;
        }

        double roundedFps = std::round(s_fps * 10.0) / 10.0;
        printf("ccap: New frame available: %lux%lu, bytes %lu, Data address: %p, fps: %g\n", newFrame->width, newFrame->height, newFrame->sizeInBytes, newFrame->data[0], roundedFps);
    }

    newFrameAvailable(std::move(newFrame));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderWin::BufferCB(double SampleTime, BYTE* pBuffer, long BufferLen)
{
    /// Never reached.
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderWin::QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject)
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

ULONG STDMETHODCALLTYPE ProviderWin::AddRef()
{ // Using smart pointers for management, reference counting implementation is not needed
    return S_OK;
}

ULONG STDMETHODCALLTYPE ProviderWin::Release()
{ // same as AddRef
    return S_OK;
}

bool ProviderWin::isOpened() const
{
    return m_isOpened;
}

void ProviderWin::close()
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

bool ProviderWin::start()
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

void ProviderWin::stop()
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

bool ProviderWin::isStarted() const
{
    return m_isRunning && m_mediaControl;
}

} // namespace ccap
#endif