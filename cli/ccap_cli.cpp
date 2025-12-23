/**
 * @file ccap_cli.cpp
 * @brief CLI tool for CameraCapture (ccap) library
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 *
 * A command-line interface for camera capture operations, inspired by ffmpeg and imagemagick.
 * Supports camera enumeration, capture, format conversion, and optional window preview.
 */

#include "ccap_cli.h"

#include <ccap.h>
#include <ccap_convert.h>

#include <algorithm>
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

#ifdef CCAP_CLI_WITH_GLFW
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace ccap_cli {

// Version info
constexpr const char* CLI_VERSION = "1.0.0";

// Default values
constexpr int DEFAULT_WIDTH = 1280;
constexpr int DEFAULT_HEIGHT = 720;
constexpr double DEFAULT_FPS = 30.0;
constexpr int DEFAULT_CAPTURE_COUNT = 1;
constexpr int DEFAULT_TIMEOUT_MS = 5000;

void printVersion() {
    std::cout << "ccap CLI version " << CLI_VERSION << std::endl;
    std::cout << "Built with ccap library version " << CCAP_VERSION_STRING << std::endl;
#ifdef CCAP_CLI_WITH_WUFFS
    std::cout << "Image formats: BMP (wuffs available for future formats)" << std::endl;
#else
    std::cout << "Image formats: BMP only" << std::endl;
#endif
#ifdef CCAP_CLI_WITH_GLFW
    std::cout << "Window preview: enabled (GLFW)" << std::endl;
#else
    std::cout << "Window preview: disabled" << std::endl;
#endif
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "CameraCapture CLI Tool - A command-line interface for camera operations.\n\n";

    std::cout << "General Options:\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "  -v, --version              Show version information\n";
    std::cout << "  --verbose                  Enable verbose logging\n\n";

    std::cout << "Device Enumeration:\n";
    std::cout << "  -l, --list-devices         List all available camera devices\n";
    std::cout << "  -i, --device-info [INDEX]  Show detailed info for device at INDEX (default: all devices)\n\n";

    std::cout << "Capture Options:\n";
    std::cout << "  -d, --device INDEX|NAME    Select camera device by index or name (default: 0)\n";
    std::cout << "  -w, --width WIDTH          Set capture width (default: " << DEFAULT_WIDTH << ")\n";
    std::cout << "  -H, --height HEIGHT        Set capture height (default: " << DEFAULT_HEIGHT << ")\n";
    std::cout << "  -f, --fps FPS              Set frame rate (default: " << DEFAULT_FPS << ")\n";
    std::cout << "  -c, --count COUNT          Number of frames to capture (default: " << DEFAULT_CAPTURE_COUNT << ")\n";
    std::cout << "  -t, --timeout MS           Capture timeout in milliseconds (default: " << DEFAULT_TIMEOUT_MS << ")\n";
    std::cout << "  -o, --output DIR           Output directory for captured images\n";
    std::cout << "  --format FORMAT            Output pixel format: rgb24, bgr24, rgba32, bgra32, nv12, i420, yuyv, uyvy\n";
    std::cout << "  --save-yuv                 Save YUV frames directly without conversion\n\n";

#ifdef CCAP_CLI_WITH_GLFW
    std::cout << "Preview Options:\n";
    std::cout << "  -p, --preview              Enable window preview\n";
    std::cout << "  --preview-only             Preview without saving frames\n\n";
#endif

    std::cout << "Format Conversion:\n";
    std::cout << "  --convert INPUT            Convert YUV file to image\n";
    std::cout << "  --yuv-format FORMAT        YUV format: nv12, nv12f, i420, i420f, yuyv, yuyvf, uyvy, uyvyf\n";
    std::cout << "  --yuv-width WIDTH          Width of YUV input\n";
    std::cout << "  --yuv-height HEIGHT        Height of YUV input\n";
    std::cout << "  --convert-output FILE      Output file for conversion\n\n";

    std::cout << "Examples:\n";
    std::cout << "  " << programName << " --list-devices\n";
    std::cout << "  " << programName << " --device-info 0\n";
    std::cout << "  " << programName << " -d 0 -w 1920 -H 1080 -c 10 -o ./captures\n";
#ifdef CCAP_CLI_WITH_GLFW
    std::cout << "  " << programName << " -d 0 --preview\n";
#endif
    std::cout << "  " << programName << " --convert input.yuv --yuv-format nv12 --yuv-width 1920 --yuv-height 1080 --convert-output output.bmp\n";
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
        } else if (arg == "--save-yuv") {
            opts.saveYuv = true;
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
            std::cerr << "Unknown option: " << arg << std::endl;
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

    std::cout << "Found " << deviceNames.size() << " camera device(s):" << std::endl;
    for (size_t i = 0; i < deviceNames.size(); ++i) {
        std::cout << "  [" << i << "] " << deviceNames[i] << std::endl;
    }

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

bool saveFrameToFile(ccap::VideoFrame* frame, const std::string& outputPath, bool saveAsYuv) {
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
    return saveFrameAsImage(frame, outputPath);
}

bool saveFrameAsImage(ccap::VideoFrame* frame, const std::string& outputPath) {
#ifdef CCAP_CLI_WITH_WUFFS
    // Use wuffs for more image formats
    return saveFrameWithWuffs(frame, outputPath);
#else
    // Use built-in BMP saving
    std::string filePath = outputPath + ".bmp";
    bool isBGR = ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatBGRBit);
    bool hasAlpha = ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatAlphaColorBit);
    bool isTopToBottom = (frame->orientation == ccap::FrameOrientation::TopToBottom);

    if (ccap::saveRgbDataAsBMP(filePath.c_str(), frame->data[0], frame->width, frame->stride[0],
                               frame->height, isBGR, hasAlpha, isTopToBottom)) {
        std::cout << "Saved BMP to: " << filePath << std::endl;
        return true;
    }
    std::cerr << "Failed to save BMP: " << filePath << std::endl;
    return false;
#endif
}

int captureFrames(const CLIOptions& opts) {
    ccap::Provider provider;

    // Set capture parameters
    provider.set(ccap::PropertyName::Width, opts.width);
    provider.set(ccap::PropertyName::Height, opts.height);
    provider.set(ccap::PropertyName::FrameRate, opts.fps);

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

        if (!saveFrameToFile(frame.get(), baseName, opts.saveYuv)) {
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
    gladLoadGL(glfwGetProcAddress);

    // Create shader program
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &previewVertexShader, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &previewFragmentShader, nullptr);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glBindAttribLocation(prog, 0, "pos");
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Create VAO and VBO
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    const float vertData[8] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
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

        if (auto frame = provider.grab(30)) {
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
    ccap::ConvertFlag flag = isFullRange ? (ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange)
                                         : ccap::ConvertFlag::Default;

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

    // Save as BMP
    std::string outputPath = opts.convertOutput.empty() ? (opts.convertInput + "_converted") : opts.convertOutput;
    if (outputPath.size() < 4 || outputPath.substr(outputPath.size() - 4) != ".bmp") {
        outputPath += ".bmp";
    }

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
