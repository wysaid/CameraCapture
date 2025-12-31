/**
 * @file capture_grab.cpp
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

int main(int argc, char** argv) {
    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    // Set error callback to receive error notifications
    ccap::setErrorCallback([](ccap::ErrorCode errorCode, std::string_view description) {
        std::cerr << "Camera Error - Code: " << static_cast<int>(errorCode)
                  << ", Description: " << description << std::endl;
    });

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

    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60;

    cameraProvider.set(ccap::PropertyName::Width, requestedWidth);
    cameraProvider.set(ccap::PropertyName::Height, requestedHeight);
#if 1 /// switch to test.
    cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::RGB24);
    // cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::RGBA32);
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

    /// 3000 ms timeout when grabbing frames
    while (auto frame = cameraProvider.grab(3000)) {
        printf("VideoFrame %d grabbed: width = %d, height = %d, bytes: %d\n", (int)frame->frameIndex, frame->width, frame->height,
               (int)frame->sizeInBytes);
        if (auto dumpFile = ccap::dumpFrameToDirectory(frame.get(), captureDir); !dumpFile.empty()) {
            std::cout << "VideoFrame saved to: " << dumpFile << std::endl;
        } else {
            std::cerr << "Failed to save frame!" << std::endl;
        }

        if (frame->frameIndex >= 10) {
            frame = nullptr;
            std::cout << "Captured 10 frames, stopping..." << std::endl;
            break;
        }
    }

    return 0;
}
