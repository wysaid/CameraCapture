/**
 * @file ccap_writer_c.h
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface for ccap video writer.
 * @date 2025-05
 *
 * @note Requires CCAP_ENABLE_VIDEO_WRITER to be defined.
 *       Only available on Windows and macOS.
 */

#pragma once
#ifndef CCAP_WRITER_C_H
#define CCAP_WRITER_C_H

#include "ccap_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Forward Declarations ========== */

/** @brief Opaque pointer to ccap::VideoWriter C++ object */
typedef struct CcapVideoWriter CcapVideoWriter;

/* ========== Enumerations ========== */

/** @brief Video codec enumeration */
typedef enum {
    CCAP_VIDEO_CODEC_H264 = 0,  ///< H.264 / AVC (default, best compatibility)
    CCAP_VIDEO_CODEC_HEVC = 1,  ///< H.265 / HEVC (better compression, less compatible)
} CcapVideoCodec;

/** @brief Video container format */
typedef enum {
    CCAP_VIDEO_FORMAT_MP4 = 0,
    CCAP_VIDEO_FORMAT_MOV = 1,
} CcapVideoFormat;

/* ========== Data Structures ========== */

/**
 * @brief Video writer configuration.
 * @note Use `CCAP_WRITER_CONFIG_INIT` for codec/container/frameRate/bitRate defaults.
 *       `width` and `height` must still be set before opening a writer.
 */
typedef struct {
    CcapVideoCodec codec;          ///< Preferred codec
    CcapVideoFormat container;     ///< Container format
    uint32_t width;                ///< Frame width
    uint32_t height;               ///< Frame height
    double frameRate;              ///< Target frame rate; 0 lets open() normalize to 30fps
    uint64_t bitRate;              ///< Target bit rate in bits/s (0 = auto, YouTube recommended bitrates)
} CcapWriterConfig;

/**
 * @brief Default initializer for `CcapWriterConfig`.
 * @note `width` and `height` remain 0 and must be assigned by the caller.
 */
#define CCAP_WRITER_CONFIG_INIT { CCAP_VIDEO_CODEC_H264, CCAP_VIDEO_FORMAT_MP4, 0u, 0u, 30.0, 0ULL }

/* ========== Writer Lifecycle ========== */

/**
 * @brief Create a new video writer instance
 * @return Pointer to CcapVideoWriter instance, or NULL on failure
 */
CCAP_EXPORT CcapVideoWriter* ccap_video_writer_create(void);

/**
 * @brief Destroy a video writer instance and finalize the output file
 * @param writer Pointer to CcapVideoWriter instance
 */
CCAP_EXPORT void ccap_video_writer_destroy(CcapVideoWriter* writer);

/**
 * @brief Open writer to a file path
 * @param writer Pointer to CcapVideoWriter instance
 * @param filePath Output file path (e.g., "output.mp4")
 * @param config Writer configuration
 * @return true on success, false on failure
 */
CCAP_EXPORT bool ccap_video_writer_open(CcapVideoWriter* writer, const char* filePath,
                                        const CcapWriterConfig* config);

/**
 * @brief Close and finalize the output file
 * @param writer Pointer to CcapVideoWriter instance
 */
CCAP_EXPORT void ccap_video_writer_close(CcapVideoWriter* writer);

/**
 * @brief Check if writer is opened
 * @param writer Pointer to CcapVideoWriter instance
 * @return true if opened, false otherwise
 */
CCAP_EXPORT bool ccap_video_writer_is_opened(const CcapVideoWriter* writer);

/**
 * @brief Write a single frame
 * @param writer Pointer to CcapVideoWriter instance
 * @param frameInfo Frame data to write (must match configured width/height)
 * @param timestampNs Timestamp in nanoseconds (0 for auto-increment)
 * @return true on success, false on failure
 */
CCAP_EXPORT bool ccap_video_writer_write_frame(CcapVideoWriter* writer,
                                               const CcapVideoFrameInfo* frameInfo,
                                               uint64_t timestampNs);

/**
 * @brief Get the actual codec being used (may differ from config due to fallback)
 * @param writer Pointer to CcapVideoWriter instance
 * @return Actual codec enum value. Only meaningful after `ccap_video_writer_open()` succeeds.
 *         Unopened or null writers return `CCAP_VIDEO_CODEC_H264` for ABI compatibility.
 */
CCAP_EXPORT CcapVideoCodec ccap_video_writer_actual_codec(const CcapVideoWriter* writer);

#ifdef __cplusplus
}
#endif

#endif /* CCAP_WRITER_C_H */
