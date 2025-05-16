/**
 * @file CameraCaptureDirectShow.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for ProviderDirectShow class.
 * @date 2025-04
 *
 */

#pragma once
#ifndef CAMERA_CAPTURE_DSHOW_H
#define CAMERA_CAPTURE_DSHOW_H

#if defined(_WIN32) || defined(_MSC_VER)

#include "ccap_imp.h"

#include <atomic>
#include <deque>
#include <dshow.h>
#include <mutex>
#ifdef _MSC_VER
#pragma include_alias("dxtrans.h", "qedit.h")
#endif
#define __IDxtCompositor_INTERFACE_DEFINED__
#define __IDxtAlphaSetter_INTERFACE_DEFINED__
#define __IDxtJpeg_INTERFACE_DEFINED__
#define __IDxtKey_INTERFACE_DEFINED__
#include <aviriff.h>
#include <windows.h>

// Due to a missing qedit.h in recent Platform SDKs, we've replicated the relevant contents here
// #include <qedit.h>
MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SampleCB(
        double SampleTime,
        IMediaSample* pSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE BufferCB(
        double SampleTime,
        BYTE* pBuffer,
        long BufferLen) = 0;
};

MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(
        BOOL OneShot) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetMediaType(
        const AM_MEDIA_TYPE* pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(
        AM_MEDIA_TYPE * pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
        BOOL BufferThem) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
        /* [out][in] */ long* pBufferSize,
        /* [out] */ long* pBuffer) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
        /* [retval][out] */ IMediaSample * *ppSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetCallback(
        ISampleGrabberCB * pCallback,
        long WhichMethodToCallback) = 0;
};
EXTERN_C const CLSID CLSID_SampleGrabber;
EXTERN_C const IID IID_ISampleGrabber;
EXTERN_C const CLSID CLSID_NullRenderer;

namespace ccap
{
class ProviderDirectShow : public ProviderImp, public ISampleGrabberCB
{
public:
    ProviderDirectShow();
    ~ProviderDirectShow() override;
    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    std::optional<DeviceInfo> getDeviceInfo() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

    HRESULT STDMETHODCALLTYPE SampleCB(double SampleTime, IMediaSample* pSample) override;
    HRESULT STDMETHODCALLTYPE BufferCB(double SampleTime, BYTE* pBuffer, long BufferLen) override;

private:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef(void) override;
    ULONG STDMETHODCALLTYPE Release(void) override;

    // Initialize com, call this before all operations
    bool setup();

    // Available at anytime, automatically calls setup.
    void enumerateDevices(std::function<bool(IMoniker* moniker, std::string_view)> callback);

    // During open phase, buildGraph, createStream and setGrabberOutputSubtype are executed in sequence.
    // If any of these fails, the open operation will fail.
    bool buildGraph();
    bool createStream();
    bool setGrabberOutputSubtype(GUID subtype);

    struct MediaInfo
    {
        ~MediaInfo();
        IAMStreamConfig* streamConfig = nullptr;
        std::vector<AM_MEDIA_TYPE*> mediaTypes;
        std::vector<AM_MEDIA_TYPE*> videoMediaTypes;
    };

    std::unique_ptr<MediaInfo> enumerateMediaInfo(std::function<bool(AM_MEDIA_TYPE* mediaType, const char* name, PixelFormat pixelFormat, const DeviceInfo::Resolution& resolution)> callback);

private:
    IGraphBuilder* m_graph = nullptr;
    ICaptureGraphBuilder2* m_captureBuilder = nullptr;
    IBaseFilter* m_deviceFilter = nullptr;
    IBaseFilter* m_sampleGrabberFilter = nullptr;
    IBaseFilter* m_dstNullFilter = nullptr;
    ISampleGrabber* m_sampleGrabber = nullptr;
    IMediaControl* m_mediaControl = nullptr;
    std::string m_deviceName;
    std::vector<std::string> m_allDeviceNames;
    
    std::vector<uint8_t> m_memCache;
    PixelFormat m_cameraPixelFormat{};

    std::chrono::steady_clock::time_point m_startTime{};
    bool m_firstFrameArrived = false;

    // 状态变量
    bool m_didSetup{ false };
    bool m_isOpened{ false };
    bool m_isRunning{ false };

    std::mutex m_callbackMutex;
};

ProviderImp* createProviderDirectShow();

} // namespace ccap

#endif
#endif // CAMERA_CAPTURE_DSHOW_H