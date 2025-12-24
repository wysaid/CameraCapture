/**
 * @file ccap_cli.cpp
 * @brief CLI tool for CameraCapture (ccap) library
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 *
 * A command-line interface for camera capture operations.
 * Supports camera enumeration, capture, format conversion, and optional window preview.
 */

#include "ccap_cli.h"

#include <algorithm>
#include <ccap.h>
#include <ccap_config.h>
#include <ccap_convert.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef CCAP_CLI_WITH_STB_IMAGE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#ifdef CCAP_CLI_WITH_GLFW
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace ccap_cli {

// Default values
constexpr int DEFAULT_WIDTH = 1280;
constexpr int DEFAULT_HEIGHT = 720;
constexpr double DEFAULT_FPS = 30.0;
constexpr int DEFAULT_CAPTURE_COUNT = 1;
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
              << "  -i, --device-info index    show detailed info for device at index\n"
              << "\n"
              << "Global options:\n"
              << "  --verbose                  enable verbose logging\n"
              << "\n"
              << "Capture options:\n"
              << "  -d, --device index|name    select camera device by index or name (default: 0)\n"
              << "  -w, --width width          set capture width (default: " << DEFAULT_WIDTH << ")\n"
              << "  -H, --height height        set capture height (default: " << DEFAULT_HEIGHT << ")\n"
              << "  -f, --fps fps              set frame rate (default: " << DEFAULT_FPS << ")\n"
              << "  -c, --count count          number of frames to capture (default: " << DEFAULT_CAPTURE_COUNT << ")\n"
              << "  -t, --timeout ms           capture timeout in milliseconds (default: " << DEFAULT_TIMEOUT_MS << ")\n"
              << "  -o, --output dir           output directory for captured images\n"
              << "  --format format            output pixel format: rgb24, bgr24, rgba32, bgra32, nv12, i420, yuyv, uyvy\n"
              << "  --internal-format format   internal pixel format (camera native format)\n"
              << "  --save-yuv                 save YUV frames directly without conversion\n"
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
              << "  --preview-only             preview without saving frames\n"
              << "\n";
#endif

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
#ifdef CCAP_CLI_WITH_STB_IMAGE
              << "  " << programName << " -d 0 -w 1920 -H 1080 -c 10 -o ./captures --image-format jpg\n"
#else
              << "  " << programName << " -d 0 -w 1920 -H 1080 -c 10 -o ./captures\n"
#endif
              << "  " << programName << " -d 0 -w 1920 -H 1080 -c 10 -o ./captures --image-format png\n";
#ifdef CCAP_CLI_WITH_GLFW
    std::cout << "  " << programName << " -d 0 --preview\n";
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
        } else if (arg == "-i" || arg == "--device-info") {
            opts.showDeviceInfo = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts.deviceInfoIndex = std::atoi(argv[++i]);
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
            }
        } else if (arg == "-c" || arg == "--count") {
            if (i + 1 < argc) {
                opts.captureCount = std::atoi(argv[++i]);
            }
        } else if (arg == "-t" || arg == "--timeout") {
            if (i + 1 < argc) {
                opts.timeoutMs = std::atoi(argv[++i]);
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                opts.outputDir = argv[++i];
            }
        } else if (arg == "--format") {
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
        } else if (arg == "--image-format") {
            if (i + 1 < argc) {
                opts.imageFormat = parseImageFormat(argv[++i]);
            }
        } else if (arg == "--jpeg-quality") {
            if (i + 1 < argc) {
                int quality = std::atoi(argv[++i]);
                opts.jpegQuality = std::max(1, std::min(100, quality)); // Clamp to 1-100
            }
        }
#ifdef CCAP_CLI_WITH_GLFW
        else if (arg == "-p" || arg == "--preview") {
            opts.enablePreview = true;
        } else if (arg == "--preview-only") {
            opts.enablePreview = true;
            opts.previewOnly = true;
        }
#endif
        else if (arg == "--convert") {
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

    return opts;
}

