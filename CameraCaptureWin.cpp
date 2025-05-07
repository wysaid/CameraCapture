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
        // 检查 AMCONTROL_COLORINFO_PRESENT
        if (vih2->dwControlFlags & AMCONTROL_COLORINFO_PRESENT)
        {
            // DXVA_ExtendedFormat 紧跟在 VIDEOINFOHEADER2 之后
            BYTE* extFmtPtr = (BYTE*)vih2 + sizeof(VIDEOINFOHEADER2);
            if (pmt->cbFormat >= sizeof(VIDEOINFOHEADER2) + sizeof(DXVA_ExtendedFormat))
            {
                DXVA_ExtendedFormat* extFmt = (DXVA_ExtendedFormat*)extFmtPtr;
                if (extFmt->VideoNominalRange == DXVA_NominalRange_0_255)
                {
                    rangeStr = " (FullRange)";
                }
                else if (extFmt->VideoNominalRange == DXVA_NominalRange_16_235)
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

    HRESULT hr = S_OK;

    // Initialize COM
    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoInitializeEx failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

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

    // Enumerate video capture devices
    ICreateDevEnum* pDevEnum = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pDevEnum);
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoCreateInstance CLSID_SystemDeviceEnum failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    IEnumMoniker* pEnum = nullptr;
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    pDevEnum->Release();
    if (FAILED(hr) || !pEnum)
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CreateClassEnumerator CLSID_VideoInputDeviceCategory failed, hr=0x" << std::hex << hr << std::endl;
            if (hr == S_OK)
            {
                std::cerr << "ccap: No video capture devices found." << std::endl;
            }
        }

        return false;
    }

    IMoniker* pMoniker = nullptr;
    ULONG fetched = 0;
    bool found = false;
    while (pEnum->Next(1, &pMoniker, &fetched) == S_OK)
    {
        IPropertyBag* pPropBag = nullptr;
        hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
        if (SUCCEEDED(hr))
        {
            VARIANT varName;
            VariantInit(&varName);
            hr = pPropBag->Read(L"FriendlyName", &varName, 0);
            if (SUCCEEDED(hr))
            {
                char narrowName[256] = { 0 };
                WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, narrowName, 256, nullptr, nullptr);
                if (deviceName.empty() || deviceName == narrowName)
                {
                    if (ccap::infoLogEnabled())
                    {
                        std::cout << "ccap: Found video capture device: " << narrowName << std::endl;
                    }

                    hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&m_deviceFilter);
                    found = SUCCEEDED(hr);
                    VariantClear(&varName);
                    pPropBag->Release();
                    break;
                }
            }
            VariantClear(&varName);
            pPropBag->Release();
        }
        pMoniker->Release();
    }
    pEnum->Release();
    if (!found)
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: No matching video capture device found." << std::endl;
        }
        return false;
    }

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

    // 设置 SampleGrabber 媒体类型
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

    // 添加 SampleGrabber 到 Graph
    hr = m_graph->AddFilter(m_sampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: AddFilter Sample Grabber failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    { // 获取相机设备支持的分辨率
        IAMStreamConfig* pConfig = nullptr;
        hr = m_captureBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, IID_IAMStreamConfig, (void**)&pConfig);
        if (SUCCEEDED(hr) && pConfig)
        {
            int iCount = 0, iSize = 0;
            pConfig->GetNumberOfCapabilities(&iCount, &iSize);
            if (ccap::infoLogEnabled())
            {
                std::cout << "ccap: Found " << iCount << " supported resolutions." << std::endl;
            }

            std::vector<BYTE> sccs(iSize);

            // 你想要设置的分辨率
            const int desiredWidth = m_frameProp.width;
            const int desiredHeight = m_frameProp.height;
            double distance = 1.e9;
            int bestIndex = -1;
            std::vector<AM_MEDIA_TYPE*> allMediaTypes(iCount);
            std::vector<int> videoIndices;
            std::vector<int> matchedIndices;
            videoIndices.reserve(iCount);
            matchedIndices.reserve(iCount);

            for (int i = 0; i < iCount; i++)
            {
                auto& pmt = allMediaTypes[i];
                if (SUCCEEDED(pConfig->GetStreamCaps(i, &pmt, sccs.data())))
                {
                    if (pmt->formattype == FORMAT_VideoInfo && pmt->pbFormat)
                    {
                        if (ccap::infoLogEnabled())
                        {
                            printMediaType(pmt, "  ");
                        }

                        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
                        if (desiredWidth <= vih->bmiHeader.biWidth && desiredHeight <= vih->bmiHeader.biHeight)
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

            for (int i : matchedIndices)
            {
                auto& pmt = allMediaTypes[i];
                if (pmt->formattype == FORMAT_VideoInfo && pmt->pbFormat)
                {
                    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
                    double w = static_cast<double>(vih->bmiHeader.biWidth);
                    double h = static_cast<double>(vih->bmiHeader.biHeight);
                    double d = std::abs((w - desiredWidth) + std::abs(h - desiredHeight));
                    if (d < distance)
                    {
                        distance = d;
                        bestIndex = i;
                    }
                }
            }

            if (bestIndex >= 0)
            {
                auto& pmt = allMediaTypes[bestIndex];
                if (pmt->formattype == FORMAT_VideoInfo && pmt->pbFormat)
                {
                    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
                    m_frameProp.width = vih->bmiHeader.biWidth;
                    m_frameProp.height = vih->bmiHeader.biHeight;
                    m_frameProp.fps = 10000000.0 / vih->AvgTimePerFrame;
                    m_frameProp.pixelFormat = PixelFormat::BGR888; // 假设 RGB24 格式
                    auto hr = pConfig->SetFormat(pmt);
                    if (FAILED(hr))
                    {
                        if (ccap::errorLogEnabled())
                        {
                            std::cerr << "ccap: SetFormat failed, hr=0x" << std::hex << hr << std::endl;
                        }
                    }
                    else if (ccap::infoLogEnabled())
                    {
                        printMediaType(pmt, "ccap: SetFormat succeeded: ");
                    }
                }
            }

            for (auto* pmt : allMediaTypes)
            {
                if (pmt != nullptr)
                {
                    if (pmt->cbFormat != 0)
                    {
                        CoTaskMemFree((PVOID)pmt->pbFormat);
                        pmt->cbFormat = 0;
                        pmt->pbFormat = nullptr;
                    }
                    CoTaskMemFree(pmt);
                }
            }

            pConfig->Release();
        }
    }

    hr = m_captureBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, m_sampleGrabberFilter, nullptr);

    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: RenderStream failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    // 设置 SampleGrabber 回调
    m_sampleGrabber->SetBufferSamples(TRUE);
    m_sampleGrabber->SetOneShot(FALSE);
    m_sampleGrabber->SetCallback(this, 0); // 0 = SampleCB

    // 获取 IMediaControl
    hr = m_graph->QueryInterface(IID_IMediaControl, (void**)&m_mediaControl);
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: QueryInterface IMediaControl failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    { // Remove the `ActiveMovie Window`.
        IVideoWindow* pVideoWindow = nullptr;
        hr = m_graph->QueryInterface(IID_IVideoWindow, (LPVOID*)&pVideoWindow);
        if (FAILED(hr))
        {
            if (ccap::errorLogEnabled())
            {
                std::cerr << "ccap: QueryInterface IVideoWindow failed, hr=0x" << std::hex << hr << std::endl;
            }
            return hr;
        }
        pVideoWindow->put_AutoShow(false);
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

    // 获取 sample 数据
    BYTE* pBuffer = nullptr;
    if (FAILED(mediaSample->GetPointer(&pBuffer)))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: GetPointer failed." << std::endl;
        }
        return S_OK;
    }

    long bufferLen = mediaSample->GetActualDataLength();
    bool noCopy = newFrame->allocator == nullptr;

    // 转换为纳秒
    newFrame->timestamp = static_cast<uint64_t>(sampleTime * 1e9);

    if (noCopy)
    { // 零拷贝，直接引用 sample 数据
        newFrame->sizeInBytes = bufferLen;
        newFrame->pixelFormat = PixelFormat::BGR888; // 假设 RGB24 格式
        newFrame->width = m_frameProp.width;
        newFrame->height = m_frameProp.height;
        newFrame->stride[0] = m_frameProp.width * 3; // BGR888 每个像素 3 字节
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;
        newFrame->data[0] = pBuffer;
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;

        mediaSample->AddRef(); // 保证数据生命周期
        auto manager = std::make_shared<FakeFrame>([newFrame, mediaSample]() mutable {
            newFrame = nullptr;
            mediaSample->Release();
        });

        auto fakeFrame = std::shared_ptr<Frame>(manager, newFrame.get());
        newFrame = fakeFrame;
    }
    else
    {
        if (newFrame->sizeInBytes != bufferLen)
        {
            newFrame->allocator->resize(bufferLen);
            newFrame->sizeInBytes = bufferLen;
        }

        newFrame->data[0] = newFrame->allocator->data();
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;
        newFrame->stride[0] = m_frameProp.width * 3; // BGR888 每个像素 3 字节
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;
        newFrame->pixelFormat = PixelFormat::BGRA8888; // 假设 RGB32 格式
        newFrame->width = m_frameProp.width;
        newFrame->height = m_frameProp.height;
        newFrame->timestamp = static_cast<uint64_t>(sampleTime * 1e9);
    }

    newFrame->frameIndex = m_frameIndex++;

    if (ccap::verboseLogEnabled())
    {
        static uint64_t _lastFrameTime = 0;
        static int _frameCounter{};
        static double _duration{};
        static double _fps{ 0.0 };

        if (_lastFrameTime != 0)
        {
            _duration += (newFrame->timestamp - _lastFrameTime) / 1.e9;
        }
        _lastFrameTime = newFrame->timestamp;
        ++_frameCounter;

        if (_duration > 0.5 || _frameCounter >= 30)
        {
            auto newFps = _frameCounter / _duration;
            constexpr double alpha = 0.8; // Smoothing factor, smaller value means smoother
            _fps = alpha * newFps + (1.0 - alpha) * _fps;
            _frameCounter = 0;
            _duration = 0;
        }

        double roundedFps = std::round(_fps * 10.0) / 10.0;
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