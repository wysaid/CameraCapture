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
LogLevel g_logLevel = LogLevel::Info;

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

std::shared_ptr<Provider> createProvider()
{
#if __APPLE__
    return std::make_shared<ProviderMac>();
#elif defined(_MSC_VER) || defined(_WIN32)
#endif

    if (g_logLevel & LogLevel::Error)
    {
        std::cerr << "Unsupported platform!" << std::endl;
    }
    return nullptr;
}

void setLogLevel(LogLevel level)
{
    g_logLevel = level;
}

} // namespace ccap