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
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mutex>
#include <thread>

namespace ccap
{

class ProviderWin : public ProviderImp
{
public:
    ProviderWin();
    ~ProviderWin() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

private:
    void captureThread();
    bool processFrame(IMFSample* sample);

    // Media Foundation interfaces
    IMFSourceReader* m_reader = nullptr;

    // 状态变量
    std::atomic<bool> m_isOpened{ false };
    std::atomic<bool> m_isRunning{ false };
    std::atomic<bool> m_stopRequested{ false };

    // 线程相关
    std::thread m_captureThread;
};
} // namespace ccap

#endif
#endif // CAMERA_CAPTURE_WIN_H