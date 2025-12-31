/**
 * @file args_parser.cpp
 * @brief Command-line argument parser implementation
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 */

#include "args_parser.h"

#include <algorithm>
#include <ccap_config.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace ccap_cli {

// Default values
constexpr int DEFAULT_WIDTH = 1280;
constexpr int DEFAULT_HEIGHT = 720;
constexpr double DEFAULT_FPS = 30.0;
constexpr int DEFAULT_TIMEOUT_MS = 5000;

void printVersion() {
    std::cout << "ccap version " << CCAP_VERSION_STRING << " Copyright (c) 2025 wysaid\n";

#if defined(CCAP_CLI_BUILD_COMPILER) || defined(CCAP_CLI_BUILD_PLATFORM) || defined(CCAP_CLI_BUILD_ARCH)
    std::cout << "  built with";
#ifdef CCAP_CLI_BUILD_COMPILER
    std::cout << " " << CCAP_CLI_BUILD_COMPILER;
#endif
#ifdef CCAP_CLI_BUILD_PLATFORM
    std::cout << " on " << CCAP_CLI_BUILD_PLATFORM;
#endif
#ifdef CCAP_CLI_BUILD_ARCH
    std::cout << " (" << CCAP_CLI_BUILD_ARCH << ")";
#endif
#ifdef CCAP_CLI_LIBC_TYPE
    std::cout << " / " << CCAP_CLI_LIBC_TYPE;
#endif
    std::cout << "\n";
#endif

#ifdef CCAP_CLI_BUILD_TYPE
    std::cout << "  cmake options: -DCMAKE_BUILD_TYPE=" << CCAP_CLI_BUILD_TYPE;
#ifdef CCAP_CLI_STATIC_RUNTIME
#if CCAP_CLI_STATIC_RUNTIME
    std::cout << " -DCCAP_BUILD_CLI_STANDALONE=ON";
#else
    std::cout << " -DCCAP_BUILD_CLI_STANDALONE=OFF";
#endif
#else
    std::cout << " -DCCAP_BUILD_CLI_STANDALONE=OFF";
#endif

#ifdef CCAP_CLI_WITH_GLFW
    std::cout << " -DCCAP_CLI_WITH_GLFW=ON";
#else
    std::cout << " -DCCAP_CLI_WITH_GLFW=OFF";
#endif

#ifdef CCAP_CLI_WITH_STB_IMAGE
    std::cout << " -DCCAP_CLI_WITH_STB_IMAGE=ON";
#else
    std::cout << " -DCCAP_CLI_WITH_STB_IMAGE=OFF";
#endif
    std::cout << "\n";
#endif

    std::cout << "  libccap        " << CCAP_VERSION_STRING << "\n";

#ifdef CCAP_CLI_STATIC_RUNTIME
#if CCAP_CLI_STATIC_RUNTIME
    std::cout << "  runtime        static\n";
#else
    std::cout << "  runtime        dynamic\n";
#endif
#else
    std::cout << "  runtime        dynamic\n";
#endif

    std::cout << "Simple and efficient camera capture tool\n";
}

