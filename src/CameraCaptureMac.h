/**
 * @file CameraCaptureMac.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-04
 *
 */

#pragma once

#ifndef CAMERA_CAPTURE_MAC_H
#define CAMERA_CAPTURE_MAC_H

#if __APPLE__

#include "CameraCaptureImp.h"

#if __OBJC__
@class CameraCaptureObjc;
#else
typedef void* CameraCaptureObjc;
#endif

namespace ccap
{
class ProviderMac : public ProviderImp
{
public:
    ProviderMac();
    ~ProviderMac() override;

    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    std::optional<DeviceInfo> getDeviceInfo() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

    using ProviderImp::getFreeFrame;
    using ProviderImp::newFrameAvailable;

private:
    CameraCaptureObjc* m_imp{};
};

ProviderImp* createProviderMac();

} // namespace ccap

#endif

#endif // CAMERA_CAPTURE_MAC_H