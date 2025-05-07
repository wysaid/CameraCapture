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

void saveRgbDataAsBMP(const unsigned char* data, const char* filename, uint32_t w, uint32_t stride, uint32_t h, bool isBGR, bool hasAlpha);

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

        if (frame->pixelFormat & ccap::kPixelFormatRGBColorBit)
        { /// RGB or RGBA
            auto filePath = getNewFilePath() + ".bmp";
            saveRgbDataAsBMP(frame->data[0], filePath.c_str(), frame->width, frame->stride[0], frame->height, true, frame->pixelFormat & ccap::kPixelFormatAlphaColorBit);
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

/// @brief  Save RGB data as BMP file.
/// @param isBGR indicate if the data is in B-G-R bytes order. if true, the data is in B-G-R order, else it is in R-G-B order.
/// @param hasAlpha indicate if the data has an alpha channel. The alpha channel is always at the end of the pixel byte order.
void saveRgbDataAsBMP(const unsigned char* data, const char* filename, uint32_t w, uint32_t stride, uint32_t h, bool isBGR, bool hasAlpha)
{
    FILE* fp = fopen(filename, "wb");
    if (fp == nullptr)
        return;

    if (hasAlpha)
    {
        // 32bpp, BITMAPV4HEADER
        unsigned char file[14] = {
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        unsigned char info[108] = {
            108, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 32, 0, 3, 0, 0, 0, 0, 0, 0, 0,
            0x13, 0x0B, 0, 0, 0x13, 0x0B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF
        };
        if (!isBGR)
        {
            (uint32_t&)info[40] = 0x000000FF;
            (uint32_t&)info[48] = 0x00FF0000;
        }
        int lineSize = w * 4;
        int sizeData = lineSize * h;
        (uint32_t&)file[2] = sizeof(file) + sizeof(info) + sizeData;
        (uint32_t&)file[10] = sizeof(file) + sizeof(info);
        (uint32_t&)info[4] = w;
        (uint32_t&)info[8] = h;
        (uint32_t&)info[20] = sizeData;
        fwrite(file, sizeof(file), 1, fp);
        fwrite(info, sizeof(info), 1, fp);
        for (int i = h - 1; i >= 0; --i)
            fwrite(data + stride * i, lineSize, 1, fp);
    }
    else
    {
        // 24bpp, BITMAPINFOHEADER
        unsigned char file[14] = {
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        unsigned char info[40] = {
            40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x13, 0x0B, 0, 0, 0x13, 0x0B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        int lineSize = ((w * 3 + 3) / 4) * 4; // 4字节对齐
        int sizeData = lineSize * h;
        (uint32_t&)file[2] = sizeof(file) + sizeof(info) + sizeData;
        (uint32_t&)file[10] = sizeof(file) + sizeof(info);
        (uint32_t&)info[4] = w;
        (uint32_t&)info[8] = h;
        (uint32_t&)info[20] = sizeData;
        fwrite(file, sizeof(file), 1, fp);
        fwrite(info, sizeof(info), 1, fp);
        unsigned char padding[3] = { 0, 0, 0 };
        for (int i = h - 1; i >= 0; --i)
        {
            const unsigned char* src = data + stride * i;
            if (isBGR)
            {
                fwrite(src, w * 3, 1, fp);
            }
            else
            {
                // RGB转BGR
                for (uint32_t x = 0; x < w; ++x)
                {
                    unsigned char bgr[3] = { src[x * 3 + 2], src[x * 3 + 1], src[x * 3 + 0] };
                    fwrite(bgr, 3, 1, fp);
                }
            }
            auto remainBytes = lineSize - w * 3;
            if (remainBytes > 0)
                fwrite(padding, 1, lineSize - w * 3, fp);
        }
    }
    fclose(fp);
}