void printUsage(const char* programName) {
    std::cout << "usage: " << programName << " [options]\n"
              << "\n"
              << "Print help / information / capabilities:\n"
              << "  -h, --help                 show this help message\n"
              << "  -v, --version              show version information\n"
              << "  -l, --list-devices         list all available camera devices\n"
              << "  -I, --device-info index    show detailed info for device at index\n"
              << "\n"
              << "Global options:\n"
              << "  --verbose                  enable verbose logging (shows all messages)\n"
              << "  -q, --quiet                quiet mode (only show errors, equivalent to log level Error)\n"
              << "  --timeout seconds          program timeout (auto-exit after N seconds)\n"
              << "  --timeout-exit-code code   exit code when timeout occurs (default: 0)\n"
              << "\n"
              << "Input options:\n"
              << "  -i, --input source         input source: video file, device index, or device name\n"
              << "  -d, --device index|name    select camera device by index or name (default: 0)\n"
              << "\n"
              << "Capture options (camera mode):\n"
              << "  -w, --width width          set capture width (default: " << DEFAULT_WIDTH << ")\n"
              << "  -H, --height height        set capture height (default: " << DEFAULT_HEIGHT << ")\n"
              << "  -f, --fps fps              set frame rate (default: " << DEFAULT_FPS << ")\n"
              << "                             Camera mode: sets camera capture frame rate\n"
              << "                             Video mode: calculates playback speed from video's native fps\n"
              << "                             Note: Cannot be used with --speed\n"
              << "  -c, --count count          number of frames to capture, then exit\n"
              << "  -t, --grab-timeout ms      timeout for grabbing a single frame (default: " << DEFAULT_TIMEOUT_MS << ")\n"
              << "  --format, --output-format  output pixel format: rgb24, bgr24, rgba32, bgra32, nv12, i420, yuyv, uyvy\n"
              << "  --internal-format format   internal pixel format (camera native format, camera mode only)\n"
              << "\n"
              << "Save options:\n"
              << "  -o, --output dir           output directory for captured images\n"
              << "  -s, --save                 save captured frames to output directory\n"
              << "  --save-yuv                 save YUV frames directly (auto-enables --save)\n"
              << "  --save-format format       image format: jpg, png, bmp (auto-enables --save)\n"
              << "  --save-jpg                 save as JPEG format (shortcut for --save-format jpg)\n"
              << "  --save-png                 save as PNG format (shortcut for --save-format png)\n"
              << "  --save-bmp                 save as BMP format (shortcut for --save-format bmp)\n"
#ifdef CCAP_CLI_WITH_STB_IMAGE
              << "  --image-format format      image format: jpg, png, bmp (default: jpg)\n"
              << "  --jpeg-quality quality     JPEG quality 1-100 (default: 90, only for jpg)\n"
#else
              << "  --image-format format      image format: bmp (jpg/png require CCAP_CLI_WITH_STB_IMAGE=ON)\n"
#endif
              << "\n";

#ifdef CCAP_CLI_WITH_GLFW
    std::cout << "Preview options:\n"
              << "  -p, --preview              enable window preview\n"
              << "  --preview-only             same as --preview (kept for compatibility)\n"
              << "\n";
#endif

    std::cout << "Video playback options:\n"
              << "  --loop[=N]                 loop video playback (N times, omit for infinite)\n"
              << "                             Note: --loop and -c are mutually exclusive\n"
              << "  --speed speed              playback speed multiplier\n"
              << "                             0.0 = no frame rate control (process as fast as possible)\n"
              << "                             1.0 = normal speed (match video's original frame rate)\n"
              << "                             >1.0 = speed up (e.g., 2.0 = 2x speed)\n"
              << "                             <1.0 = slow down (e.g., 0.5 = half speed)\n"
              << "                             Default: 0.0 for capture mode, 1.0 for preview mode\n"
              << "                             Note: Cannot be used with -f/--fps\n"
              << "\n";

    std::cout << "Format conversion options:\n"
              << "  --convert input            convert YUV file to image\n"
              << "  --yuv-format format        YUV format: nv12, nv12f, i420, i420f, yuyv, yuyvf, uyvy, uyvyf\n"
              << "  --yuv-width width          width of YUV input\n"
              << "  --yuv-height height        height of YUV input\n"
              << "  --convert-output file      output file for conversion\n"
              << "\n";

    std::cout << "Examples:\n"
              << "  " << programName << " --list-devices\n"
              << "  " << programName << " --device-info 0\n"
              << "  " << programName << " -d 0                                 # print camera info\n"
              << "  " << programName << " -i /path/to/video.mp4                # print video info\n"
              << "  " << programName << " -d 0 -c 10 -o ./captures             # capture 10 frames\n"
              << "  " << programName << " -d 0 -c 10 -o ./captures --save-jpg  # capture and save as JPEG\n"
              << "  " << programName << " -i /path/to/video.mp4 -c 30 -o ./frames  # extract 30 frames from video\n"
              << "  " << programName << " -i /path/to/video.mp4 --loop         # loop video playback\n"
              << "  " << programName << " --timeout 60 -d 0 --preview          # preview for 60 seconds then exit\n";
#ifdef CCAP_CLI_WITH_GLFW
    std::cout << "  " << programName << " -d 0 --preview\n"
              << "  " << programName << " -i /path/to/video.mp4 --preview\n";
#endif
#ifdef CCAP_CLI_WITH_STB_IMAGE
    std::cout << "  " << programName << " --convert input.yuv --yuv-format nv12 --yuv-width 1920 --yuv-height 1080 --convert-output output.jpg --image-format jpg\n";
#else
    std::cout << "  " << programName << " --convert input.yuv --yuv-format nv12 --yuv-width 1920 --yuv-height 1080 --convert-output output.bmp\n";
#endif
}

