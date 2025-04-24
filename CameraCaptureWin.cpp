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
    // if (m_isOpened)
    // {
    //     close();
    // }

    // HRESULT hr = S_OK;

    // // 获取设备
    // IMFAttributes* pAttributes = nullptr;
    // hr = MFCreateAttributes(&pAttributes, 1);
    // if (FAILED(hr))
    // {
    //     return false;
    // }

    // hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    // if (FAILED(hr))
    // {
    //     pAttributes->Release();
    //     return false;
    // }

    // IMFActivate** devices = nullptr;
    // UINT32 deviceCount = 0;

    // hr = MFEnumDeviceSources(pAttributes, &devices, &deviceCount);
    // pAttributes->Release();

    // if (FAILED(hr) || deviceCount == 0)
    // {
    //     return false;
    // }

    // // 找到指定的设备或使用第一个可用设备
    // int deviceIndex = 0;
    // if (!deviceName.empty())
    // {
    //     // 尝试查找指定名称的设备
    //     for (UINT32 i = 0; i < deviceCount; i++)
    //     {
    //         WCHAR friendlyName[256];
    //         UINT32 nameLength = 0;

    //         hr = devices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, friendlyName, 256, &nameLength);
    //         if (SUCCEEDED(hr))
    //         {
    //             char narrowName[256];
    //             WideCharToMultiByte(CP_UTF8, 0, friendlyName, -1, narrowName, 256, nullptr, nullptr);

    //             if (deviceName == narrowName)
    //             {
    //                 deviceIndex = i;
    //                 break;
    //             }
    //         }
    //     }
    // }

    // // 创建媒体源
    // IMFMediaSource* pSource = nullptr;
    // hr = devices[deviceIndex]->ActivateObject(IID_PPV_ARGS(&pSource));

    // // 释放不需要的设备
    // for (UINT32 i = 0; i < deviceCount; i++)
    // {
    //     devices[i]->Release();
    // }
    // CoTaskMemFree(devices);

    // if (FAILED(hr))
    // {
    //     return false;
    // }

    // // 创建源读取器
    // hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &m_reader);
    // pSource->Release();

    // if (FAILED(hr))
    // {
    //     return false;
    // }

    // // 配置媒体类型
    // IMFMediaType* pMediaType = nullptr;
    // hr = MFCreateMediaType(&pMediaType);
    // if (FAILED(hr))
    // {
    //     m_reader->Release();
    //     m_reader = nullptr;
    //     return false;
    // }

    // hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    // hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);

    // hr = MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, m_width, m_height);
    // hr = MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, static_cast<UINT32>(m_fps * 1000), 1000);

    // hr = m_reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pMediaType);
    // pMediaType->Release();

    // if (FAILED(hr))
    // {
    //     m_reader->Release();
    //     m_reader = nullptr;
    //     return false;
    // }

    // m_isOpened = true;
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

void ProviderWin::stop()
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

    auto frame = getFreeFrame();

    // 复制数据
    {
    }

    // 更新时间戳
    LONGLONG timestamp = 0;
    sample->GetSampleTime(&timestamp);
    frame->timestamp = timestamp; // 转换为秒

    buffer->Unlock();
    buffer->Release();

    return true;
}

} // namespace ccap
#endif