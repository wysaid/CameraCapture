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
 * @brief Image format for saving captured frames
 */
enum class ImageFormat {
    BMP,  // BMP format (always available)
    JPG,  // JPEG format (requires CCAP_CLI_WITH_STB_IMAGE)
    PNG   // PNG format (requires CCAP_CLI_WITH_STB_IMAGE)
};

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
    ImageFormat imageFormat = ImageFormat::JPG; // Default to JPG format
    int jpegQuality = 90; // JPEG quality (1-100), default 90

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
 * @brief Parse image format string to ImageFormat enum
 * @param formatStr Format string (e.g., "jpg", "png", "bmp")
 * @return ImageFormat enum value
 */
ImageFormat parseImageFormat(const std::string& formatStr);

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
 * @param imageFormat Image format to use (JPG/PNG/BMP)
 * @param jpegQuality JPEG quality (1-100, only used for JPG format)
 * @return true on success, false on error
 */
bool saveFrameToFile(ccap::VideoFrame* frame, const std::string& outputPath, bool saveAsYuv, 
                     ImageFormat imageFormat = ImageFormat::JPG, int jpegQuality = 90);

/**
 * @brief Save a video frame as image file (BMP/JPG/PNG)
 * @param frame Video frame to save
 * @param outputPath Output file path (without extension)
 * @param imageFormat Image format to use (JPG/PNG/BMP)
 * @param jpegQuality JPEG quality (1-100, only used for JPG format)
 * @return true on success, false on error
 */
bool saveFrameAsImage(ccap::VideoFrame* frame, const std::string& outputPath,
                      ImageFormat imageFormat = ImageFormat::JPG, int jpegQuality = 90);

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
