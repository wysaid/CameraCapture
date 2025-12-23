/**
 * @file ccap_file_reader_apple.h
 * @author wysaid (this@wysaid.org)
 * @brief Video file reader interface for macOS/iOS using AVFoundation
 * @date 2025-12
 *
 */

#pragma once

#ifndef CCAP_FILE_READER_APPLE_H
#define CCAP_FILE_READER_APPLE_H

#if __APPLE__

#include "ccap_imp.h"

#include <string>
#include <string_view>

#if __OBJC__
@class CcapFileReaderObjc;
#else
typedef void* CcapFileReaderObjc;
#endif

namespace ccap {

class ProviderApple;

class FileReaderApple {
public:
    FileReaderApple(ProviderApple* provider);
    ~FileReaderApple();

    bool open(std::string_view filePath);
    void close();
    bool isOpened() const;

    bool start();
    void stop();
    bool isStarted() const;

    /// Get video duration in seconds
    double getDuration() const;

    /// Get total frame count
    double getFrameCount() const;

    /// Get current playback time in seconds
    double getCurrentTime() const;

    /// Seek to specified time in seconds
    bool seekToTime(double timeInSeconds);

    /// Get current frame index
    double getCurrentFrameIndex() const;

    /// Seek to specified frame
    bool seekToFrame(int64_t frameIndex);

    /// Get playback speed (1.0 = normal)
    double getPlaybackSpeed() const;

    /// Set playback speed
    bool setPlaybackSpeed(double speed);

    /// Get frame rate
    double getFrameRate() const;

    /// Get video width
    int getWidth() const;

    /// Get video height
    int getHeight() const;

private:
    CcapFileReaderObjc* m_objc = nullptr;
    ProviderApple* m_provider = nullptr;
};

} // namespace ccap

#endif // __APPLE__
#endif // CCAP_FILE_READER_APPLE_H
