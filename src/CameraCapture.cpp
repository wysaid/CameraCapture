/**
 * @file CameraCapture.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#include "CameraCapture.h"

#include "CameraCaptureImp.h"
#include "CameraCaptureMac.h"
#include "CameraCaptureWin.h"

#include <chrono>
#include <iostream>

namespace ccap
{
Allocator::~Allocator() = default;

Frame::Frame() = default;
Frame::~Frame() = default;

Provider::Provider(ccap::ProviderImp* imp) :
    m_imp(imp)
{
}

std::vector<std::string> Provider::findDeviceNames()
{
    return m_imp->findDeviceNames();
}

bool Provider::open(std::string_view deviceName)
{
    return m_imp->open(deviceName);
}

bool Provider::open(int deviceIndex)
{
    std::string deviceName;
    if (deviceIndex >= 0)
    {
        auto deviceNames = findDeviceNames();
        if (!deviceNames.empty())
        {
            deviceIndex = std::min(deviceIndex, static_cast<int>(deviceNames.size()) - 1);
            deviceName = deviceNames[deviceIndex];

            if (ccap::verboseLogEnabled())
            {
                printf("ccap: input device index %d, selected device name: %s\n", deviceIndex, deviceName.c_str());
            }
        }
    }

    return open(deviceName);
}

bool Provider::isOpened() const
{
    return m_imp->isOpened();
}

void Provider::close()
{
    m_imp->close();
}

bool Provider::start()
{
    return m_imp->start();
}

void Provider::stop()
{
    m_imp->stop();
}

bool Provider::isStarted() const
{
    return m_imp->isStarted();
}

bool Provider::set(PropertyName prop, double value)
{
    return m_imp->set(prop, value);
}

double Provider::get(PropertyName prop)
{
    return m_imp->get(prop);
}

std::shared_ptr<Frame> Provider::grab(bool waitForNewFrame)
{
    return m_imp->grab(waitForNewFrame);
}

void Provider::setNewFrameCallback(std::function<bool(std::shared_ptr<Frame>)> callback)
{
    m_imp->setNewFrameCallback(std::move(callback));
}

void Provider::setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory)
{
    m_imp->setFrameAllocator(std::move(allocatorFactory));
}

void Provider::setMaxAvailableFrameSize(uint32_t size)
{
    m_imp->setMaxAvailableFrameSize(size);
}

void Provider::setMaxCacheFrameSize(uint32_t size)
{
    m_imp->setMaxCacheFrameSize(size);
}

Provider::~Provider() = default;

Provider* createProvider()
{
#if __APPLE__
    return new Provider(new ProviderMac());
#elif defined(_MSC_VER) || defined(_WIN32)
    return new Provider(new ProviderWin());
#endif

    if (warningLogEnabled())
    {
        std::cerr << "Unsupported platform!" << std::endl;
    }
    return nullptr;
}

std::string dumpFrameToFile(Frame* frame, std::string_view fileNameWithNoSuffix)
{
    if (frame->pixelFormat & ccap::kPixelFormatRGBColorBit)
    { /// RGB or RGBA
        auto filePath = std::string(fileNameWithNoSuffix) + ".bmp";
        saveRgbDataAsBMP(filePath.c_str(), frame->data[0], frame->width, frame->stride[0], frame->height, true, frame->pixelFormat & ccap::kPixelFormatAlphaColorBit);
        return filePath;
    }
    else if (ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatYUVColorBit))
    {
        auto filePath = std::string(fileNameWithNoSuffix) + ".yuv";
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
            return filePath;
        }
    }
    return {};
}

std::string dumpFrameToDirectory(Frame* frame, std::string_view directory)
{
    /// 创建一个基于当前时间的文件名
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm nowTm = *std::localtime(&nowTime);
    char filename[256];
    std::strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S", &nowTm);
    return dumpFrameToFile(frame, std::string(directory) + '/' + filename + '_' + std::to_string(frame->width) + 'x' + std::to_string(frame->height) + '_' + std::to_string(frame->frameIndex));
}

bool saveRgbDataAsBMP(const char* filename, const unsigned char* data, uint32_t w, uint32_t stride, uint32_t h, bool isBGR, bool hasAlpha)
{
    FILE* fp = fopen(filename, "wb");
    if (fp == nullptr)
        return false;

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
        for (uint32_t i = 0; i < h; ++i)
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
        for (uint32_t i = 0; i < h; ++i)
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
    return true;
}

#if _CCAP_LOG_ENABLED_
LogLevel globalLogLevel = LogLevel::Info;
#endif

void setLogLevel(LogLevel level)
{
#if _CCAP_LOG_ENABLED_
    globalLogLevel = level;
#else
    (void)level;
    std::cerr << "ccap: Log is not enabled in this build." << std::endl;
#endif
}

} // namespace ccap