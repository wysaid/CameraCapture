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
    CCAP_VIDEO_CODEC_HEVC = 0,  ///< H.265 / HEVC (preferred)
    CCAP_VIDEO_CODEC_H264 = 1,  ///< H.264 / AVC (fallback)
} CcapVideoCodec;

/** @brief Video container format */
typedef enum {
    CCAP_VIDEO_FORMAT_MP4 = 0,
    CCAP_VIDEO_FORMAT_MOV = 1,
} CcapVideoFormat;

/* ========== Data Structures ========== */

/** @brief Video writer configuration */
typedef struct {
    CcapVideoCodec codec;          ///< Preferred codec
    CcapVideoFormat container;     ///< Container format
    uint32_t width;                ///< Frame width
    uint32_t height;               ///< Frame height
    double frameRate;              ///< Target frame rate (0 = variable)
    uint64_t bitRate;              ///< Target bit rate in bits/s (0 = auto)
} CcapWriterConfig;

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
 * @return Actual codec enum value
 */
CCAP_EXPORT CcapVideoCodec ccap_video_writer_actual_codec(const CcapVideoWriter* writer);

#ifdef __cplusplus
}
#endif

#endif /* CCAP_WRITER_C_H */
