/**
 * @file capture_callback.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Example for ccap.
 * @date 2025-05
 *
 */

#include "utils/helper.h"

#include <ccap.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    std::string cwd = argv[0];

    if (auto lastSlashPos = cwd.find_last_of("/\\"); lastSlashPos != std::string::npos && cwd[0] != '.') {
        cwd = cwd.substr(0, lastSlashPos);
    } else {
        cwd = std::filesystem::current_path().string();
    }

    /// Create a capture directory under the cwd directory
    std::string captureDir = cwd + "/image_capture";
    if (!std::filesystem::exists(captureDir)) {
        std::filesystem::create_directory(captureDir);
    }

    ccap::Provider cameraProvider;

    // Set error callback to receive error notifications
    ccap::setErrorCallback([](ccap::ErrorCode errorCode, std::string_view description) {
        std::cerr << "Camera Error - Code: " << static_cast<int>(errorCode)
                  << ", Description: " << description << std::endl;
    });

    if (auto deviceNames = cameraProvider.findDeviceNames(); !deviceNames.empty()) {
        for (const auto& name : deviceNames) {
            std::cout << "## Found video capture device: " << name << std::endl;
        }
    }

    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60;

    cameraProvider.set(ccap::PropertyName::Width, requestedWidth);
    cameraProvider.set(ccap::PropertyName::Height, requestedHeight);
#if 1 /// switch to test.
    cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGR24);
    // cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGRA32);
#else
    cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::NV12);
#endif
    cameraProvider.set(ccap::PropertyName::FrameRate, requestedFps);

    int deviceIndex;
    if (argc > 1 && std::isdigit(argv[1][0])) {
        deviceIndex = std::stoi(argv[1]);
    } else {
        deviceIndex = selectCamera(cameraProvider);
    }
    cameraProvider.open(deviceIndex, true);

    if (!cameraProvider.isStarted()) {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    /// Print the real resolution and fps after camera started.
    int realWidth = static_cast<int>(cameraProvider.get(ccap::PropertyName::Width));
    int realHeight = static_cast<int>(cameraProvider.get(ccap::PropertyName::Height));
    double realFps = cameraProvider.get(ccap::PropertyName::FrameRate);

    printf("Camera started successfully, requested resolution: %dx%d, real resolution: %dx%d, requested fps %g, real fps: %g\n",
           requestedWidth, requestedHeight, realWidth, realHeight, requestedFps, realFps);

    cameraProvider.setNewFrameCallback([=](const std::shared_ptr<ccap::VideoFrame>& frame) -> bool {
        printf("VideoFrame %d grabbed: width = %d, height = %d, bytes: %d\n", (int)frame->frameIndex, frame->width, frame->height,
               (int)frame->sizeInBytes);
        if (auto dumpFile = ccap::dumpFrameToDirectory(frame.get(), captureDir); !dumpFile.empty()) {
            std::cout << "VideoFrame saved to: " << dumpFile << std::endl;
        } else {
            std::cerr << "Failed to save frame!" << std::endl;
        }
        return true; /// no need to retain the frame.
    });

    /// Wait for 5 seconds to capture frames.
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Captured 5 seconds, stopping..." << std::endl;
    return 0;
}
