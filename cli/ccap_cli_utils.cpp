/**
 * @file ccap_cli_utils.cpp
 * @brief Utility functions implementation for ccap CLI tool
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 */

#include "ccap_cli_utils.h"

#include <ccap_convert.h>
#include <ccap_utils.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#ifdef CCAP_CLI_WITH_STB_IMAGE
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

#ifdef CCAP_CLI_WITH_GLFW
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace ccap_cli {

// ============================================================================
// Device Operations
// ============================================================================

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

int printVideoInfo(const std::string& videoPath) {
#if defined(CCAP_ENABLE_FILE_PLAYBACK)
    ccap::Provider provider;
    if (!provider.open(videoPath, false)) { // Don't start capture, just get info
        std::cerr << "Failed to open video file: " << videoPath << std::endl;
        return 1;
    }

    double duration = provider.get(ccap::PropertyName::Duration);
    double frameCount = provider.get(ccap::PropertyName::FrameCount);
    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    int width = static_cast<int>(provider.get(ccap::PropertyName::Width));
    int height = static_cast<int>(provider.get(ccap::PropertyName::Height));

    std::cout << "\n===== Video File Information =====" << std::endl;
    std::cout << "  File: " << videoPath << std::endl;
    std::cout << "  Resolution: " << width << "x" << height << std::endl;
    std::cout << "  Frame rate: " << frameRate << " fps" << std::endl;
    std::cout << "  Duration: " << duration << " seconds" << std::endl;
    std::cout << "  Total frames: " << static_cast<int>(frameCount) << std::endl;
    std::cout << "===================================" << std::endl;
    return 0;
#else
    std::cerr << "Video file playback is not supported. Rebuild with CCAP_ENABLE_FILE_PLAYBACK=ON" << std::endl;
    (void)videoPath;
    return 1;
#endif
}

int captureFrames(const CLIOptions& opts) {
    ccap::Provider provider;

    // Set capture parameters (only meaningful for camera mode)
    bool isVideoMode = !opts.videoFilePath.empty();
    if (!isVideoMode) {
        provider.set(ccap::PropertyName::Width, opts.width);
        provider.set(ccap::PropertyName::Height, opts.height);
        provider.set(ccap::PropertyName::FrameRate, opts.fps);

        // Internal format only applies to camera mode
        if (opts.internalFormat != ccap::PixelFormat::Unknown) {
            provider.set(ccap::PropertyName::PixelFormatInternal, opts.internalFormat);
        }
    }

    if (opts.outputFormat != ccap::PixelFormat::Unknown) {
        provider.set(ccap::PropertyName::PixelFormatOutput, opts.outputFormat);
    }

    // Open device or video file
    bool opened = false;
    if (isVideoMode) {
        // Video file playback mode
#if defined(CCAP_ENABLE_FILE_PLAYBACK)
        opened = provider.open(opts.videoFilePath, true);
        if (!opened) {
            std::cerr << "Failed to open video file: " << opts.videoFilePath << std::endl;
            std::cerr << "Make sure the file exists and is a supported video format." << std::endl;
            return 1;
        }

        if (provider.isFileMode()) {
            // Get video properties
            double duration = provider.get(ccap::PropertyName::Duration);
            double frameCount = provider.get(ccap::PropertyName::FrameCount);
            double frameRate = provider.get(ccap::PropertyName::FrameRate);
            int width = static_cast<int>(provider.get(ccap::PropertyName::Width));
            int height = static_cast<int>(provider.get(ccap::PropertyName::Height));

            // Always print video information (unless quiet mode)
            if (ccap::infoLogEnabled()) {
                std::cout << "Video file: " << opts.videoFilePath << std::endl;
                std::cout << "  Resolution: " << width << "x" << height << std::endl;
                std::cout << "  Frame rate: " << frameRate << " fps" << std::endl;
                std::cout << "  Duration: " << duration << " seconds" << std::endl;
                std::cout << "  Total frames: " << static_cast<int>(frameCount) << std::endl;
            }

            // Calculate and set playback speed
            double playbackSpeed = 0.0;
            if (opts.playbackSpeedSpecified) {
                playbackSpeed = opts.playbackSpeed;
                if (ccap::infoLogEnabled()) {
                    std::cout << "  Playback speed: " << playbackSpeed << "x" << std::endl;
                }
            } else if (opts.fpsSpecified) {
                // Calculate speed from desired fps
                if (frameRate > 0) {
                    playbackSpeed = opts.fps / frameRate;
                    if (ccap::infoLogEnabled()) {
                        std::cout << "  Calculated playback speed: " << playbackSpeed << "x (from --fps " << opts.fps << ")" << std::endl;
                    }
                } else {
                    std::cerr << "Warning: Cannot calculate playback speed, video frame rate is 0." << std::endl;
                }
            } else {
                // Default: no frame rate control (0.0)
                playbackSpeed = 0.0;
                if (ccap::infoLogEnabled()) {
                    std::cout << "  Playback speed: 0.0 (no frame rate control, process as fast as possible)" << std::endl;
                }
            }
            
            if (playbackSpeed >= 0) {
                provider.set(ccap::PropertyName::PlaybackSpeed, playbackSpeed);
            }
        }
#else
        std::cerr << "Video file playback is not supported. Rebuild with CCAP_ENABLE_FILE_PLAYBACK=ON" << std::endl;
        return 1;
#endif
    } else {
        // Camera capture mode
        if (!opts.deviceName.empty()) {
            opened = provider.open(opts.deviceName, true);
        } else {
            opened = provider.open(opts.deviceIndex, true);
        }

        if (!opened) {
            std::cerr << "Failed to open camera device." << std::endl;
            return 1;
        }
    }

    if (!opened || !provider.isStarted()) {
        std::cerr << "Failed to open/start " << (isVideoMode ? "video file" : "camera device") << "." << std::endl;
        return 1;
    }

    // Create output directory if saving frames
    bool shouldSave = opts.saveFrames && !opts.outputDir.empty();
    if (shouldSave) {
        std::error_code ec;
        std::filesystem::create_directories(opts.outputDir, ec);
        if (ec) {
            std::cerr << "Failed to create output directory: " << opts.outputDir << std::endl;
            return 1;
        }
    }

    std::string outputDir = opts.outputDir.empty() ? "." : opts.outputDir;

    // Setup timeout tracking
    auto startTime = std::chrono::steady_clock::now();
    bool timeoutOccurred = false;

    // Loop control for video playback
    int currentLoop = 0;
    int maxLoops = opts.enableLoop ? (opts.loopCount > 0 ? opts.loopCount : -1) : 1; // -1 = infinite

    std::cout << "Capturing " << (opts.captureCountSpecified ? std::to_string(opts.captureCount) + " frame(s)" : "frames") << "..." << std::endl;

    int capturedCount = 0;
    int totalCaptureLimit = opts.captureCountSpecified ? opts.captureCount : INT_MAX;

    while (capturedCount < totalCaptureLimit) {
        // Check program timeout
        if (opts.timeoutSeconds > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsedSeconds >= opts.timeoutSeconds) {
                std::cout << "Program timeout reached (" << opts.timeoutSeconds << " seconds)." << std::endl;
                timeoutOccurred = true;
                break;
            }
        }

        auto frame = provider.grab(opts.grabTimeoutMs);
        if (!frame) {
            if (isVideoMode) {
                // Video ended
                currentLoop++;
                if (maxLoops == -1 || currentLoop < maxLoops) {
                    // Seek to beginning for loop
                    provider.set(ccap::PropertyName::CurrentTime, 0.0);
                    std::cout << "Loop " << currentLoop << " completed, restarting..." << std::endl;
                    continue;
                } else {
                    std::cout << "Video playback finished." << std::endl;
                    break;
                }
            } else {
                std::cerr << "Timeout waiting for frame." << std::endl;
                break;
            }
        }

        std::cout << "Frame " << frame->frameIndex << ": " << frame->width << "x" << frame->height
                  << " format=" << ccap::pixelFormatToString(frame->pixelFormat) << std::endl;

        // Save frame if enabled
        if (shouldSave) {
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
        }

        ++capturedCount;
    }

    std::cout << "Captured " << capturedCount << " frame(s)." << std::endl;

    if (timeoutOccurred) {
        return opts.timeoutExitCode;
    }
    return 0;
}

