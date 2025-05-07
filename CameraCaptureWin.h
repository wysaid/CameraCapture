/**
 * @file CameraCaptureWin.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for ProviderWin class.
 * @date 2025-04
 *
 */

#pragma once
#ifndef CAMERA_CAPTURE_WIN_H
#define CAMERA_CAPTURE_WIN_H

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureImp.h"

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
class ProviderWin : public ProviderImp, public ISampleGrabberCB
{
public:
    ProviderWin();
    ~ProviderWin() override;
    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
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

    bool setup();
    void enumerateDevices(std::function<bool(IMoniker* moniker, std::string_view)> callback);

private:
    IGraphBuilder* m_graph = nullptr;
    ICaptureGraphBuilder2* m_captureBuilder = nullptr;
    IBaseFilter* m_deviceFilter = nullptr;
    IBaseFilter* m_sampleGrabberFilter = nullptr;
    ISampleGrabber* m_sampleGrabber = nullptr;
    IMediaControl* m_mediaControl = nullptr;

    // 状态变量
    bool m_didSetup{ false };
    bool m_isOpened{ false };
    bool m_isRunning{ false };
};
} // namespace ccap

#endif
#endif // CAMERA_CAPTURE_WIN_H