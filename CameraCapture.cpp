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

#include <iostream>

namespace ccap
{
Allocator::~Allocator() = default;
LogLevel globalLogLevel = LogLevel::Info;

Frame::Frame() = default;
Frame::~Frame() = default;

Provider::Provider(ccap::ProviderImp* imp) :
    m_imp(imp)
{
}

bool Provider::open(std::string_view deviceName)
{
    return m_imp->open(deviceName);
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