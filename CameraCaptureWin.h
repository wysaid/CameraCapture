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

#include "CameraCapture.h"

#include <atomic>
#include <deque>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mutex>
#include <thread>

namespace ccap
{
constexpr int MAX_CACHE_FRAME_SIZE = 10;

class ProviderWin : public Provider
{
public:
    ProviderWin();
    ~ProviderWin() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    void close() override;
    bool start() override;
    void pause() override;
    bool isStarted() const override;
    bool set(Property prop, double value) override;
    double get(Property prop) override;
    std::shared_ptr<Frame> grab(bool waitNewFrame) override;
    void setNewFrameCallback(std::function<bool(std::shared_ptr<Frame>)> callback) override;
    void setFrameAllocator(std::shared_ptr<Allocator> allocator) override;

private:
    void captureThread();
    std::shared_ptr<Frame> getFreeFrame();
    bool processFrame(IMFSample* sample);

    // Media Foundation interfaces
    IMFSourceReader* m_reader = nullptr;

    // 状态变量
    std::atomic<bool> m_isOpened{ false };
    std::atomic<bool> m_isRunning{ false };
    std::atomic<bool> m_stopRequested{ false };

    // 帧处理相关
    std::shared_ptr<Frame> m_latestFrame;
    std::mutex m_frameMutex;
    std::condition_variable m_frameCondition;
    bool m_newFrameAvailable{ false };

    // 线程相关
    std::thread m_captureThread;

    // 回调和分配器
    std::shared_ptr<std::function<void(std::shared_ptr<Frame>)>> m_callback;
    std::shared_ptr<Allocator> m_allocator;

    std::deque<std::shared_ptr<Frame>> m_framePool;

    // 相机属性
    double m_fps{ 30.0 };
    PixelFormat m_pixelFormat{ PixelFormat::BGR888 };
    std::atomic_uint32_t m_frameIndex{};
    int m_width{ 640 };
    int m_height{ 480 };
};
} // namespace ccap

#endif
#endif // CAMERA_CAPTURE_WIN_H