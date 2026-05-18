/**
 * @file ccap_writer.h
 * @author wysaid (this@wysaid.org)
 * @brief Video writer header file for ccap.
 * @date 2025-05
 *
 * @note Requires CCAP_ENABLE_VIDEO_WRITER to be defined.
 *       Only available on Windows and macOS.
 */

#ifndef __cplusplus
#error "ccap_writer.h is for C++ only. For C language, please use ccap_writer_c.h instead."
#endif

#pragma once
#ifndef CCAP_WRITER_H
#define CCAP_WRITER_H

#include "ccap_def.h"

#include <memory>
#include <string_view>

namespace ccap {

/**
 * @brief Video codec for encoding.
 */
enum class VideoCodec {
    H264,  ///< H.264 / AVC (default, best compatibility and performance)
    HEVC,  ///< H.265 / HEVC (better compression, less compatible)
};

/**
 * @brief Video container format.
 */
enum class VideoFormat {
    MP4,   ///< MP4 container
    MOV,   ///< MOV container
};

/**
 * @brief Configuration for video writer.
 */
struct WriterConfig {
    VideoCodec codec = VideoCodec::H264; ///< Default codec; auto-fallback to HEVC if H.264 is unavailable
    VideoFormat container = VideoFormat::MP4;
    uint32_t width = 0;          ///< Frame width in pixels
    uint32_t height = 0;         ///< Frame height in pixels
    double frameRate = 30.0;     ///< Target frame rate (default 30fps; used for timestamp generation when timestampNs is 0)
    uint64_t bitRate = 0;        ///< Target bit rate in bits/s; 0 = auto (YouTube official recommended bitrates)
};

/**
 * @brief Video file writer. Captures frames and encodes them into a video file.
 * @note This class is not thread-safe. Use it in a single thread or protect with a mutex.
 */
class CCAP_EXPORT VideoWriter {
public:
    VideoWriter();
    ~VideoWriter();

    /// Move-only
    VideoWriter(VideoWriter&&) noexcept;
    VideoWriter& operator=(VideoWriter&&) noexcept;
    VideoWriter(const VideoWriter&) = delete;
    VideoWriter& operator=(const VideoWriter&) = delete;

    /**
     * @brief Open writer to a file path.
     * @param filePath Output file path (e.g., "output.mp4")
     * @param config Writer configuration (width, height, codec, etc.)
        * @note Call `close()` before reopening an existing writer instance.
     * @return true on success, false on failure.
     */
    bool open(std::string_view filePath, const WriterConfig& config);

    /// Close and finalize the file.
    void close();
    bool isOpened() const;

    /**
     * @brief Write a single frame.
     * @param frame The video frame to write. Pixel format will be converted to NV12 internally.
     * @param timestampNs Optional timestamp in nanoseconds. If 0, auto-increment based on frameRate.
     * @return true on success, false on failure.
     */
    bool writeFrame(const VideoFrame& frame, uint64_t timestampNs = 0);

    /// Query the actual codec being used (may differ from config due to fallback).
    /// Only meaningful after `open()` succeeds.
    VideoCodec actualCodec() const;

    uint32_t width() const;
    uint32_t height() const;
    double frameRate() const;

    struct Impl;

private:
    void* m_impl;
};

} // namespace ccap

#endif // CCAP_WRITER_H