int listDevices() {
    ccap::Provider provider;
    auto deviceNames = provider.findDeviceNames();

    if (deviceNames.empty()) {
        std::cout << "No camera devices found." << std::endl;
        return 0;
    }

    std::cout << "Found " << deviceNames.size() << " camera device(s):\n"
              << std::endl;

    for (size_t i = 0; i < deviceNames.size(); ++i) {
        std::cout << "[" << i << "] " << deviceNames[i] << std::endl;

        // Try to get device info
        ccap::Provider devProvider(deviceNames[i]);
        if (devProvider.isOpened()) {
            auto info = devProvider.getDeviceInfo();
            if (info) {
                // Print supported resolutions
                if (!info->supportedResolutions.empty()) {
                    if (info->supportedResolutions.size() <= 5) {
                        // Horizontal display for few resolutions
                        std::cout << "    Resolutions: ";
                        for (size_t j = 0; j < info->supportedResolutions.size(); ++j) {
                            if (j > 0) std::cout << ", ";
                            std::cout << info->supportedResolutions[j].width << "x" << info->supportedResolutions[j].height;
                        }
                        std::cout << std::endl;
                    } else {
                        // Vertical display for many resolutions
                        std::cout << "    Resolutions:" << std::endl;
                        for (const auto& res : info->supportedResolutions) {
                            std::cout << "      " << res.width << "x" << res.height << std::endl;
                        }
                    }
                }

                // Print supported pixel formats
                if (!info->supportedPixelFormats.empty()) {
                    std::cout << "    Formats: ";
                    for (size_t j = 0; j < info->supportedPixelFormats.size(); ++j) {
                        if (j > 0) std::cout << ", ";
                        std::cout << ccap::pixelFormatToString(info->supportedPixelFormats[j]);
                    }
                    std::cout << std::endl;
                }
            }
        }
        std::cout << std::endl;
    }

    std::cout << "Use --device-info <index> for detailed information about a specific device." << std::endl;
    return 0;
}

int showDeviceInfo(int deviceIndex) {
    ccap::Provider provider;
    auto deviceNames = provider.findDeviceNames();

    if (deviceNames.empty()) {
        std::cerr << "No camera devices found." << std::endl;
        return 1;
    }

    auto showInfo = [&](size_t idx) {
        if (idx >= deviceNames.size()) {
            std::cerr << "Device index " << idx << " out of range." << std::endl;
            return false;
        }

        const auto& name = deviceNames[idx];
        ccap::Provider devProvider(name);

        if (!devProvider.isOpened()) {
            std::cerr << "Failed to open device: " << name << std::endl;
            return false;
        }

        auto info = devProvider.getDeviceInfo();
        if (!info) {
            std::cerr << "Failed to get info for device: " << name << std::endl;
            return false;
        }

        std::cout << "\n===== Device [" << idx << "]: " << name << " =====" << std::endl;

        std::cout << "  Supported resolutions:" << std::endl;
        for (const auto& res : info->supportedResolutions) {
            std::cout << "    " << res.width << "x" << res.height << std::endl;
        }

        std::cout << "  Supported pixel formats:" << std::endl;
        for (auto fmt : info->supportedPixelFormats) {
            std::cout << "    " << ccap::pixelFormatToString(fmt) << std::endl;
        }

        std::cout << "=====================================" << std::endl;
        return true;
    };

    if (deviceIndex < 0) {
        // Show info for all devices
        for (size_t i = 0; i < deviceNames.size(); ++i) {
            showInfo(i);
        }
    } else {
        if (!showInfo(static_cast<size_t>(deviceIndex))) {
            return 1;
        }
    }

    return 0;
}

