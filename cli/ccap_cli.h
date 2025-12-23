/**
 * @file ccap_cli.h
 * @brief Header file for ccap CLI tool
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 */

#pragma once
#ifndef CCAP_CLI_H
#define CCAP_CLI_H

#include <ccap_def.h>
#include <string>

namespace ccap_cli {

/**
 * @brief CLI options parsed from command line arguments
 */
struct CLIOptions {
    // General options
    bool showHelp = false;
    bool showVersion = false;
    bool verbose = false;

    // Device enumeration
    bool listDevices = false;
    bool showDeviceInfo = false;
    int deviceInfoIndex = -1; // -1 means all devices

    // Capture options
    int deviceIndex = 0;
    std::string deviceName;
    int width = 1280;
    int height = 720;
    double fps = 30.0;
    int captureCount = 1;
    int timeoutMs = 5000;
    std::string outputDir;
    ccap::PixelFormat outputFormat = ccap::PixelFormat::Unknown;
    ccap::PixelFormat internalFormat = ccap::PixelFormat::Unknown;
    bool saveYuv = false;

    // Preview options (only when GLFW is enabled)
    bool enablePreview = false;
    bool previewOnly = false;

    // Format conversion options
    std::string convertInput;
    std::string convertOutput;
    ccap::PixelFormat yuvFormat = ccap::PixelFormat::Unknown;
    int yuvWidth = 0;
    int yuvHeight = 0;
};

/**
 * @brief Pixel format info with YUV flag
 */
struct PixelFormatInfo {
    ccap::PixelFormat format = ccap::PixelFormat::Unknown;
    bool isYuv = false;
};

/**
 * @brief Print CLI version information
 */
void printVersion();

/**
 * @brief Print CLI usage/help information
 * @param programName Name of the program (argv[0])
 */
void printUsage(const char* programName);

/**
 * @brief Parse pixel format string to PixelFormat enum
 * @param formatStr Format string (e.g., "rgb24", "nv12")
 * @return PixelFormatInfo containing format and isYuv flag
 */
PixelFormatInfo parsePixelFormat(const std::string& formatStr);

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @return Parsed CLI options
 */
CLIOptions parseArgs(int argc, char* argv[]);

/**
 * @brief List all available camera devices
 * @return 0 on success, non-zero on error
 */
int listDevices();

/**
 * @brief Show detailed information for a device
 * @param deviceIndex Device index (-1 for all devices)
 * @return 0 on success, non-zero on error
 */
int showDeviceInfo(int deviceIndex);

/**
 * @brief Save a video frame to file
 * @param frame Video frame to save
 * @param outputPath Output file path (without extension)
 * @param saveAsYuv Force saving as YUV format
 * @return true on success, false on error
 */
bool saveFrameToFile(ccap::VideoFrame* frame, const std::string& outputPath, bool saveAsYuv);

/**
 * @brief Save a video frame as BMP image file
 * @param frame Video frame to save
 * @param outputPath Output file path (without extension)
 * @return true on success, false on error
 */
bool saveFrameAsImage(ccap::VideoFrame* frame, const std::string& outputPath);

/**
 * @brief Capture frames from camera
 * @param opts CLI options
 * @return 0 on success, non-zero on error
 */
int captureFrames(const CLIOptions& opts);

#ifdef CCAP_CLI_WITH_GLFW
/**
 * @brief Run camera preview with GLFW window
 * @param opts CLI options
 * @return 0 on success, non-zero on error
 */
int runPreview(const CLIOptions& opts);
#endif

/**
 * @brief Convert YUV file to image
 * @param opts CLI options
 * @return 0 on success, non-zero on error
 */
int convertYuvToImage(const CLIOptions& opts);

} // namespace ccap_cli

#endif // CCAP_CLI_H
