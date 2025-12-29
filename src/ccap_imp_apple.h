/**
 * @file ccap_imp_macos.h
 * @author wysaid (this@wysaid.org)
 * @date 2025-04
 *
 */

#pragma once

#ifndef CAMERA_CAPTURE_MAC_H
#define CAMERA_CAPTURE_MAC_H

#if __APPLE__

#include "ccap_imp.h"

#if __OBJC__
@class CameraCaptureObjc;
#else
typedef void* CameraCaptureObjc;
#endif

namespace ccap {

class FileReaderApple;

class ProviderApple : public ProviderImp {
public:
    ProviderApple();
    ~ProviderApple() override;

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

    inline FrameOrientation frameOrientation() const { return m_frameOrientation; }

    inline bool hasNewFrameCallback() const { return m_callback && *m_callback; }

    // File playback support
    bool setFileProperty(PropertyName prop, double value) override;
    double getFileProperty(PropertyName prop) const override;

private:
    bool openCamera(std::string_view deviceName);
    bool openFile(std::string_view filePath);

    CameraCaptureObjc* m_imp{};
    std::unique_ptr<FileReaderApple> m_fileReader;
};

ProviderImp* createProviderApple();

} // namespace ccap

#endif

#endif // CAMERA_CAPTURE_MAC_H