PixelFormatInfo parsePixelFormat(const std::string& formatStr) {
    PixelFormatInfo info;
    std::string lower = formatStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "rgb24") {
        info.format = ccap::PixelFormat::RGB24;
        info.isYuv = false;
    } else if (lower == "bgr24") {
        info.format = ccap::PixelFormat::BGR24;
        info.isYuv = false;
    } else if (lower == "rgba32") {
        info.format = ccap::PixelFormat::RGBA32;
        info.isYuv = false;
    } else if (lower == "bgra32") {
        info.format = ccap::PixelFormat::BGRA32;
        info.isYuv = false;
    } else if (lower == "nv12") {
        info.format = ccap::PixelFormat::NV12;
        info.isYuv = true;
    } else if (lower == "nv12f") {
        info.format = ccap::PixelFormat::NV12f;
        info.isYuv = true;
    } else if (lower == "i420") {
        info.format = ccap::PixelFormat::I420;
        info.isYuv = true;
    } else if (lower == "i420f") {
        info.format = ccap::PixelFormat::I420f;
        info.isYuv = true;
    } else if (lower == "yuyv") {
        info.format = ccap::PixelFormat::YUYV;
        info.isYuv = true;
    } else if (lower == "yuyvf") {
        info.format = ccap::PixelFormat::YUYVf;
        info.isYuv = true;
    } else if (lower == "uyvy") {
        info.format = ccap::PixelFormat::UYVY;
        info.isYuv = true;
    } else if (lower == "uyvyf") {
        info.format = ccap::PixelFormat::UYVYf;
        info.isYuv = true;
    } else {
        info.format = ccap::PixelFormat::Unknown;
        info.isYuv = false;
    }
    return info;
}

ImageFormat parseImageFormat(const std::string& formatStr) {
    std::string lower = formatStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "jpg" || lower == "jpeg") {
        return ImageFormat::JPG;
    } else if (lower == "png") {
        return ImageFormat::PNG;
    } else if (lower == "bmp") {
        return ImageFormat::BMP;
    }

    // Default to JPG if format is not recognized
    return ImageFormat::JPG;
}

void parseInputSource(const std::string& inputStr, CLIOptions& opts) {
    // Check if input is a pure number (device index)
    bool isNumber = true;
    for (char c : inputStr) {
        if (!std::isdigit(c) && c != '-') {
            isNumber = false;
            break;
        }
    }

    if (isNumber) {
        // Input is a device index
        opts.deviceIndex = std::atoi(inputStr.c_str());
        opts.deviceName.clear();
        opts.videoFilePath.clear();
        return;
    }

    // Check if input is an existing file (video file)
    std::filesystem::path inputPath(inputStr);
    std::error_code ec;
    if (std::filesystem::exists(inputPath, ec) && std::filesystem::is_regular_file(inputPath, ec)) {
        // Input is a video file
        opts.videoFilePath = inputStr;
        opts.deviceName.clear();
        return;
    }

    // Input is a device name (not a file, not a number)
    opts.deviceName = inputStr;
    opts.videoFilePath.clear();
}

