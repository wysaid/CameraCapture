/**
 * @file capture_grab.cpp
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

int selectCamera(ccap::Provider& provider)
{
    if (auto names = provider.findDeviceNames(); names.size() > 1) {
        std::cout << "Multiple devices found, please select one:" << std::endl;
        for (size_t i = 0; i < names.size(); ++i) {
            std::cout << "  " << i << ": " << names[i] << std::endl;
        }
        int selectedIndex;
        std::cout << "Enter the index of the device you want to use: ";
        std::cin >> selectedIndex;
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(names.size())) {
            selectedIndex = 0;
            std::cerr << "Invalid index, using the first device:" << names[0] << std::endl;
        }
        else {
            std::cout << "Using device: " << names[selectedIndex] << std::endl;
        }
        return selectedIndex;
    }

    return -1; // One or no device, use default.
}

int main(int argc, char** argv)
{
    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    std::string cwd = argv[0];

    if (auto lastSlashPos = cwd.find_last_of("/\\"); lastSlashPos != std::string::npos && cwd[0] != '.') {
        cwd = cwd.substr(0, lastSlashPos);
    }
    else {
        cwd = std::filesystem::current_path().string();
    }

    /// 在 cwd 目录下创建一个 capture 目录
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
    }
    else {
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
        printf("VideoFrame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height,
               frame->sizeInBytes);
        if (auto dumpFile = ccap::dumpFrameToDirectory(frame.get(), captureDir); !dumpFile.empty()) {
            std::cout << "VideoFrame saved to: " << dumpFile << std::endl;
        }
        else {
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
