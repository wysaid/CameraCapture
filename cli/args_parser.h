/**
 * @file args_parser.h
 * @brief Command-line argument parser for ccap CLI
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 */

#ifndef CCAP_CLI_ARGS_PARSER_H
#define CCAP_CLI_ARGS_PARSER_H

#include <ccap.h>
#include <string>

namespace ccap_cli {

// Image format for saving
enum class ImageFormat {
    JPG,
    PNG,
    BMP
};

// Pixel format info
struct PixelFormatInfo {
    ccap::PixelFormat format = ccap::PixelFormat::Unknown;
    bool isYuv = false;
};

// CLI options structure
struct CLIOptions {
    // Mode flags
    bool showHelp = false;
    bool showVersion = false;
    bool listDevices = false;
    bool showDeviceInfo = false;
    bool verbose = false;

    // Input source
    std::string inputSource;
    int deviceIndex = 0;
    std::string deviceName;
    std::string videoFilePath;
    int deviceInfoIndex = -1;

    // Capture parameters
    int width = 1280;
    int height = 720;
    double fps = 30.0;
    int captureCount = 1;
    bool captureCountSpecified = false;
    bool fpsSpecified = false;
    int grabTimeoutMs = 5000;

    // Timeout settings
    int timeoutSeconds = 0;
    int timeoutExitCode = 0;

    // Output settings
    std::string outputDir;
    bool saveFrames = false;
    bool saveFramesSpecified = false;
    bool saveYuv = false;
    ImageFormat imageFormat = ImageFormat::JPG;
    int jpegQuality = 90;

    // Pixel formats
    ccap::PixelFormat outputFormat = ccap::PixelFormat::Unknown;
    ccap::PixelFormat internalFormat = ccap::PixelFormat::Unknown;

    // Preview settings
    bool enablePreview = false;
    bool previewOnly = false;

    // Loop settings (video playback)
    bool enableLoop = false;
    int loopCount = 0; // 0 = infinite loop
    double playbackSpeed = 0.0; // 0.0 = no frame rate control, 1.0 = normal speed
    bool playbackSpeedSpecified = false;

    // Conversion settings
    std::string convertInput;
    std::string convertOutput;
    ccap::PixelFormat yuvFormat = ccap::PixelFormat::Unknown;
    int yuvWidth = 0;
    int yuvHeight = 0;
};

/**
 * @brief Parse pixel format string
 * @param formatStr Format string (e.g., "rgb24", "nv12")
 * @return Pixel format info
 */
PixelFormatInfo parsePixelFormat(const std::string& formatStr);

/**
 * @brief Parse image format string
 * @param formatStr Format string (e.g., "jpg", "png", "bmp")
 * @return Image format enum
 */
ImageFormat parseImageFormat(const std::string& formatStr);

/**
 * @brief Parse input source (device index, name, or video file)
 * @param inputStr Input string
 * @param opts CLI options to update
 */
void parseInputSource(const std::string& inputStr, CLIOptions& opts);

/**
 * @brief Parse command-line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @return Parsed CLI options
 */
CLIOptions parseArgs(int argc, char* argv[]);

/**
 * @brief Print version information
 */
void printVersion();

/**
 * @brief Print usage help
 * @param programName Program name (argv[0])
 */
void printUsage(const char* programName);

} // namespace ccap_cli

#endif // CCAP_CLI_ARGS_PARSER_H
