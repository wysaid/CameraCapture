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

// 需要链接以下库
#pragma comment(lib, "strmiids.lib")

namespace ccap
{
ProviderWin::ProviderWin()
{
}

ProviderWin::~ProviderWin()
{
    ProviderWin::close();
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

    // 初始化 COM
    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoInitializeEx failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    // 创建 Filter Graph
    hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_graph);
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: CoCreateInstance CLSID_FilterGraph failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    // 创建 Capture Graph Builder
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

    // 枚举视频采集设备
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

    // 添加设备 Filter 到 Graph
    hr = m_graph->AddFilter(m_deviceFilter, L"Video Capture");
    if (FAILED(hr))
    {
        if (ccap::errorLogEnabled())
        {
            std::cerr << "ccap: AddFilter Video Capture failed, hr=0x" << std::hex << hr << std::endl;
        }
        return false;
    }

    // 创建 SampleGrabber
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

    // 连接 Filter
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
        newFrame->pixelFormat = PixelFormat::RGB888; // 假设 RGB24 格式
        newFrame->width = m_frameProp.width;
        newFrame->height = m_frameProp.height;
        newFrame->stride[0] = m_frameProp.width * 3; // RGB24 每个像素 3 字节
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
        newFrame->stride[0] = m_frameProp.width * 3; // RGB24 每个像素 3 字节
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;
        newFrame->pixelFormat = PixelFormat::RGB888; // 假设 RGB24 格式
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
        printf("ccap: New frame available: %lux%lu, bytes %lu, Data address: %p, fps: %g", newFrame->width, newFrame->height, newFrame->sizeInBytes, newFrame->data[0], roundedFps);
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