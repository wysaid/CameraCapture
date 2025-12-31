/**
 * @file ccap_file_reader_windows.h
 * @author wysaid (this@wysaid.org)
 * @brief Video file reader interface for Windows using Media Foundation
 * @date 2025-12
 *
 */

#pragma once

#ifndef CCAP_FILE_READER_WINDOWS_H
#define CCAP_FILE_READER_WINDOWS_H

#if defined(_WIN32) || defined(_MSC_VER)

#include "ccap_imp.h"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

struct IMFSourceReader;
struct IMFMediaType;

namespace ccap {

class ProviderDirectShow;

class FileReaderWindows {
public:
    FileReaderWindows(ProviderDirectShow* provider);
    ~FileReaderWindows();

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
    bool initMediaFoundation();
    void uninitMediaFoundation();
    bool createSourceReader(const std::wstring& filePath);
    bool configureOutput();
    void readLoop();
    void processFrame();

    ProviderDirectShow* m_provider = nullptr;
    IMFSourceReader* m_sourceReader = nullptr;

    double m_duration = 0.0;
    double m_frameRate = 30.0;
    int64_t m_totalFrameCount = 0;
    int m_width = 0;
    int m_height = 0;

    std::atomic<int64_t> m_currentFrameIndex{ 0 };
    std::atomic<double> m_currentTime{ 0.0 };
    std::atomic<double> m_playbackSpeed{ 0.0 };

    std::atomic<bool> m_isOpened{ false };
    std::atomic<bool> m_isStarted{ false };
    std::atomic<bool> m_shouldStop{ false };
    std::atomic<bool> m_isReading{ false };

    bool m_mfInitialized = false;
};

} // namespace ccap

#endif // _WIN32 || _MSC_VER
#endif // CCAP_FILE_READER_WINDOWS_H