bool saveFrameToFile(ccap::VideoFrame* frame, const std::string& outputPath, bool saveAsYuv,
                     ImageFormat imageFormat, int jpegQuality) {
    if (saveAsYuv || ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatYUVColorBit)) {
        // Save as YUV file
        std::string filePath = outputPath + "." + std::string(ccap::pixelFormatToString(frame->pixelFormat)) + ".yuv";
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file for writing: " << filePath << std::endl;
            return false;
        }

        // Write Y plane
        file.write(reinterpret_cast<const char*>(frame->data[0]), frame->stride[0] * frame->height);

        // Write UV planes based on format
        if (frame->data[1]) {
            uint32_t uvHeight = frame->height / 2;
            file.write(reinterpret_cast<const char*>(frame->data[1]), frame->stride[1] * uvHeight);
        }
        if (frame->data[2]) {
            uint32_t uvHeight = frame->height / 2;
            file.write(reinterpret_cast<const char*>(frame->data[2]), frame->stride[2] * uvHeight);
        }

        std::cout << "Saved YUV to: " << filePath << std::endl;
        return true;
    }

    // Save as image
    return saveFrameAsImage(frame, outputPath, imageFormat, jpegQuality);
}

bool saveFrameAsImage(ccap::VideoFrame* frame, const std::string& outputPath,
                      ImageFormat imageFormat, int jpegQuality) {
    bool isBGR = ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatBGRBit);
    bool hasAlpha = ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatAlphaColorBit);
    bool isTopToBottom = (frame->orientation == ccap::FrameOrientation::TopToBottom);

#ifdef CCAP_CLI_WITH_STB_IMAGE
    // Use stb_image_write for JPG/PNG formats
    if (imageFormat == ImageFormat::JPG || imageFormat == ImageFormat::PNG) {
        std::string ext = (imageFormat == ImageFormat::JPG) ? ".jpg" : ".png";
        std::string filePath = outputPath + ext;
        
        // stb_image_write expects RGB data without padding
        int comp = hasAlpha ? 4 : 3;
        int width = frame->width;
        int height = frame->height;
        int stride = frame->stride[0];
        
        // Check if we need to copy/convert data
        std::vector<uint8_t> tempBuffer;
        const uint8_t* imageData = frame->data[0];
        
        // If stride != width * comp, we need to remove padding
        if (stride != width * comp) {
            tempBuffer.resize(width * height * comp);
            for (int y = 0; y < height; ++y) {
                std::memcpy(&tempBuffer[y * width * comp], 
                           &frame->data[0][y * stride], 
                           width * comp);
            }
            imageData = tempBuffer.data();
        }
        
        // stb_image_write expects top-to-bottom orientation by default
        // If frame is bottom-to-top, we need to flip
        if (!isTopToBottom) {
            if (tempBuffer.empty()) {
                tempBuffer.resize(width * height * comp);
                std::memcpy(tempBuffer.data(), frame->data[0], width * height * comp);
                imageData = tempBuffer.data();
            }
            // Flip vertically
            std::vector<uint8_t> rowBuffer(width * comp);
            for (int y = 0; y < height / 2; ++y) {
                uint8_t* top = const_cast<uint8_t*>(&imageData[y * width * comp]);
                uint8_t* bottom = const_cast<uint8_t*>(&imageData[(height - 1 - y) * width * comp]);
                std::memcpy(rowBuffer.data(), top, width * comp);
                std::memcpy(top, bottom, width * comp);
                std::memcpy(bottom, rowBuffer.data(), width * comp);
            }
        }
        
        int result = 0;
        if (imageFormat == ImageFormat::JPG) {
            // Note: stb_image_write expects RGB, but if isBGR is true, colors will be swapped
            // We should convert BGR to RGB if needed
            if (isBGR && comp >= 3) {
                std::vector<uint8_t> rgbBuffer(width * height * comp);
                if (tempBuffer.empty()) {
                    for (int i = 0; i < width * height; ++i) {
                        rgbBuffer[i * comp + 0] = imageData[i * comp + 2]; // R from B
                        rgbBuffer[i * comp + 1] = imageData[i * comp + 1]; // G
                        rgbBuffer[i * comp + 2] = imageData[i * comp + 0]; // B from R
                        if (comp == 4) {
                            rgbBuffer[i * comp + 3] = imageData[i * comp + 3]; // A
                        }
                    }
                } else {
                    for (int i = 0; i < width * height; ++i) {
                        rgbBuffer[i * comp + 0] = imageData[i * comp + 2];
                        rgbBuffer[i * comp + 1] = imageData[i * comp + 1];
                        rgbBuffer[i * comp + 2] = imageData[i * comp + 0];
                        if (comp == 4) {
                            rgbBuffer[i * comp + 3] = imageData[i * comp + 3];
                        }
                    }
                }
                result = stbi_write_jpg(filePath.c_str(), width, height, comp, rgbBuffer.data(), jpegQuality);
            } else {
                result = stbi_write_jpg(filePath.c_str(), width, height, comp, imageData, jpegQuality);
            }
        } else { // PNG
            if (isBGR && comp >= 3) {
                std::vector<uint8_t> rgbBuffer(width * height * comp);
                for (int i = 0; i < width * height; ++i) {
                    rgbBuffer[i * comp + 0] = imageData[i * comp + 2];
                    rgbBuffer[i * comp + 1] = imageData[i * comp + 1];
                    rgbBuffer[i * comp + 2] = imageData[i * comp + 0];
                    if (comp == 4) {
                        rgbBuffer[i * comp + 3] = imageData[i * comp + 3];
                    }
                }
                result = stbi_write_png(filePath.c_str(), width, height, comp, rgbBuffer.data(), width * comp);
            } else {
                result = stbi_write_png(filePath.c_str(), width, height, comp, imageData, width * comp);
            }
        }
        
        if (result) {
            std::cout << "Saved " << ext.substr(1) << " to: " << filePath << std::endl;
            return true;
        }
        std::cerr << "Failed to save " << ext.substr(1) << ": " << filePath << std::endl;
        return false;
    }
