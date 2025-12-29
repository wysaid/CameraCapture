/**
 * @file ccap_cli_utils.h
 * @brief Utility functions for ccap CLI tool
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 */

#ifndef CCAP_CLI_UTILS_H
#define CCAP_CLI_UTILS_H

#include "args_parser.h"
#include <ccap.h>

namespace ccap_cli {

// ============================================================================
// Device Operations
// ============================================================================

/**
 * @brief List all available camera devices
 * @return Exit code (0 on success)
 */
int listDevices();

/**
 * @brief Show detailed information about a device
 * @param deviceIndex Device index (-1 for all devices)
 * @return Exit code (0 on success, 1 on error)
 */
int showDeviceInfo(int deviceIndex);

/**
 * @brief Print video file information
 * @param videoPath Path to video file
 * @return Exit code (0 on success, 1 on error)
 */
int printVideoInfo(const std::string& videoPath);

/**
 * @brief Capture frames from camera or video file
 * @param opts CLI options
 * @return Exit code (0 on success, 1 on error)
 */
int captureFrames(const CLIOptions& opts);

// ============================================================================
// Format Conversion & Saving
// ============================================================================

/**
 * @brief Save frame to file (YUV or image format)
 * @param frame Video frame to save
 * @param outputPath Output file path (without extension)
 * @param saveAsYuv Whether to save as YUV format
 * @param imageFormat Image format to use
 * @param jpegQuality JPEG quality (1-100)
 * @return true on success, false on failure
 */
bool saveFrameToFile(ccap::VideoFrame* frame, const std::string& outputPath,
                     bool saveAsYuv, ImageFormat imageFormat, int jpegQuality);

/**
 * @brief Save frame as image (BMP/JPG/PNG)
 * @param frame Video frame to save
 * @param outputPath Output file path (without extension)
 * @param imageFormat Image format to use
 * @param jpegQuality JPEG quality (1-100)
 * @return true on success, false on failure
 */
bool saveFrameAsImage(ccap::VideoFrame* frame, const std::string& outputPath,
                      ImageFormat imageFormat, int jpegQuality);

/**
 * @brief Convert YUV file to image
 * @param opts CLI options
 * @return Exit code (0 on success, 1 on error)
 */
int convertYuvToImage(const CLIOptions& opts);

// ============================================================================
// Preview Window (GLFW)
// ============================================================================

#ifdef CCAP_CLI_WITH_GLFW
/**
 * @brief Run preview window with GLFW
 * @param opts CLI options
 * @return Exit code (0 on success, 1 on error)
 */
int runPreview(const CLIOptions& opts);
#endif

} // namespace ccap_cli

#endif // CCAP_CLI_UTILS_H
