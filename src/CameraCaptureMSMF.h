/**
 * @file CameraCaptureMSMF.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for ProviderMSMF class.
 * @date 2025-04
 *
 */

#pragma once
#ifndef CAMERA_CAPTURE_MSMF_H
#define CAMERA_CAPTURE_MSMF_H

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureImp.h"

///@TODO: (Next version) Add support for MSMF (Media Foundation) on Windows 10 and later.

namespace ccap
{
class ProviderMSMF : public ProviderImp
{
public:
    ProviderMSMF();
    ~ProviderMSMF() override;
    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    std::vector<PixelFormat> getHardwareSupportedPixelFormats() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

private:
    // 状态变量
    bool m_didSetup{ false };
    bool m_isOpened{ false };
    bool m_isRunning{ false };
};

ProviderImp* createProviderMSMF();

} // namespace ccap

#endif
#endif // CAMERA_CAPTURE_MSMF_H