#endif

    // Fall back to BMP format
    std::string filePath = outputPath + ".bmp";
    if (ccap::saveRgbDataAsBMP(filePath.c_str(), frame->data[0], frame->width, frame->stride[0],
                               frame->height, isBGR, hasAlpha, isTopToBottom)) {
        std::cout << "Saved BMP to: " << filePath << std::endl;
        return true;
    }
    std::cerr << "Failed to save BMP: " << filePath << std::endl;
    return false;
}

int captureFrames(const CLIOptions& opts) {
    ccap::Provider provider;

    // Set capture parameters
    provider.set(ccap::PropertyName::Width, opts.width);
    provider.set(ccap::PropertyName::Height, opts.height);
    provider.set(ccap::PropertyName::FrameRate, opts.fps);

    if (opts.internalFormat != ccap::PixelFormat::Unknown) {
        provider.set(ccap::PropertyName::PixelFormatInternal, opts.internalFormat);
    }

    if (opts.outputFormat != ccap::PixelFormat::Unknown) {
        provider.set(ccap::PropertyName::PixelFormatOutput, opts.outputFormat);
    }

    // Open device
    bool opened = false;
    if (!opts.deviceName.empty()) {
        opened = provider.open(opts.deviceName, true);
    } else {
        opened = provider.open(opts.deviceIndex, true);
    }

    if (!opened || !provider.isStarted()) {
        std::cerr << "Failed to open/start camera device." << std::endl;
        return 1;
    }

    // Create output directory if specified
    if (!opts.outputDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(opts.outputDir, ec);
        if (ec) {
            std::cerr << "Failed to create output directory: " << opts.outputDir << std::endl;
            return 1;
        }
    }

    std::string outputDir = opts.outputDir.empty() ? "." : opts.outputDir;

    std::cout << "Capturing " << opts.captureCount << " frame(s)..." << std::endl;

    int capturedCount = 0;
    while (capturedCount < opts.captureCount) {
        auto frame = provider.grab(opts.timeoutMs);
        if (!frame) {
            std::cerr << "Timeout waiting for frame." << std::endl;
            break;
        }

        std::cout << "Frame " << frame->frameIndex << ": " << frame->width << "x" << frame->height
                  << " format=" << ccap::pixelFormatToString(frame->pixelFormat) << std::endl;

        // Generate output filename
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm nowTm = *std::localtime(&nowTime);
        char timestamp[64];
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &nowTm);

        std::string baseName = outputDir + "/capture_" + std::string(timestamp) + "_" +
            std::to_string(frame->width) + "x" + std::to_string(frame->height) + "_" +
            std::to_string(frame->frameIndex);

        if (!saveFrameToFile(frame.get(), baseName, opts.saveYuv, opts.imageFormat, opts.jpegQuality)) {
            std::cerr << "Failed to save frame." << std::endl;
        }

        ++capturedCount;
    }

    std::cout << "Captured " << capturedCount << " frame(s)." << std::endl;
    return 0;
}