CLIOptions parseArgs(int argc, char* argv[]) {
    CLIOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            opts.showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            opts.showVersion = true;
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-l" || arg == "--list-devices") {
            opts.listDevices = true;
        } else if (arg == "-I" || arg == "--device-info") {
            opts.showDeviceInfo = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts.deviceInfoIndex = std::atoi(argv[++i]);
            }
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 < argc) {
                std::string inputStr = argv[++i];
                opts.inputSource = inputStr;
                parseInputSource(inputStr, opts);
            }
        } else if (arg == "-d" || arg == "--device") {
            if (i + 1 < argc) {
                const char* val = argv[++i];
                // Check if it's a number (handles positive and negative integers safely)
                if (std::isdigit(val[0]) || (val[0] == '-' && val[1] != '\0' && std::isdigit(val[1]))) {
                    opts.deviceIndex = std::atoi(val);
                } else {
                    opts.deviceName = val;
                }
            }
        } else if (arg == "--video") {
            if (i + 1 < argc) {
                opts.videoFilePath = argv[++i];
            }
        } else if (arg == "-w" || arg == "--width") {
            if (i + 1 < argc) {
                opts.width = std::atoi(argv[++i]);
            }
        } else if (arg == "-H" || arg == "--height") {
            if (i + 1 < argc) {
                opts.height = std::atoi(argv[++i]);
            }
        } else if (arg == "-f" || arg == "--fps") {
            if (i + 1 < argc) {
                opts.fps = std::atof(argv[++i]);
                opts.fpsSpecified = true;
            }
        } else if (arg == "-c" || arg == "--count") {
            if (i + 1 < argc) {
                opts.captureCount = std::atoi(argv[++i]);
                opts.captureCountSpecified = true;
            }
        } else if (arg == "-t" || arg == "--grab-timeout") {
            if (i + 1 < argc) {
                opts.grabTimeoutMs = std::atoi(argv[++i]);
            }
        } else if (arg == "--timeout") {
            if (i + 1 < argc) {
                opts.timeoutSeconds = std::atoi(argv[++i]);
            }
        } else if (arg == "--timeout-exit-code") {
            if (i + 1 < argc) {
                opts.timeoutExitCode = std::atoi(argv[++i]);
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                opts.outputDir = argv[++i];
            }
        } else if (arg == "-s" || arg == "--save") {
            opts.saveFrames = true;
            opts.saveFramesSpecified = true;
        } else if (arg == "--format" || arg == "--output-format") {
            if (i + 1 < argc) {
                auto info = parsePixelFormat(argv[++i]);
                opts.outputFormat = info.format;
            }
        } else if (arg == "--internal-format") {
            if (i + 1 < argc) {
                auto info = parsePixelFormat(argv[++i]);
                opts.internalFormat = info.format;
            }
        } else if (arg == "--save-yuv") {
            opts.saveYuv = true;
            opts.saveFrames = true;
            // Auto-set internal format to NV12 if not specified
            if (opts.internalFormat == ccap::PixelFormat::Unknown) {
                opts.internalFormat = ccap::PixelFormat::NV12;
            }
        } else if (arg == "--save-format") {
            if (i + 1 < argc) {
                opts.imageFormat = parseImageFormat(argv[++i]);
                opts.saveFrames = true;
            }
        } else if (arg == "--save-jpg" || arg == "--save-jpeg") {
            opts.imageFormat = ImageFormat::JPG;
            opts.saveFrames = true;
        } else if (arg == "--save-png") {
            opts.imageFormat = ImageFormat::PNG;
            opts.saveFrames = true;
        } else if (arg == "--save-bmp") {
            opts.imageFormat = ImageFormat::BMP;
            opts.saveFrames = true;
        } else if (arg == "--image-format") {
            if (i + 1 < argc) {
                opts.imageFormat = parseImageFormat(argv[++i]);
            }
        } else if (arg == "--jpeg-quality") {
            if (i + 1 < argc) {
                int quality = std::atoi(argv[++i]);
                opts.jpegQuality = std::max(1, std::min(100, quality)); // Clamp to 1-100
            }
        } else if (arg == "--loop" || arg.rfind("--loop=", 0) == 0) {
            opts.enableLoop = true;
            if (arg.rfind("--loop=", 0) == 0) {
                // Parse loop count from --loop=N
                std::string countStr = arg.substr(7);
                opts.loopCount = std::atoi(countStr.c_str());
            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                // Check if next arg is a number (optional loop count)
                const char* nextArg = argv[i + 1];
                bool isNum = true;
                for (const char* p = nextArg; *p; ++p) {
                    if (!std::isdigit(*p)) {
                        isNum = false;
                        break;
                    }
                }
                if (isNum) {
                    opts.loopCount = std::atoi(argv[++i]);
                }
            }
            // loopCount = 0 means infinite loop
        } else if (arg == "--speed") {
            if (i + 1 < argc) {
                opts.playbackSpeed = std::atof(argv[++i]);
                opts.playbackSpeedSpecified = true;
            }
        } else if (arg == "-p" || arg == "--preview") {
            opts.enablePreview = true;
        } else if (arg == "--preview-only") {
            opts.enablePreview = true;
            opts.previewOnly = true;
        } else if (arg == "--convert") {
            if (i + 1 < argc) {
                opts.convertInput = argv[++i];
            }
        } else if (arg == "--yuv-format") {
            if (i + 1 < argc) {
                auto info = parsePixelFormat(argv[++i]);
                opts.yuvFormat = info.format;
            }
        } else if (arg == "--yuv-width") {
            if (i + 1 < argc) {
                opts.yuvWidth = std::atoi(argv[++i]);
            }
        } else if (arg == "--yuv-height") {
            if (i + 1 < argc) {
                opts.yuvHeight = std::atoi(argv[++i]);
            }
        } else if (arg == "--convert-output") {
            if (i + 1 < argc) {
                opts.convertOutput = argv[++i];
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n\n";
            printUsage(argv[0]);
            std::exit(1);
        }
    }

    // Post-processing: auto-enable save mode if -o is specified without preview
    if (!opts.outputDir.empty() && !opts.enablePreview && !opts.saveFramesSpecified) {
        opts.saveFrames = true;
    }

    // Post-processing: if -o is specified with save options, enable save
    if (!opts.outputDir.empty() && (opts.saveYuv || opts.saveFramesSpecified)) {
        opts.saveFrames = true;
    }

    return opts;
}

} // namespace ccap_cli