// ============================================================================
// Format Conversion & Saving
// ============================================================================

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

        // PNG/JPG formats expect top-to-bottom scan line order (first row at memory start).
        // On Windows, video frames from Media Foundation have inverted orientation metadata:
        //   - When orientation reports BottomToTop, actual data is TopToBottom (no flip needed)
        //   - When orientation reports TopToBottom, actual data is BottomToTop (flip needed)
        if (isTopToBottom) {
            if (tempBuffer.empty()) {
                tempBuffer.resize(width * height * comp);
                // Copy with proper stride handling
                for (int y = 0; y < height; ++y) {
                    std::memcpy(&tempBuffer[y * width * comp],
                                &frame->data[0][y * stride],
                                width * comp);
                }
                imageData = tempBuffer.data();
            }
            // Flip vertically in tempBuffer
            std::vector<uint8_t> rowBuffer(width * comp);
            uint8_t* mutableData = tempBuffer.data();
            for (int y = 0; y < height / 2; ++y) {
                uint8_t* top = &mutableData[y * width * comp];
                uint8_t* bottom = &mutableData[(height - 1 - y) * width * comp];
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
            if (lastFour == ".bmp")
                format = ImageFormat::BMP;
            else if (lastFour == ".jpg")
                format = ImageFormat::JPG;
            else if (lastFour == ".png")
                format = ImageFormat::PNG;
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

// ============================================================================
// Preview Window (GLFW)
// ============================================================================

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

    // Set capture parameters (only meaningful for camera mode)
    if (opts.videoFilePath.empty()) {
        provider.set(ccap::PropertyName::Width, opts.width);
        provider.set(ccap::PropertyName::Height, opts.height);
        provider.set(ccap::PropertyName::FrameRate, opts.fps);
    }
    
    provider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::BottomToTop);
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::RGBA32);

    // Open device or video file
    bool opened = false;
    if (!opts.videoFilePath.empty()) {
        // Video file playback mode
#if defined(CCAP_ENABLE_FILE_PLAYBACK)
        opened = provider.open(opts.videoFilePath, true);
        if (!opened) {
            std::cerr << "Failed to open video file: " << opts.videoFilePath << std::endl;
            return 1;
        }
        
        // Get video properties and print information
        double videoFrameRate = provider.get(ccap::PropertyName::FrameRate);
        double duration = provider.get(ccap::PropertyName::Duration);
        double frameCount = provider.get(ccap::PropertyName::FrameCount);
        int videoWidth = static_cast<int>(provider.get(ccap::PropertyName::Width));
        int videoHeight = static_cast<int>(provider.get(ccap::PropertyName::Height));
        
        if (ccap::infoLogEnabled()) {
            std::cout << "Video file: " << opts.videoFilePath << std::endl;
            std::cout << "  Resolution: " << videoWidth << "x" << videoHeight << std::endl;
            std::cout << "  Frame rate: " << videoFrameRate << " fps" << std::endl;
            std::cout << "  Duration: " << duration << " seconds" << std::endl;
            std::cout << "  Total frames: " << static_cast<int>(frameCount) << std::endl;
        }
        
        // Calculate and set playback speed
        double playbackSpeed = 1.0; // Default for preview mode
        if (opts.playbackSpeedSpecified) {
            playbackSpeed = opts.playbackSpeed;
            if (ccap::infoLogEnabled()) {
                std::cout << "  Playback speed: " << playbackSpeed << "x" << std::endl;
            }
        } else if (opts.fpsSpecified) {
            // Calculate speed from desired fps
            if (videoFrameRate > 0) {
                playbackSpeed = opts.fps / videoFrameRate;
                if (ccap::infoLogEnabled()) {
                    std::cout << "  Calculated playback speed: " << playbackSpeed << "x (from --fps " << opts.fps << ")" << std::endl;
                }
            } else {
                std::cerr << "Warning: Cannot calculate playback speed, video frame rate is 0. Using default 1.0x." << std::endl;
            }
        } else {
            // Default 1.0 for preview mode
            if (ccap::infoLogEnabled()) {
                std::cout << "  Playback speed: 1.0x (normal speed)" << std::endl;
            }
        }
        
        provider.set(ccap::PropertyName::PlaybackSpeed, playbackSpeed);
#else
        std::cerr << "Video file playback is not supported. Rebuild with CCAP_ENABLE_FILE_PLAYBACK=ON" << std::endl;
        return 1;
#endif
    } else {
        // Camera capture mode
        if (!opts.deviceName.empty()) {
            opened = provider.open(opts.deviceName, true);
        } else {
            opened = provider.open(opts.deviceIndex, true);
        }
    }

    if (!opened || !provider.isStarted()) {
        std::cerr << "Failed to open/start " << (opts.videoFilePath.empty() ? "camera device" : "video file") << "." << std::endl;
        return 1;
    }

    // Get actual frame size
    int frameWidth = 0, frameHeight = 0;
    if (auto frame = provider.grab(5000)) {
        frameWidth = frame->width;
        frameHeight = frame->height;
        if (ccap::infoLogEnabled()) {
            std::cout << "Camera resolution: " << frameWidth << "x" << frameHeight << std::endl;
        }
    } else {
        std::cerr << "Failed to grab initial frame." << std::endl;
        return 1;
    }

    // Calculate window size - scale up if resolution is too low (below 480p)
    int windowWidth = frameWidth;
    int windowHeight = frameHeight;
    constexpr int MIN_DISPLAY_HEIGHT = 480;
    
    // Only scale up for video files, not for cameras
    if (!opts.videoFilePath.empty() && frameHeight < MIN_DISPLAY_HEIGHT) {
        double scale = static_cast<double>(MIN_DISPLAY_HEIGHT) / frameHeight;
        windowWidth = static_cast<int>(frameWidth * scale);
        windowHeight = static_cast<int>(frameHeight * scale);
        if (ccap::infoLogEnabled()) {
            std::cout << "Video resolution is below " << MIN_DISPLAY_HEIGHT << "p, scaling window to " 
                      << windowWidth << "x" << windowHeight << " (" << scale << "x)" << std::endl;
        }
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

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "ccap Preview", nullptr, nullptr);
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

    bool isVideoMode = !opts.videoFilePath.empty();
    std::string sourceType = isVideoMode ? "video file" : "camera";
    std::cout << "Preview started for " << sourceType << ". Press ESC or close window to exit." << std::endl;

    // Setup timeout and frame counting
    auto startTime = std::chrono::steady_clock::now();
    bool timeoutOccurred = false;
    int capturedCount = 0;
    int totalCaptureLimit = opts.captureCountSpecified ? opts.captureCount : INT_MAX;

    // Loop control for video playback
    int currentLoop = 0;
    int maxLoops = (isVideoMode && opts.enableLoop) ? (opts.loopCount > 0 ? opts.loopCount : -1) : 1;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }

        // Check program timeout
        if (opts.timeoutSeconds > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsedSeconds >= opts.timeoutSeconds) {
                std::cout << "Program timeout reached (" << opts.timeoutSeconds << " seconds)." << std::endl;
                timeoutOccurred = true;
                break;
            }
        }

        // Check frame count limit
        if (capturedCount >= totalCaptureLimit) {
            std::cout << "Frame count limit reached (" << opts.captureCount << " frames)." << std::endl;
            break;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        if (auto frame = provider.grab(500)) {
            // Update texture data efficiently using glTexSubImage2D
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, frame->data[0]);
            ++capturedCount;
        } else {
            // If grab fails in file mode, video has ended
            if (isVideoMode) {
                currentLoop++;
                if (maxLoops == -1 || currentLoop < maxLoops) {
                    // Seek to beginning for loop
                    provider.set(ccap::PropertyName::CurrentTime, 0.0);
                    std::cout << "Loop " << currentLoop << " completed, restarting..." << std::endl;
                    continue;
                } else {
                    std::cout << "Video playback finished." << std::endl;
                    break;
                }
            }
            // For camera mode, continue waiting for next frame
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

    if (timeoutOccurred) {
        return opts.timeoutExitCode;
    }
    return 0;
}

#endif // CCAP_CLI_WITH_GLFW

} // namespace ccap_cli
