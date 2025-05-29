/**
 * @file ccap_imp_msmf.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for ProviderMSMF class using Media Foundation.
 * @date 2025-05
 *
 */

#pragma once
#ifndef CAMERA_CAPTURE_MSMF_H
#define CAMERA_CAPTURE_MSMF_H

#if defined(_WIN32) || defined(_MSC_VER)

#include "ccap_imp.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfcaptureengine.h>
#include <mferror.h>
#include <shlwapi.h>
#include <wmcodecdsp.h>

// Link necessary libraries
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")

namespace ccap
{

class ProviderMSMF : public ProviderImp, public IMFSourceReaderCallback
{
public:
    ProviderMSMF();
    ~ProviderMSMF() override;

    // ProviderImp interface implementation
    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    std::optional<DeviceInfo> getDeviceInfo() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

    // IMFSourceReaderCallback interface implementation
    HRESULT STDMETHODCALLTYPE OnReadSample(
        HRESULT hrStatus,
        DWORD dwStreamIndex,
        DWORD dwStreamFlags,
        LONGLONG llTimestamp,
        IMFSample* pSample) override;

    HRESULT STDMETHODCALLTYPE OnFlush(DWORD dwStreamIndex) override;

    HRESULT STDMETHODCALLTYPE OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent) override;

    // IUnknown interface implementation
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef(void) override;
    ULONG STDMETHODCALLTYPE Release(void) override;

private:
    // Initialize Media Foundation
    bool setup();

    // Enumerate available capture devices
    void enumerateDevices(std::function<bool(IMFActivate* activate, std::string_view)> callback);

    // Helper functions
    bool createSourceReader(IMFActivate* activate);
    bool configureSourceReader();
    PixelFormat getPixelFormatFromMediaType(IMFMediaType* mediaType);
    GUID getMediaSubtypeFromPixelFormat(PixelFormat pixelFormat);
    std::string wstringToString(const std::wstring& wstr);
    std::wstring stringToWstring(const std::string& str);

    // Device enumeration and management
    struct DeviceInfo_Internal
    {
        IMFActivate* activate = nullptr;
        std::string displayName;
        std::string symbolicLink;
    };

    // Media Foundation objects
    IMFSourceReader* m_sourceReader = nullptr;
    IMFActivate** m_devices = nullptr;
    UINT32 m_deviceCount = 0;
    std::vector<DeviceInfo_Internal> m_allDevices;
    std::string m_deviceName;

    // State variables
    bool m_didSetup = false;
    bool m_isOpened = false;
    bool m_isRunning = false;
    bool m_firstFrameArrived = false;

    // Threading and synchronization
    std::mutex m_callbackMutex;
    std::chrono::steady_clock::time_point m_startTime;

    // Reference counting for COM
    ULONG m_refCount;
};

ProviderImp* createProviderMSMF();

} // namespace ccap

#endif // defined(_WIN32) || defined(_MSC_VER)

#endif // CAMERA_CAPTURE_MSMF_H
