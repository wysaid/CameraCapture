/**
 * @file ccap_imp_linux.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for Linux implementation of ccap::Provider class using V4L2.
 * @date 2025-04
 *
 */

#pragma once
#ifndef CAMERA_CAPTURE_LINUX_H
#define CAMERA_CAPTURE_LINUX_H

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "ccap_imp.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
}

namespace ccap {

/**
 * @brief V4L2-based camera provider implementation for Linux
 */
class ProviderV4L2 : public ProviderImp {
public:
    ProviderV4L2();
    ~ProviderV4L2() override;

    // ProviderImp interface implementation
    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    std::optional<DeviceInfo> getDeviceInfo() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

private:
    struct V4L2Buffer {
        void* start = nullptr;
        size_t length = 0;
        uint32_t index = 0;
    };

    struct V4L2Format {
        uint32_t pixelformat;   // V4L2 fourcc
        PixelFormat ccapFormat; // ccap pixel format
        const char* name;
    };

    // Internal helper methods
    bool setupDevice();
    bool negotiateFormat();
    bool allocateBuffers();
    void releaseBuffers();
    bool startStreaming();
    void stopStreaming();
    void captureThread();
    bool readFrame();

    // V4L2 utility methods
    bool queryCapabilities();
    bool enumerateFormats();
    bool enumerateFrameSizes();
    std::vector<DeviceInfo::Resolution> getSupportedResolutions(uint32_t pixelformat);
    PixelFormat v4l2FormatToCcapFormat(uint32_t v4l2Format);
    uint32_t ccapFormatToV4l2Format(PixelFormat ccapFormat);
    const char* getFormatName(uint32_t pixelformat);

    // Device discovery
    bool isVideoDevice(const std::string& devicePath);
    std::string getDeviceDescription(const std::string& devicePath);

private:
    // Device state
    int m_fd = -1;
    std::string m_devicePath;
    std::string m_deviceName;
    bool m_isOpened = false;
    bool m_isStreaming = false;

    // V4L2 device capabilities
    struct v4l2_capability m_caps{};
    std::vector<V4L2Format> m_supportedFormats;
    std::vector<DeviceInfo::Resolution> m_supportedResolutions;

    // Current format
    struct v4l2_format m_currentFormat{};

    // Buffer management
    std::vector<V4L2Buffer> m_buffers;
    static constexpr size_t kBufferCount = 4;

    // Capture thread
    std::unique_ptr<std::thread> m_captureThread;
    std::atomic<bool> m_shouldStop{ false };
    std::mutex m_captureMutex;
    std::condition_variable m_captureCondition;

    // Frame management
    std::chrono::steady_clock::time_point m_startTime{};
    uint64_t m_frameIndex{ 0 };

    std::shared_ptr<int> m_lifeHolder; // To keep the provider alive while frames are being processed

    // Supported V4L2 formats mapping
    static const std::vector<V4L2Format> s_supportedV4L2Formats;
};

/**
 * @brief Create a V4L2 provider instance
 */
ProviderImp* createProviderV4L2();

} // namespace ccap

#endif // Linux check
#endif // CAMERA_CAPTURE_LINUX_H