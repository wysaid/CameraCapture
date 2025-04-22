/**
 * @file CameraCaptureWin.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include <dshow.h>
#include <iostream>
#include <vector>
#include <windows.h>

// 由于 qedit.h 在较新版本的 Windows SDK 中已被移除，手动定义所需接口
#ifdef __cplusplus
extern "C"
{
#endif

// ISampleGrabber 接口的 GUID
static const IID IID_ISampleGrabber = { 0x6B652FFF, 0x11FE, 0x4fce, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };

// CLSID_SampleGrabber
static const CLSID CLSID_SampleGrabber = { 0xC1F400A0, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

// ISampleGrabber 接口声明
MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL OneShot) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE* pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE * pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL BufferThem) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long* pBufferSize, long* pBuffer) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample * *ppSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(IUnknown * pCallback, long WhichMethod) = 0;
};

#ifdef __cplusplus
}
#endif

// 需要在项目设置中添加 strmiids.lib 和 quartz.lib
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "quartz.lib")

class OBSVirtualCamera
{
private:
    IGraphBuilder* pGraph = nullptr;
    ICaptureGraphBuilder2* pBuilder = nullptr;
    IBaseFilter* pCaptureFilter = nullptr;
    IMediaControl* pMediaControl = nullptr;
    ISampleGrabber* pGrabber = nullptr;
    IBaseFilter* pGrabberFilter = nullptr;
    IMediaEventEx* pEvent = nullptr;

    UINT32 m_width = 0;
    UINT32 m_height = 0;
    std::vector<BYTE> m_buffer;
    bool m_initialized = false;

public:
    OBSVirtualCamera() {}

    ~OBSVirtualCamera()
    {
        Close();
    }

    // 列出所有可用的摄像头
    std::vector<std::wstring> ListAvailableCameras()
    {
        std::vector<std::wstring> cameras;

        ICreateDevEnum* pDevEnum = nullptr;
        IEnumMoniker* pEnum = nullptr;

        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) return cameras;

        hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                              IID_ICreateDevEnum, (void**)&pDevEnum);
        if (FAILED(hr))
        {
            CoUninitialize();
            return cameras;
        }

        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (FAILED(hr) || pEnum == NULL)
        {
            pDevEnum->Release();
            CoUninitialize();
            return cameras;
        }

        IMoniker* pMoniker = NULL;
        ULONG fetched;

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
                    cameras.push_back(varName.bstrVal);
                    VariantClear(&varName);
                }
                pPropBag->Release();
            }
            pMoniker->Release();
        }

        pEnum->Release();
        pDevEnum->Release();
        CoUninitialize();

        return cameras;
    }

    bool Initialize(const wchar_t* cameraName = nullptr)
    {
        // 常用的OBS虚拟摄像头名称
        const wchar_t* cameraNames[] = {
            L"OBS Virtual Camera",
            L"OBS Camera",
            L"OBS-Camera",
            L"OBS Cam",
            L"OBS-Cam",
            L"OBS"
        };

        if (cameraName != nullptr)
        {
            // 如果提供了特定名称，尝试使用
            return InitializeWithName(cameraName);
        }

        // 否则尝试所有可能的名称
        for (const wchar_t* name : cameraNames)
        {
            std::wcout << L"尝试连接摄像头: " << name << std::endl;
            if (InitializeWithName(name))
            {
                std::wcout << L"成功连接到: " << name << std::endl;
                return true;
            }
        }

        std::cerr << "未找到任何OBS虚拟摄像头" << std::endl;
        return false;
    }

    bool InitializeWithName(const wchar_t* cameraName)
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr))
        {
            std::cerr << "COM初始化失败: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }

        // 创建过滤器图表管理器和捕获图表构建器
        hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                              IID_IGraphBuilder, (void**)&pGraph);
        if (FAILED(hr))
        {
            std::cerr << "创建FilterGraph失败: 0x" << std::hex << hr << std::dec << std::endl;
            CoUninitialize();
            return false;
        }

        hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER,
                              IID_ICaptureGraphBuilder2, (void**)&pBuilder);
        if (FAILED(hr))
        {
            std::cerr << "创建CaptureGraphBuilder2失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        hr = pBuilder->SetFiltergraph(pGraph);
        if (FAILED(hr))
        {
            std::cerr << "设置过滤器图表失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        // 创建SampleGrabber过滤器
        hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
                              IID_IBaseFilter, (void**)&pGrabberFilter);
        if (FAILED(hr))
        {
            std::cerr << "创建SampleGrabber过滤器失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        hr = pGraph->AddFilter(pGrabberFilter, L"Sample Grabber");
        if (FAILED(hr))
        {
            std::cerr << "添加SampleGrabber过滤器失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        hr = pGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&pGrabber);
        if (FAILED(hr))
        {
            std::cerr << "获取ISampleGrabber接口失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        // 设置媒体类型
        AM_MEDIA_TYPE mt;
        ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = MEDIASUBTYPE_RGB24;
        mt.formattype = FORMAT_VideoInfo; // 添加格式类型
        hr = pGrabber->SetMediaType(&mt);
        if (FAILED(hr))
        {
            std::cerr << "设置媒体类型失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        // 枚举视频捕获设备
        ICreateDevEnum* pDevEnum = nullptr;
        IEnumMoniker* pEnum = nullptr;

        hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                              IID_ICreateDevEnum, (void**)&pDevEnum);
        if (FAILED(hr))
        {
            std::cerr << "创建设备枚举器失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (FAILED(hr) || pEnum == NULL)
        {
            std::cerr << "创建视频设备类枚举器失败: 0x" << std::hex << hr << std::dec << std::endl;
            pDevEnum->Release();
            Close();
            return false;
        }

        // 寻找OBS虚拟相机
        IMoniker* pMoniker = NULL;
        bool found = false;
        ULONG fetched;

        while (pEnum->Next(1, &pMoniker, &fetched) == S_OK && !found)
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
                    std::wcout << L"发现摄像头: " << varName.bstrVal << std::endl;

                    if (wcsstr(varName.bstrVal, cameraName) != NULL)
                    {
                        // 找到了OBS虚拟相机
                        hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCaptureFilter);
                        if (SUCCEEDED(hr))
                        {
                            found = true;
                            std::wcout << L"成功绑定到摄像头: " << varName.bstrVal << std::endl;
                        }
                        else
                        {
                            std::cerr << "绑定到摄像头对象失败: 0x" << std::hex << hr << std::dec << std::endl;
                        }
                    }
                    VariantClear(&varName);
                }
                pPropBag->Release();
            }

            if (!found)
            {
                pMoniker->Release();
            }
        }

        pEnum->Release();
        pDevEnum->Release();

        if (!found || !pCaptureFilter)
        {
            std::cerr << "未找到指定的摄像头: " << cameraName << std::endl;
            Close();
            return false;
        }

        // 添加捕获过滤器到图表
        hr = pGraph->AddFilter(pCaptureFilter, L"Video Capture");
        if (FAILED(hr))
        {
            std::cerr << "添加捕获过滤器失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        // 连接捕获过滤器到SampleGrabber
        hr = pBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                    pCaptureFilter, pGrabberFilter, NULL);
        if (FAILED(hr))
        {
            std::cerr << "连接捕获过滤器到SampleGrabber失败: 0x" << std::hex << hr << std::dec << std::endl;
            // 尝试使用不同的连接方法
            std::cout << "尝试使用备用连接方法..." << std::endl;

            // 直接连接输出引脚到输入引脚
            IPin* pOut = nullptr;
            IPin* pIn = nullptr;

            // 查找捕获过滤器的输出引脚
            hr = FindPin(pCaptureFilter, PINDIR_OUTPUT, &pOut);
            if (FAILED(hr) || !pOut)
            {
                std::cerr << "找不到捕获过滤器的输出引脚: 0x" << std::hex << hr << std::dec << std::endl;
                Close();
                return false;
            }

            // 查找SampleGrabber的输入引脚
            hr = FindPin(pGrabberFilter, PINDIR_INPUT, &pIn);
            if (FAILED(hr) || !pIn)
            {
                pOut->Release();
                std::cerr << "找不到SampleGrabber的输入引脚: 0x" << std::hex << hr << std::dec << std::endl;
                Close();
                return false;
            }

            // 连接引脚
            hr = pGraph->Connect(pOut, pIn);
            pOut->Release();
            pIn->Release();

            if (FAILED(hr))
            {
                std::cerr << "手动连接引脚失败: 0x" << std::hex << hr << std::dec << std::endl;
                Close();
                return false;
            }

            std::cout << "手动连接成功!" << std::endl;
        }

        // 获取媒体控制接口
        hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pMediaControl);
        if (FAILED(hr))
        {
            std::cerr << "获取媒体控制接口失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        // 获取媒体事件接口
        hr = pGraph->QueryInterface(IID_IMediaEventEx, (void**)&pEvent);
        if (FAILED(hr))
        {
            std::cerr << "获取媒体事件接口失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        // 获取媒体类型以确定宽度和高度
        AM_MEDIA_TYPE grabberMediaType;
        ZeroMemory(&grabberMediaType, sizeof(AM_MEDIA_TYPE));

        hr = pGrabber->GetConnectedMediaType(&grabberMediaType);
        if (SUCCEEDED(hr) && grabberMediaType.formattype == FORMAT_VideoInfo)
        {
            VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)grabberMediaType.pbFormat;
            m_width = pVih->bmiHeader.biWidth;
            m_height = pVih->bmiHeader.biHeight;
            m_buffer.resize(m_width * m_height * 3); // RGB24每像素3字节

            std::cout << "获取到视频分辨率: " << m_width << "x" << m_height << std::endl;
        }
        else
        {
            std::cerr << "获取媒体类型失败或格式不是VideoInfo: 0x" << std::hex << hr << std::dec << std::endl;
            if (SUCCEEDED(hr))
            {
                std::cout << "媒体格式类型: " << grabberMediaType.formattype.Data1 << std::endl;
            }

            // 使用默认值
            m_width = 640;
            m_height = 480;
            m_buffer.resize(m_width * m_height * 3);
            std::cout << "使用默认分辨率: 640x480" << std::endl;
        }

        if (grabberMediaType.pbFormat)
        {
            CoTaskMemFree(grabberMediaType.pbFormat);
        }
        if (grabberMediaType.pUnk)
        {
            grabberMediaType.pUnk->Release();
        }

        // 配置SampleGrabber回调
        hr = pGrabber->SetBufferSamples(TRUE);
        if (FAILED(hr))
        {
            std::cerr << "设置缓冲区采样失败: 0x" << std::hex << hr << std::dec << std::endl;
        }

        hr = pGrabber->SetOneShot(FALSE);
        if (FAILED(hr))
        {
            std::cerr << "设置OneShot失败: 0x" << std::hex << hr << std::dec << std::endl;
        }

        // 运行图表
        hr = pMediaControl->Run();
        if (FAILED(hr))
        {
            std::cerr << "运行媒体控制失败: 0x" << std::hex << hr << std::dec << std::endl;
            Close();
            return false;
        }

        // 等待图表启动
        std::cout << "等待摄像头启动..." << std::endl;
        Sleep(1000);

        m_initialized = true;
        return true;
    }

    // 辅助函数：查找过滤器引脚
    HRESULT FindPin(IBaseFilter* pFilter, PIN_DIRECTION direction, IPin** ppPin)
    {
        IEnumPins* pEnum = nullptr;
        IPin* pPin = nullptr;
        HRESULT hr = pFilter->EnumPins(&pEnum);

        if (FAILED(hr) || !pEnum)
        {
            return hr;
        }

        while (pEnum->Next(1, &pPin, 0) == S_OK)
        {
            PIN_DIRECTION pinDir;
            hr = pPin->QueryDirection(&pinDir);

            if (SUCCEEDED(hr) && pinDir == direction)
            {
                *ppPin = pPin;
                pEnum->Release();
                return S_OK;
            }

            pPin->Release();
        }

        pEnum->Release();
        return E_FAIL;
    }

    bool GetFrame(std::vector<BYTE>& frameData)
    {
        if (!m_initialized || !pGrabber)
        {
            std::cerr << "错误：相机未初始化或pGrabber为空" << std::endl;
            return false;
        }

        // 等待新的样本
        std::cout << "等待新样本..." << std::endl;
        Sleep(500); // 增加等待时间到500ms

        long bufferSize = 0;
        HRESULT hr = pGrabber->GetCurrentBuffer(&bufferSize, NULL);
        if (FAILED(hr))
        {
            std::cerr << "错误：获取缓冲区大小失败，HRESULT: 0x" << std::hex << hr << std::dec << std::endl;

            // 尝试用其他Media类型
            if (hr == VFW_E_WRONG_STATE)
            {
                std::cerr << "图形可能尚未完全初始化，尝试等待更长时间..." << std::endl;
                Sleep(2000);
                hr = pGrabber->GetCurrentBuffer(&bufferSize, NULL);
                if (FAILED(hr))
                {
                    std::cerr << "重试后仍然失败: 0x" << std::hex << hr << std::dec << std::endl;
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        if (bufferSize <= 0)
        {
            std::cerr << "错误：缓冲区大小为0或负值" << std::endl;
            return false;
        }

        std::cout << "缓冲区大小: " << bufferSize << " 字节" << std::endl;

        // 确保缓冲区足够大
        if (m_buffer.size() < static_cast<size_t>(bufferSize))
        {
            m_buffer.resize(bufferSize);
            std::cout << "调整缓冲区大小为: " << bufferSize << " 字节" << std::endl;
        }

        // 获取当前帧数据
        hr = pGrabber->GetCurrentBuffer(&bufferSize, (long*)m_buffer.data());
        if (FAILED(hr))
        {
            std::cerr << "错误：获取帧数据失败，HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }

        // 复制数据到输出缓冲区
        frameData.resize(bufferSize);
        memcpy(frameData.data(), m_buffer.data(), bufferSize);

        std::cout << "成功获取帧，宽度: " << m_width << "，高度: " << m_height
                  << "，数据大小: " << bufferSize << " 字节" << std::endl;

        return true;
    }

    UINT32 GetWidth() const { return m_width; }
    UINT32 GetHeight() const { return m_height; }

    void Close()
    {
        if (pMediaControl)
        {
            pMediaControl->Stop();
            pMediaControl->Release();
            pMediaControl = nullptr;
        }

        if (pEvent)
        {
            pEvent->Release();
            pEvent = nullptr;
        }

        if (pGrabber)
        {
            pGrabber->Release();
            pGrabber = nullptr;
        }

        if (pGrabberFilter)
        {
            pGrabberFilter->Release();
            pGrabberFilter = nullptr;
        }

        if (pCaptureFilter)
        {
            pCaptureFilter->Release();
            pCaptureFilter = nullptr;
        }

        if (pBuilder)
        {
            pBuilder->Release();
            pBuilder = nullptr;
        }

        if (pGraph)
        {
            pGraph->Release();
            pGraph = nullptr;
        }

        CoUninitialize();
        m_initialized = false;
    }
};

#endif