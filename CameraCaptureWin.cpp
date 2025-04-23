/**
 * @file CameraCaptureWin.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Implementation for ProviderWin class using MSMF.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureWin.h"

#include <dshow.h>
#include <iostream>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>

// 需要链接以下库
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "strmiids.lib")

namespace ccap
{
ProviderWin::ProviderWin()
{
    // 初始化Media Foundation
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
}

ProviderWin::~ProviderWin()
{
    ProviderWin::close();
    MFShutdown();
    CoUninitialize();
}

bool ProviderWin::open(std::string_view deviceName)
{
    if (m_isOpened)
    {
        close();
    }

    HRESULT hr = S_OK;

    // 获取设备
    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr))
    {
        return false;
    }

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr))
    {
        pAttributes->Release();
        return false;
    }

    IMFActivate** devices = nullptr;
    UINT32 deviceCount = 0;

    hr = MFEnumDeviceSources(pAttributes, &devices, &deviceCount);
    pAttributes->Release();

    if (FAILED(hr) || deviceCount == 0)
    {
        return false;
    }

    // 找到指定的设备或使用第一个可用设备
    int deviceIndex = 0;
    if (!deviceName.empty())
    {
        // 尝试查找指定名称的设备
        for (UINT32 i = 0; i < deviceCount; i++)
        {
            WCHAR friendlyName[256];
            UINT32 nameLength = 0;

            hr = devices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, friendlyName, 256, &nameLength);
            if (SUCCEEDED(hr))
            {
                char narrowName[256];
                WideCharToMultiByte(CP_UTF8, 0, friendlyName, -1, narrowName, 256, nullptr, nullptr);

                if (deviceName == narrowName)
                {
                    deviceIndex = i;
                    break;
                }
            }
        }
    }

    // 创建媒体源
    IMFMediaSource* pSource = nullptr;
    hr = devices[deviceIndex]->ActivateObject(IID_PPV_ARGS(&pSource));

    // 释放不需要的设备
    for (UINT32 i = 0; i < deviceCount; i++)
    {
        devices[i]->Release();
    }
    CoTaskMemFree(devices);

    if (FAILED(hr))
    {
        return false;
    }

    // 创建源读取器
    hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &m_reader);
    pSource->Release();

    if (FAILED(hr))
    {
        return false;
    }

    // 配置媒体类型
    IMFMediaType* pMediaType = nullptr;
    hr = MFCreateMediaType(&pMediaType);
    if (FAILED(hr))
    {
        m_reader->Release();
        m_reader = nullptr;
        return false;
    }

    hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);

    hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, m_width, m_height);
    hr = MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, static_cast<UINT32>(m_fps * 1000), 1000);

    hr = m_reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pMediaType);
    pMediaType->Release();

    if (FAILED(hr))
    {
        m_reader->Release();
        m_reader = nullptr;
        return false;
    }

    m_isOpened = true;
    return true;
}

bool ProviderWin::isOpened() const
{
    return m_isOpened;
}

void ProviderWin::close()
{
    if (m_isRunning)
    {
        // 停止采集线程
        m_stopRequested = true;
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
        m_isRunning = false;
    }

    if (m_reader)
    {
        m_reader->Release();
        m_reader = nullptr;
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
    m_captureThread = std::thread(&ProviderWin::captureThread, this);

    return true;
}

void ProviderWin::pause()
{
    if (m_isRunning)
    {
        m_stopRequested = true;
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
        m_isRunning = false;
    }
}

bool ProviderWin::isStarted() const
{
    return m_isRunning;
}

bool ProviderWin::set(Property prop, double value)
{
    if (!m_isOpened || !m_reader)
    {
        return false;
    }

    HRESULT hr = S_OK;
    bool needRestart = false;
    bool wasRunning = m_isRunning;

    // 记录原始值，以便在设置失败时恢复
    int originalWidth = m_width;
    int originalHeight = m_height;
    double originalFps = m_fps;

    switch (prop)
    {
    case Property::Width:
        m_width = static_cast<int>(value);
        needRestart = true;
        break;

    case Property::Height:
        m_height = static_cast<int>(value);
        needRestart = true;
        break;

    case Property::FrameRate:
        m_fps = value;
        needRestart = true;
        break;

    case Property::PixelFormat:
        // 这个示例只支持RGB24格式，如需其他格式可扩展
        return false;

    default:
        return false;
    }

    if (needRestart && wasRunning)
    {
        pause(); // 暂停捕获
    }

    // 如果需要重新配置媒体类型
    if (needRestart)
    {
        IMFMediaType* pMediaType = nullptr;
        hr = MFCreateMediaType(&pMediaType);
        if (FAILED(hr))
        {
            // 恢复原始值
            m_width = originalWidth;
            m_height = originalHeight;
            m_fps = originalFps;
            if (wasRunning)
                start();
            return false;
        }

        hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);

        hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, m_width, m_height);
        hr = MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, static_cast<UINT32>(m_fps * 1000), 1000);

        hr = m_reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pMediaType);
        pMediaType->Release();

        if (FAILED(hr))
        {
            // 恢复原始值
            m_width = originalWidth;
            m_height = originalHeight;
            m_fps = originalFps;
            if (wasRunning)
                start();
            return false;
        }
    }

    // 如果之前在运行，则重新启动
    if (needRestart && wasRunning)
    {
        return start();
    }

    return true;
}

double ProviderWin::get(Property prop)
{
    if (!m_isOpened || !m_reader)
    {
        return 0.0;
    }

    switch (prop)
    {
    case Property::Width:
        return static_cast<double>(m_width);

    case Property::Height:
        return static_cast<double>(m_height);

    case Property::FrameRate:
        return m_fps;

    case Property::PixelFormat:
        // 始终返回RGB888格式，对应CameraCapture.h中的PixelFormat枚举
        return static_cast<double>(PixelFormat::RGB888);

    default:
        // 对于不支持的属性，返回0
        return NAN;
    }
}

std::shared_ptr<Frame> ProviderWin::grab(bool waitNewFrame)
{
    std::unique_lock<std::mutex> lock(m_frameMutex);

    if (waitNewFrame && !m_newFrameAvailable)
    {
        // 等待新帧
        m_frameCondition.wait(lock, [this]() { return m_newFrameAvailable || !m_isRunning; });
    }

    m_newFrameAvailable = false;
    return m_latestFrame;
}

void ProviderWin::setNewFrameCallback(std::function<bool(std::shared_ptr<Frame>)> callback)
{
    m_callback = std::make_shared<std::function<bool(std::shared_ptr<Frame>)>>(std::move(callback));
}

void ProviderWin::setFrameAllocator(std::shared_ptr<Allocator> allocator)
{
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_allocator = std::move(allocator);
    m_framePool.clear();
}

void ProviderWin::captureThread()
{
    while (!m_stopRequested)
    {
        if (!m_isOpened || !m_reader)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 读取下一帧
        DWORD streamIndex, flags;
        LONGLONG timestamp;
        IMFSample* pSample = nullptr;

        HRESULT hr = m_reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &pSample);

        if (SUCCEEDED(hr) && pSample)
        {
            processFrame(pSample);
            pSample->Release();
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

std::shared_ptr<Frame> ProviderWin::getFreeFrame()
{
    std::lock_guard<std::mutex> lock(m_frameMutex);
    std::shared_ptr<Frame> frame;
    if (!m_framePool.empty())
    {
        auto ret = std::find_if(m_framePool.begin(), m_framePool.end(), [](const std::shared_ptr<Frame>& frame) {
            return frame.use_count() == 1;
        });

        if (ret != m_framePool.end())
        {
            frame = std::move(*ret);
            m_framePool.erase(ret);
        }
        else
        {
            if (m_framePool.size() >= MAX_CACHE_FRAME_SIZE)
            {
#ifdef DEBUG
                std::cout << "Frame pool is full, new frame allocated..." << std::endl;
#endif
                m_framePool.erase(m_framePool.end());
            }
        }
    }

    if (!frame)
    {
        frame = std::make_shared<Frame>();
        frame->allocator = m_allocator;
        m_framePool.push_back(frame);
    }
    return frame;
}

bool ProviderWin::processFrame(IMFSample* sample)
{
    if (!sample)
        return false;

    IMFMediaBuffer* buffer = nullptr;
    HRESULT hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr))
        return false;

    BYTE* data = nullptr;
    DWORD maxLength = 0, currentLength = 0;

    hr = buffer->Lock(&data, &maxLength, &currentLength);
    if (FAILED(hr))
    {
        buffer->Release();
        return false;
    }

    if (!m_allocator)
    {
        m_allocator = std::make_shared<DefaultAllocator>();
    }

    // 创建帧对象
    std::shared_ptr<Frame> frame = getFreeFrame();

    frame->width = m_width;
    frame->height = m_height;
    frame->pixelFormat = m_pixelFormat;
    frame->frameIndex = m_frameIndex++;

    if (m_pixelFormat & PixelFormat::YUVColorBit)
    {
        auto bytes = m_width * m_height * 3 / 2;
        frame->sizeInBytes = bytes;
        frame->allocator->resize(bytes);
        frame->data[0] = frame->allocator->data();
        frame->data[1] = frame->data[0] + m_width * m_height;
        if (m_pixelFormat == PixelFormat::YUV420P)
        {
            frame->data[2] = frame->data[1] + m_width * m_height / 4;
        }
        else
        {
            frame->data[2] = nullptr;
        }
    }
    else
    {
        // RGB24 格式
        int channels = m_pixelFormat & PixelFormat::RGBColorBit ? 3 : 4;
        auto bytes = m_width * m_height * channels;
        frame->sizeInBytes = bytes;
        frame->allocator->resize(bytes);
        frame->data[0] = frame->allocator->data();
        frame->data[1] = nullptr;
        frame->data[2] = nullptr;
    }

    // 复制数据
    {
    }

    // 更新时间戳
    LONGLONG timestamp = 0;
    sample->GetSampleTime(&timestamp);
    frame->timestamp = timestamp; // 转换为秒

    buffer->Unlock();
    buffer->Release();

    // 处理新帧
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_latestFrame = frame;
        m_newFrameAvailable = true;
    }

    // 通知等待线程
    m_frameCondition.notify_all();

    // 防止调用回调过程中回调被删除.
    if (auto callback = m_callback; callback && *callback)
    {
        (*callback)(frame);
    }

    return true;
}

} // namespace ccap
#endif