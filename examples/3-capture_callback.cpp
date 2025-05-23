/**
 * @file capture_callback.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Example for ccap.
 * @date 2025-05
 *
 */

#include <ccap.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

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
    cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGR24);
    // cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGRA32);
#else
    cameraProvider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::NV12);
#endif
    cameraProvider.set(ccap::PropertyName::FrameRate, requestedFps);

    cameraProvider.open(deviceIndex, true);

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

    cameraProvider.setNewFrameCallback([=](std::shared_ptr<ccap::Frame> frame) -> bool {
        printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);
        if (auto dumpFile = ccap::dumpFrameToDirectory(frame.get(), captureDir); !dumpFile.empty())
        {
            std::cout << "Frame saved to: " << dumpFile << std::endl;
        }
        else
        {
            std::cerr << "Failed to save frame!" << std::endl;
        }
        return true; /// no need to retain the frame.
    });

    /// Wait for 5 seconds to capture frames.
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Captured 5 seconds, stopping..." << std::endl;
    return 0;
}
