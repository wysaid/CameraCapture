/**
 * @file ccap_utils.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Some utility functions for ccap.
 * @date 2025-05
 *
 */

#include "ccap_utils.h"

#include "ccap_imp.h"

#include <chrono>

namespace ccap
{
std::string dumpFrameToDirectory(Frame* frame, std::string_view directory)
{
    // Create a filename based on current time
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm nowTm = *std::localtime(&nowTime);
    char filename[256];
    std::strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S", &nowTm);
    return dumpFrameToFile(frame, std::string(directory) + '/' + filename + '_' + std::to_string(frame->width) + 'x' + std::to_string(frame->height) + '_' + std::to_string(frame->frameIndex));
}

bool saveRgbDataAsBMP(const char* filename, const unsigned char* data, uint32_t w, uint32_t stride, uint32_t h, bool isBGR, bool hasAlpha, bool isTopToBottom)
{
    FILE* fp = fopen(filename, "wb");
    if (fp == nullptr)
        return false;

    unsigned char file[14] = {
        'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    int srcLineOffset = static_cast<int>(stride);

    if (hasAlpha)
    {
        // 32bpp, BITMAPV4HEADER
        unsigned char info[108] = {
            108, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 32, 0, 3, 0, 0, 0, 0, 0, 0, 0,
            0x13, 0x0B, 0, 0, 0x13, 0x0B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF
        };
        auto lineSize = w * 4;
        auto sizeData = lineSize * h;
        (uint32_t&)file[2] = sizeof(file) + sizeof(info) + sizeData;
        (uint32_t&)file[10] = sizeof(file) + sizeof(info);
        (uint32_t&)info[4] = w;
        (uint32_t&)info[8] = h;
        (uint32_t&)info[20] = sizeData;
        fwrite(file, sizeof(file), 1, fp);
        fwrite(info, sizeof(info), 1, fp);

        if (isTopToBottom)
        {
            data += srcLineOffset * (h - 1);
            srcLineOffset = -srcLineOffset;
        }

        if (isBGR)
        {
            for (uint32_t i = 0; i < h; ++i)
            {
                fwrite(data, lineSize, 1, fp);
                data += srcLineOffset;
            }
        }
        else
        {
            // Swap R and B channels, write as BGRA
            std::vector<unsigned char> line(lineSize);
            for (uint32_t i = 0; i < h; ++i)
            {
                for (uint32_t x = 0; x < w; ++x)
                {
                    const auto offset = x * 4;
                    line[offset + 0] = data[offset + 2]; // B
                    line[offset + 1] = data[offset + 1]; // G
                    line[offset + 2] = data[offset + 0]; // R
                    line[offset + 3] = data[offset + 3]; // A
                }
                fwrite(line.data(), lineSize, 1, fp);
                data += srcLineOffset;
            }
        }
    }
    else
    {
        // 24bpp, BITMAPINFOHEADER
        unsigned char info[40] = {
            40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0x13, 0x0B, 0, 0, 0x13, 0x0B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        auto lineSize = ((w * 3 + 3) / 4) * 4; // 4 bytes aligned
        auto sizeData = lineSize * h;
        (uint32_t&)file[2] = sizeof(file) + sizeof(info) + sizeData;
        (uint32_t&)file[10] = sizeof(file) + sizeof(info);
        (uint32_t&)info[4] = w;
        (uint32_t&)info[8] = h;
        (uint32_t&)info[20] = sizeData;
        fwrite(file, sizeof(file), 1, fp);
        fwrite(info, sizeof(info), 1, fp);

        if (isTopToBottom)
        {
            data += srcLineOffset * (h - 1);
            srcLineOffset = -srcLineOffset;
        }

        if (isBGR)
        {
            unsigned char padding[3] = { 0, 0, 0 };
            auto remainBytes = lineSize - w * 3;
            for (uint32_t i = 0; i < h; ++i)
            {
                fwrite(data, w * 3, 1, fp);
                if (remainBytes > 0)
                    fwrite(padding, 1, remainBytes, fp);
                data += srcLineOffset;
            }
        }
        else
        {
            std::vector<unsigned char> line(lineSize);
            for (uint32_t i = 0; i < h; ++i)
            {
                auto* d = line.data();
                // RGB to BGR
                for (uint32_t x = 0; x < w; ++x)
                {
                    const int index = x * 3;
                    d[index] = data[index + 2];     // B
                    d[index + 1] = data[index + 1]; // G
                    d[index + 2] = data[index + 0]; // R
                }
                fwrite(line.data(), lineSize, 1, fp);
                data += srcLineOffset;
            }
        }
    }
    fclose(fp);
    return true;
}

std::string dumpFrameToFile(Frame* frame, std::string_view fileNameWithNoSuffix)
{
    if (frame->pixelFormat & ccap::kPixelFormatRGBColorBit)
    { /// RGB or RGBA
        auto filePath = std::string(fileNameWithNoSuffix) + ".bmp";
        bool isBGR = (frame->pixelFormat & ccap::kPixelFormatBGRBit) != 0;

        saveRgbDataAsBMP(filePath.c_str(), frame->data[0], frame->width, frame->stride[0], frame->height, isBGR, frame->pixelFormat & ccap::kPixelFormatAlphaColorBit, frame->orientation == FrameOrientation::TopToBottom);
        return filePath;
    }
    else if (ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatYUVColorBit))
    {
        auto filePath = std::string(fileNameWithNoSuffix) + '.' + pixelFormatToString(frame->pixelFormat).data() + ".yuv";
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

std::string_view pixelFormatToString(PixelFormat format)
{
    switch (format)
    {
    case PixelFormat::NV12:
        return "NV12";
    case PixelFormat::NV12f:
        return "NV12f";

    case PixelFormat::I420:
        return "I420";
    case PixelFormat::I420f:
        return "I420f";

    case PixelFormat::RGB24:
        return "RGB24";
    case PixelFormat::RGBA32:
        return "RGBA32";

    case PixelFormat::BGR24:
        return "BGR24";
    case PixelFormat::BGRA32:
        return "BGRA32";
    default:
        break;
    }
    return "Unknown";
}

#if _CCAP_LOG_ENABLED_
#ifdef DEBUG
LogLevel globalLogLevel = LogLevel::Info;
bool globalLogLevelChanged = false;
#else
LogLevel globalLogLevel = LogLevel::Error;
#endif
#endif

void setLogLevel(LogLevel level)
{
#if _CCAP_LOG_ENABLED_
    globalLogLevel = level;
#ifdef DEBUG
    globalLogLevelChanged = true;
#endif
#else
    (void)level;
    fprintf(stderr, "ccap: Log is not enabled in this build.\n");
#endif
}

} // namespace ccap
