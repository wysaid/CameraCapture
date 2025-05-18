/**
 * @file ccap_core.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#include "ccap_core.h"
#include "ccap_imp.h"

#include <chrono>
#include <cstdio>

namespace ccap
{
ProviderImp* createProviderMac();
ProviderImp* createProviderDirectShow();

Allocator::~Allocator() = default;

Frame::Frame() = default;
Frame::~Frame() = default;

ProviderImp* createProvider(std::string_view extraInfo)
{
#if __APPLE__
    return createProviderMac();
#elif defined(_MSC_VER) || defined(_WIN32)
    return createProviderDirectShow();
#endif
    if (warningLogEnabled())
    {
        CCAP_LOG_W("ccap: Unsupported platform!\n");
    }
    return nullptr;
}

Provider::Provider() :
    m_imp(createProvider(""))
{
}

Provider::~Provider()
{
    delete m_imp;
}  

Provider::Provider(std::string_view deviceName, std::string_view extraInfo) :
    m_imp(createProvider(extraInfo))
{
    if (m_imp)
    {
        open(deviceName);
    }
}

Provider::Provider(int deviceIndex, std::string_view extraInfo) :
    m_imp(createProvider(extraInfo))
{
    if (m_imp)
    {
        open(deviceIndex);
    }
}

std::vector<std::string> Provider::findDeviceNames()
{
    return m_imp ? m_imp->findDeviceNames() : std::vector<std::string>();
}

bool Provider::open(std::string_view deviceName)
{
    return m_imp && m_imp->open(deviceName);
}

bool Provider::open(int deviceIndex)
{
    if (!m_imp)
        return false;

    std::string deviceName;
    if (deviceIndex >= 0)
    {
        auto deviceNames = findDeviceNames();
        if (!deviceNames.empty())
        {
            deviceIndex = std::min(deviceIndex, static_cast<int>(deviceNames.size()) - 1);
            deviceName = deviceNames[deviceIndex];

            CCAP_LOG_V("ccap: input device index %d, selected device name: %s\n", deviceIndex, deviceName.c_str());
        }
    }

    return open(deviceName);
}

bool Provider::isOpened() const
{
    return m_imp && m_imp->isOpened();
}

std::optional<DeviceInfo> Provider::getDeviceInfo() const
{
    return m_imp ? m_imp->getDeviceInfo() : std::nullopt;
}

void Provider::close()
{
    m_imp->close();
}

bool Provider::start()
{
    return m_imp && m_imp->start();
}

void Provider::stop()
{
    if (m_imp)
        m_imp->stop();
}

bool Provider::isStarted() const
{
    return m_imp && m_imp->isStarted();
}

bool Provider::set(PropertyName prop, double value)
{
    return m_imp->set(prop, value);
}

double Provider::get(PropertyName prop)
{
    return m_imp ? m_imp->get(prop) : NAN;
}

std::shared_ptr<Frame> Provider::grab(uint32_t timeoutInMs)
{
    return m_imp->grab(timeoutInMs);
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



} // namespace ccap