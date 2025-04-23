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

namespace ccap
{
class ProviderMac : public ProviderImp
{
public:
    ~ProviderMac() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

private:
};

} // namespace ccap

#endif

#endif // CAMERA_CAPTURE_MAC_H