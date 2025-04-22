/**
 * @file CameraCaptureMac.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#pragma once

#ifndef CAMERA_CAPTURE_MAC_H
#define CAMERA_CAPTURE_MAC_H

#if __APPLE__

#include "CameraCapture.h"

namespace ccap
{
class ProviderMac : public Provider
{
public:
    ~ProviderMac() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    void close() override;
    bool start() override;
    void pause() override;
    bool isStarted() const override;
    bool set(Property prop, double value) override;
    double get(Property prop) override;
    std::shared_ptr<Frame> grab(bool waitNewFrame) override;
    void setNewFrameCallback(std::function<void(std::shared_ptr<Frame>)> callback) override;
    void setFrameAllocator(std::shared_ptr<Allocator> allocator) override;

private:
};
} // namespace ccap

#endif

#endif // CAMERA_CAPTURE_MAC_H