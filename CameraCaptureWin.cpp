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
    if (m_isOpened)
    {
        close();
    }

    HRESULT hr = S_OK;

    // 初始化 COM
    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return false;

    // 创建 Filter Graph
    hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_graph);
    if (FAILED(hr)) return false;

    // 创建 Capture Graph Builder
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&m_captureBuilder);
    if (FAILED(hr)) return false;
    m_captureBuilder->SetFiltergraph(m_graph);

    // 枚举视频采集设备
    ICreateDevEnum* pDevEnum = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pDevEnum);
    if (FAILED(hr)) return false;

    IEnumMoniker* pEnum = nullptr;
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    pDevEnum->Release();
    if (FAILED(hr) || !pEnum) return false;

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
    if (!found) return false;

    // 添加设备 Filter 到 Graph
    hr = m_graph->AddFilter(m_deviceFilter, L"Video Capture");
    if (FAILED(hr)) return false;

    // 创建 SampleGrabber
    hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_sampleGrabberFilter);
    if (FAILED(hr)) return false;
    hr = m_sampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&m_sampleGrabber);
    if (FAILED(hr)) return false;

    // 设置 SampleGrabber 媒体类型
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(mt));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    mt.formattype = FORMAT_VideoInfo;
    hr = m_sampleGrabber->SetMediaType(&mt);
    if (FAILED(hr)) return false;

    // 添加 SampleGrabber 到 Graph
    hr = m_graph->AddFilter(m_sampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr)) return false;

    // 连接 Filter
    hr = m_captureBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_deviceFilter, m_sampleGrabberFilter, nullptr);
    if (FAILED(hr)) return false;

    // 设置 SampleGrabber 回调
    m_sampleGrabber->SetBufferSamples(TRUE);
    m_sampleGrabber->SetOneShot(FALSE);
    m_sampleGrabber->SetCallback(this, 0); // 0 = SampleCB

    // 获取 IMediaControl
    hr = m_graph->QueryInterface(IID_IMediaControl, (void**)&m_mediaControl);
    if (FAILED(hr)) return false;

    m_isOpened = true;
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
        return S_OK;

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
    if (m_isRunning)
    {
        stop();
    }

    m_isOpened = false;
}

bool ProviderWin::start()
{
    if (!m_isOpened || m_isRunning)
    {
        return false;
    }

    m_stopRequested = false;
    m_isRunning = true;

    return true;
}

void ProviderWin::stop()
{
    if (m_isRunning)
    {
        m_isRunning = false;
    }
}

bool ProviderWin::isStarted() const
{
    return m_isRunning;
}

bool ProviderWin::processFrame(IMediaSample* sample)
{
    return true;
}

} // namespace ccap
#endif