/**
 * @file ccap_imp_windows_msmf.h
 * @brief Header file for ProviderMSMF class.
 * @date 2026-03
 */

#pragma once
#ifndef CAMERA_CAPTURE_MSMF_H
#define CAMERA_CAPTURE_MSMF_H

#if defined(_WIN32) || defined(_MSC_VER)

#include "ccap_imp.h"

#include <atomic>
#include <guiddef.h>
#include <string>
#include <thread>
#include <vector>

struct IMFMediaSource;
struct IMFMediaType;
struct IMFSourceReader;

namespace ccap {

class ProviderMSMF : public ProviderImp {
public:
    ProviderMSMF();
    ~ProviderMSMF() override;

    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    std::optional<DeviceInfo> getDeviceInfo() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

private:
    struct DeviceEntry {
        std::string friendlyName;
        std::wstring symbolicLink;
    };

    struct MediaTypeInfo {
        IMFMediaType* mediaType = nullptr;
        GUID subtype{};
        PixelFormat pixelFormat = PixelFormat::Unknown;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t frameRateNumerator = 0;
        uint32_t frameRateDenominator = 0;
        double fps = 0.0;
        bool isCompressed = false;
    };

private:
    bool setup();
    bool initMediaFoundation();
    void uninitMediaFoundation();
    bool ensureDeviceCache();
    bool enumerateDevices(std::vector<DeviceEntry>& devices);
    bool createMediaSource(const std::wstring& symbolicLink);
    bool createSourceReader();
    std::vector<MediaTypeInfo> enumerateMediaTypes() const;
    void releaseMediaTypes(std::vector<MediaTypeInfo>& mediaTypes) const;
    bool configureMediaType();
    bool trySetCurrentMediaType(const MediaTypeInfo& selected, const GUID& subtype);
    bool updateCurrentMediaType();
    void readLoop();

private:
    std::vector<DeviceEntry> m_devices;
    std::string m_deviceName;
    std::wstring m_deviceSymbolicLink;
    IMFMediaSource* m_mediaSource = nullptr;
    IMFSourceReader* m_sourceReader = nullptr;
    std::thread m_readThread;
    std::atomic<bool> m_shouldStop{ false };
    std::atomic<bool> m_isRunning{ false };
    bool m_isOpened{ false };
    bool m_didSetup{ false };
    bool m_didInitializeCom{ false };
    bool m_mfInitialized{ false };
    PixelFormat m_activePixelFormat = PixelFormat::Unknown;
    uint32_t m_activeWidth = 0;
    uint32_t m_activeHeight = 0;
    double m_activeFps = 0.0;
    int32_t m_activeStride0 = 0;
    FrameOrientation m_inputOrientation = FrameOrientation::TopToBottom;
};

ProviderImp* createProviderMSMF();

} // namespace ccap

#endif
#endif // CAMERA_CAPTURE_MSMF_H