#ifdef CCAP_CLI_WITH_GLFW

// Simple shaders for preview
static const char* previewVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 pos;
out vec2 texCoord;
void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
    texCoord = (pos / 2.0) + 0.5;
}
)";

static const char* previewFragmentShader = R"(
#version 330 core
in vec2 texCoord;
out vec4 fragColor;
uniform sampler2D tex;
void main() {
    fragColor = texture(tex, texCoord);
}
)";

int runPreview(const CLIOptions& opts) {
    ccap::Provider provider;

    // Set capture parameters
    provider.set(ccap::PropertyName::Width, opts.width);
    provider.set(ccap::PropertyName::Height, opts.height);
    provider.set(ccap::PropertyName::FrameRate, opts.fps);
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::RGBA32);
    provider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::BottomToTop);

    // Open device
    bool opened = false;
    if (!opts.deviceName.empty()) {
        opened = provider.open(opts.deviceName, true);
    } else {
        opened = provider.open(opts.deviceIndex, true);
    }

    if (!opened || !provider.isStarted()) {
        std::cerr << "Failed to open/start camera device." << std::endl;
        return 1;
    }

    // Get actual frame size
    int frameWidth = 0, frameHeight = 0;
    if (auto frame = provider.grab(5000)) {
        frameWidth = frame->width;
        frameHeight = frame->height;
        std::cout << "Camera resolution: " << frameWidth << "x" << frameHeight << std::endl;
    } else {
        std::cerr << "Failed to grab initial frame." << std::endl;
        return 1;
    }

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(frameWidth, frameHeight, "ccap Preview", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        std::cerr << "Failed to create GLFW window." << std::endl;
        return 1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to load OpenGL functions." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Create shader program
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &previewVertexShader, nullptr);
    glCompileShader(vs);

    // Check vertex shader compilation
    GLint vsSuccess = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vsSuccess);
    if (!vsSuccess) {
        char infoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(vs);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &previewFragmentShader, nullptr);
    glCompileShader(fs);

    // Check fragment shader compilation
    GLint fsSuccess = 0;
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fsSuccess);
    if (!fsSuccess) {
        char infoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(vs);
        glDeleteShader(fs);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    GLuint prog = glCreateProgram();
    glBindAttribLocation(prog, 0, "pos");
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Check program linking
    GLint progSuccess = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &progSuccess);
    if (!progSuccess) {
        char infoLog[512];
        glGetProgramInfoLog(prog, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        glDeleteProgram(prog);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Create VAO and VBO
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    const float vertData[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertData), vertData, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // Create texture
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Pre-allocate texture storage once
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    std::cout << "Preview started. Press ESC or close window to exit." << std::endl;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        if (auto frame = provider.grab(500)) {
            // Update texture data efficiently using glTexSubImage2D
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, frame->data[0]);
        }

        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        glViewport(0, 0, windowWidth, windowHeight);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glfwDestroyWindow(window);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteTextures(1, &texture);
    glfwTerminate();

    return 0;
}

#endif // CCAP_CLI_WITH_GLFW

