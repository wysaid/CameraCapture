/**
 * @file capture_grab.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Demo for ccap.
 * @date 2025-05
 *
 */

#include "ccap.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    std::string cwd = argv[0];
    int deviceIndex = -1; // Indicates using the system's default camera
    if (argc > 1 && std::isdigit(argv[1][0]))
    {
        deviceIndex = std::stoi(argv[1]);
    }

    if (auto lastSlashPos = cwd.find_last_of("/\\"); lastSlashPos != std::string::npos && cwd[0] != '.')
    {
        cwd = cwd.substr(0, lastSlashPos);
    }
    else
    {
        cwd = std::filesystem::current_path().string();
    }

    /// 在 cwd 目录下创建一个 capture 目录
    std::string captureDir = cwd + "/image_capture";
    if (!std::filesystem::exists(captureDir))
    {
        std::filesystem::create_directory(captureDir);
    }

    ccap::Provider cameraProvider;

    if (auto deviceNames = cameraProvider.findDeviceNames(); !deviceNames.empty())
    {
        for (const auto& name : deviceNames)
        {
            std::cout << "## Found video capture device: " << name << std::endl;
        }
    }

    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60;

    cameraProvider.set(ccap::PropertyName::Width, requestedWidth);
    cameraProvider.set(ccap::PropertyName::Height, requestedHeight);
#if 1 /// switch to test.
    cameraProvider.set(ccap::PropertyName::PixelFormat, ccap::PixelFormat::BGR24);
    // cameraProvider.set(ccap::PropertyName::PixelFormat, ccap::PixelFormat::BGRA32);
#else
    cameraProvider.set(ccap::PropertyName::PixelFormat, ccap::PixelFormat::NV12f);
#endif
    cameraProvider.set(ccap::PropertyName::FrameRate, requestedFps);

    // cameraProvider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::BottomToTop);

    cameraProvider.open(deviceIndex) && cameraProvider.start();

    if (!cameraProvider.isStarted())
    {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    /// Print the real resolution and fps after camera started.
    int realWidth = static_cast<int>(cameraProvider.get(ccap::PropertyName::Width));
    int realHeight = static_cast<int>(cameraProvider.get(ccap::PropertyName::Height));
    double realFps = cameraProvider.get(ccap::PropertyName::FrameRate);

    printf("Camera started successfully, requested resolution: %dx%d, real resolution: %dx%d, requested fps %g, real fps: %g\n", requestedWidth, requestedHeight, realWidth, realHeight, requestedFps, realFps);

    /// 3000 ms timeout when grabbing frames
    while (auto frame = cameraProvider.grab(3000))
    {
        printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);
        if (auto dumpFile = ccap::dumpFrameToDirectory(frame.get(), captureDir); !dumpFile.empty())
        {
            std::cout << "Frame saved to: " << dumpFile << std::endl;
        }
        else
        {
            std::cerr << "Failed to save frame!" << std::endl;
        }

        if (frame->frameIndex >= 10)
        {
            frame = nullptr;
            std::cout << "Captured 10 frames, stopping..." << std::endl;
            break;
        }
    }

    return 0;
}
