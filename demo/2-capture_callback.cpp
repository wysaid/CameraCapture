/**
 * @file capture_demo2.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Demo for ccap.
 * @date 2025-05
 *
 */

#include "CameraCapture.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

void saveRgbaDataAsBMP(const unsigned char* data, const char* filename, uint32_t w, uint32_t stride, uint32_t h, bool isBGRA);

int main(int argc, char** argv)
{
    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    std::string cwd = argv[0];

    if (auto lastSlashPos = cwd.find_last_of("/\\"); lastSlashPos != std::string::npos)
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

    auto cameraProvider = ccap::createProvider();
    if (!cameraProvider)
    {
        std::cerr << "Failed to create provider!" << std::endl;
        return -1;
    }

    int requestedWidth = 1280;
    int requestedHeight = 720;
    double requestedFps = 60;

    cameraProvider->set(ccap::PropertyName::Width, requestedWidth);
    cameraProvider->set(ccap::PropertyName::Height, requestedHeight);
#if 1 /// switch to test.
    cameraProvider->set(ccap::PropertyName::PixelFormat, ccap::PixelFormat::BGRA8888);
#else
    cameraProvider->set(ccap::PropertyName::PixelFormat, ccap::PixelFormat::NV12f);
#endif
    cameraProvider->set(ccap::PropertyName::FrameRate, requestedFps);

    cameraProvider->open() && cameraProvider->start();

    if (!cameraProvider->isStarted())
    {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    /// Print the real resolution and fps after camera started.
    int realWidth = static_cast<int>(cameraProvider->get(ccap::PropertyName::Width));
    int realHeight = static_cast<int>(cameraProvider->get(ccap::PropertyName::Height));
    double realFps = cameraProvider->get(ccap::PropertyName::FrameRate);

    printf("Camera started successfully, requested resolution: %dx%d, real resolution: %dx%d, requested fps %g, real fps: %g\n", requestedWidth, requestedHeight, realWidth, realHeight, requestedFps, realFps);

    int imageIndex = 0;
    auto getNewFilePath = [&]() {
        /// 创建一个基于当前时间的文件名
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm nowTm = *std::localtime(&nowTime);
        char filename[256];
        std::strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S", &nowTm);
        return captureDir + "/" + filename + "_" + std::to_string(imageIndex++);
    };

    cameraProvider->setNewFrameCallback([getNewFilePath](std::shared_ptr<ccap::Frame> frame) -> bool {
        printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);

        if (ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatRGBAColorBit))
        {
            auto filePath = getNewFilePath() + ".bmp";
            saveRgbaDataAsBMP(frame->data[0], filePath.c_str(), frame->width, frame->stride[0], frame->height, false);
            std::cout << "Saving frame to: " << filePath << std::endl;
        }
        else if (ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatYUVColorBit))
        {
            auto filePath = getNewFilePath() + ".yuv";
            FILE* fp = fopen(filePath.c_str(), "wb");
            if (fp)
            {
                fwrite(frame->data[0], frame->stride[0], frame->height, fp);
                fwrite(frame->data[1], frame->stride[1], frame->height / 2, fp);
                if (frame->data[2] != nullptr)
                {
                    fwrite(frame->data[2], frame->stride[2], frame->height / 2, fp);
                }
                fclose(fp);
                std::cout << "Saving frame to: " << filePath << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing: " << filePath << std::endl;
            }
        }
        return true; /// no need to retain the frame.
    });

    /// Wait for 10 seconds to capture frames.
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Captured 10 seconds, stopping..." << std::endl;
    cameraProvider = nullptr;
    return 0;
}

void saveRgbaDataAsBMP(const unsigned char* data, const char* filename, uint32_t w, uint32_t stride, uint32_t h, bool isBGRA)
{
    FILE* fp = fopen(filename, "wb");
    if (fp == nullptr)
        return;
    unsigned char file[14] = {
        'B', 'M',   // magic
        0, 0, 0, 0, // <NEED FILL> all size in bytes
        0, 0,       // app data
        0, 0,       // app data
        0, 0, 0, 0  // <NEED FILL> start of image data offset
    };
    // BITMAPV4HEADER
    unsigned char info[108] = {
        108, 0, 0, 0,           // DIB header size
        0, 0, 0, 0,             // <NEED FILL> width
        0, 0, 0, 0,             // <NEED FILL> height
        1, 0,                   // number color planes
        32, 0,                  // bits per pixel
        3, 0, 0, 0,             // BI_BITFIELDS, means RGBA
        0, 0, 0, 0,             // <NEED FILL> image data size by bytes
        0x13, 0x0B, 0, 0,       // horiz resolution in pixel / m
        0x13, 0x0B, 0, 0,       // vert resolutions (0x03C3 = 96 dpi, 0x0B13 = 72 dpi)
        0, 0, 0, 0,             // #colors in palette
        0, 0, 0, 0,             // #important colors
        0x00, 0x00, 0xFF, 0x00, // Red channel bit mask (valid because BI_BITFIELDS is specified)
        0x00, 0xFF, 0x00, 0x00, // Green channel bit mask (valid because BI_BITFIELDS is specified)
        0xFF, 0x00, 0x00, 0x00, // Blue channel bit mask (valid because BI_BITFIELDS is specified)
        0x00, 0x00, 0x00, 0xFF, // Alpha channel bit mask
        // 0x20, 0x6E, 0x69, 0x57, // "Win ", LCS_WINDOWS_COLOR_SPACE, not working on Macos.
        // ... unused bytes
    };

    if (!isBGRA)
    {
        (uint32_t&)info[40] = 0x00FF0000; // Red channel bit mask
        (uint32_t&)info[44] = 0x0000FF00; // Green channel bit mask
        (uint32_t&)info[48] = 0x000000FF; // Blue channel bit mask
    }

    int lineSize = w * 4;
    int sizeData = (lineSize * h);

    (uint32_t&)file[2] = sizeof(file) + sizeof(info) + sizeData;
    (uint32_t&)file[10] = sizeof(file) + sizeof(info);
    (uint32_t&)info[4] = w;
    (uint32_t&)info[8] = h;
    (uint32_t&)info[20] = sizeData;

    fwrite(file, sizeof(file), 1, fp);
    fwrite(info, sizeof(info), 1, fp);

    if (stride == w)
    {
        fwrite(data, lineSize, h, fp);
    }
    else
    {
        for (uint32_t i = 0; i < h; ++i)
        {
            fwrite(data + stride * i, lineSize, 1, fp);
        }
    }

    fclose(fp);
}