int convertYuvToImage(const CLIOptions& opts) {
    if (opts.convertInput.empty()) {
        std::cerr << "No input file specified for conversion." << std::endl;
        return 1;
    }

    if (opts.yuvWidth <= 0 || opts.yuvHeight <= 0) {
        std::cerr << "YUV width and height must be specified." << std::endl;
        return 1;
    }

    if (opts.yuvFormat == ccap::PixelFormat::Unknown) {
        std::cerr << "YUV format must be specified." << std::endl;
        return 1;
    }

    // Read YUV file
    std::ifstream file(opts.convertInput, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open input file: " << opts.convertInput << std::endl;
        return 1;
    }

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> yuvData(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(yuvData.data()), fileSize);
    file.close();

    // Calculate expected size and pointers
    int width = opts.yuvWidth;
    int height = opts.yuvHeight;

    // Validate file size matches expected YUV format
    size_t expectedSize = 0;
    switch (opts.yuvFormat) {
    case ccap::PixelFormat::NV12:
    case ccap::PixelFormat::NV12f:
    case ccap::PixelFormat::I420:
    case ccap::PixelFormat::I420f:
        expectedSize = width * height * 3 / 2; // Y plane + UV planes (half resolution)
        break;
    case ccap::PixelFormat::YUYV:
    case ccap::PixelFormat::YUYVf:
    case ccap::PixelFormat::UYVY:
    case ccap::PixelFormat::UYVYf:
        expectedSize = width * height * 2; // Packed format with 2 bytes per pixel
        break;
    default:
        break;
    }

    if (expectedSize > 0 && static_cast<size_t>(fileSize) < expectedSize) {
        std::cerr << "File size (" << fileSize << " bytes) is smaller than expected ("
                  << expectedSize << " bytes) for " << opts.yuvWidth << "x" << opts.yuvHeight
                  << " " << ccap::pixelFormatToString(opts.yuvFormat) << std::endl;
        return 1;
    }

    uint8_t* yPtr = yuvData.data();
    uint8_t* uPtr = nullptr;
    uint8_t* vPtr = nullptr;
    int yStride = width;
    int uvStride = 0;

    bool isPlanar = false;
    bool isSemiPlanar = false;

    switch (opts.yuvFormat) {
    case ccap::PixelFormat::NV12:
    case ccap::PixelFormat::NV12f:
        isSemiPlanar = true;
        uvStride = width;
        uPtr = yPtr + width * height;
        break;
    case ccap::PixelFormat::I420:
    case ccap::PixelFormat::I420f:
        isPlanar = true;
        uvStride = width / 2;
        uPtr = yPtr + width * height;
        vPtr = uPtr + (width / 2) * (height / 2);
        break;
    case ccap::PixelFormat::YUYV:
    case ccap::PixelFormat::YUYVf:
    case ccap::PixelFormat::UYVY:
    case ccap::PixelFormat::UYVYf:
        // Packed format
        yStride = width * 2;
        break;
    default:
        std::cerr << "Unsupported YUV format for conversion." << std::endl;
        return 1;
    }

    // Allocate RGB buffer
    std::vector<uint8_t> rgbData(width * height * 3);
    int rgbStride = width * 3;

    // Determine conversion flags
    bool isFullRange = ccap::pixelFormatInclude(opts.yuvFormat, ccap::kPixelFormatFullRangeBit);
    ccap::ConvertFlag flag = isFullRange ? (ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange) : ccap::ConvertFlag::Default;

    // Perform conversion
    if (isSemiPlanar) {
        ccap::nv12ToBgr24(yPtr, yStride, uPtr, uvStride, rgbData.data(), rgbStride, width, height, flag);
    } else if (isPlanar) {
        ccap::i420ToBgr24(yPtr, yStride, uPtr, uvStride, vPtr, uvStride, rgbData.data(), rgbStride, width, height, flag);
    } else {
        // Packed formats
        if (opts.yuvFormat == ccap::PixelFormat::YUYV || opts.yuvFormat == ccap::PixelFormat::YUYVf) {
            ccap::yuyvToBgr24(yPtr, yStride, rgbData.data(), rgbStride, width, height, flag);
        } else {
            ccap::uyvyToBgr24(yPtr, yStride, rgbData.data(), rgbStride, width, height, flag);
        }
    }

    // Save as image (BMP/JPG/PNG)
    std::string outputPath = opts.convertOutput.empty() ? (opts.convertInput + "_converted") : opts.convertOutput;

    // Determine output format based on extension or use opts.imageFormat
    ImageFormat format = opts.imageFormat;
    
    // Check if file already has an image extension
    // Check longer extensions first (.jpeg) before shorter ones (.jpg, .png, .bmp)
    std::string ext;
    // Check for 5-char extension first (.jpeg)
    if (outputPath.size() >= 5) {
        std::string lastFive = outputPath.substr(outputPath.size() - 5);
        std::transform(lastFive.begin(), lastFive.end(), lastFive.begin(), ::tolower);
        if (lastFive == ".jpeg") {
            ext = lastFive;
            format = ImageFormat::JPG;
        }
    }
    // Then check for 4-char extensions (.bmp, .jpg, .png) if no 5-char match
    if (ext.empty() && outputPath.size() >= 4) {
        std::string lastFour = outputPath.substr(outputPath.size() - 4);
        std::transform(lastFour.begin(), lastFour.end(), lastFour.begin(), ::tolower);
        if (lastFour == ".bmp" || lastFour == ".jpg" || lastFour == ".png") {
            ext = lastFour;
            // Parse format from extension
            if (lastFour == ".bmp") format = ImageFormat::BMP;
            else if (lastFour == ".jpg") format = ImageFormat::JPG;
            else if (lastFour == ".png") format = ImageFormat::PNG;
        }
    }

    // Add extension if not present
    if (ext.empty()) {
        switch (format) {
            case ImageFormat::JPG:
                outputPath += ".jpg";
                break;
            case ImageFormat::PNG:
                outputPath += ".png";
                break;
            case ImageFormat::BMP:
                outputPath += ".bmp";
                break;
        }
    }

#ifdef CCAP_CLI_WITH_STB_IMAGE
    // Use stb_image_write for JPG/PNG
    if (format == ImageFormat::JPG || format == ImageFormat::PNG) {
        int result = 0;
        if (format == ImageFormat::JPG) {
            result = stbi_write_jpg(outputPath.c_str(), width, height, 3, rgbData.data(), opts.jpegQuality);
        } else { // PNG
            result = stbi_write_png(outputPath.c_str(), width, height, 3, rgbData.data(), rgbStride);
        }
        
        if (result) {
            std::cout << "Converted to: " << outputPath << std::endl;
            return 0;
        }
        std::cerr << "Failed to save converted image: " << outputPath << std::endl;
        return 1;
    }
#endif

    // Fall back to BMP format
    if (ccap::saveRgbDataAsBMP(outputPath.c_str(), rgbData.data(), width, rgbStride, height, true, false, true)) {
        std::cout << "Converted to: " << outputPath << std::endl;
        return 0;
    }

    std::cerr << "Failed to save converted image." << std::endl;
    return 1;
}

} // namespace ccap_cli

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ccap_cli::printUsage(argv[0]);
        return 0;
    }

    auto opts = ccap_cli::parseArgs(argc, argv);

    if (opts.showHelp) {
        ccap_cli::printUsage(argv[0]);
        return 0;
    }

    if (opts.showVersion) {
        ccap_cli::printVersion();
        return 0;
    }

    if (opts.verbose) {
        ccap::setLogLevel(ccap::LogLevel::Verbose);
    }

    // Set error callback
    ccap::setErrorCallback([](ccap::ErrorCode errorCode, std::string_view description) {
        std::cerr << "Camera Error - Code: " << static_cast<int>(errorCode)
                  << ", Description: " << description << std::endl;
    });

    // Handle different modes
    if (opts.listDevices) {
        return ccap_cli::listDevices();
    }

    if (opts.showDeviceInfo) {
        return ccap_cli::showDeviceInfo(opts.deviceInfoIndex);
    }

    if (!opts.convertInput.empty()) {
        return ccap_cli::convertYuvToImage(opts);
    }

#ifdef CCAP_CLI_WITH_GLFW
    if (opts.enablePreview) {
        if (opts.previewOnly) {
            return ccap_cli::runPreview(opts);
        }
        // TODO: Support preview + capture simultaneously
        return ccap_cli::runPreview(opts);
    }
#endif

    // Default: capture mode
    if (!opts.outputDir.empty() || opts.captureCount > 0) {
        return ccap_cli::captureFrames(opts);
    }

    // No specific action, show help
    ccap_cli::printUsage(argv[0]);
    return 0;
}
