/**
 * @file CameraCapture.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#include "CameraCapture.h"

#include "CameraCaptureMac.h"

#include <iostream>

namespace ccap
{
Allocator::~Allocator() = default;
DefaultAllocator::~DefaultAllocator() = default;
LogLevel globalLogLevel = LogLevel::Info;

void DefaultAllocator::resize(size_t size)
{
    m_data.resize(size);
}

uint8_t* DefaultAllocator::data()
{
    return m_data.data();
}

size_t DefaultAllocator::size()
{
    return m_data.size();
}

Frame::Frame() = default;
Frame::~Frame() = default;

Provider::~Provider() = default;

Provider* createProvider()
{
#if __APPLE__
    return new ProviderMac();
#elif defined(_MSC_VER) || defined(_WIN32)
#endif

    if (static_cast<uint32_t>(globalLogLevel) & static_cast<uint32_t>(LogLevel::Warning))
    {
        std::cerr << "Unsupported platform!" << std::endl;
    }
    return nullptr;
}

void setLogLevel(LogLevel level)
{
    globalLogLevel = level;
}

} // namespace ccap