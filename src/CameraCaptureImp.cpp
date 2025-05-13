/**
 * @file CameraCaptureImp.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Common Imp of ccap::Provider class.
 * @date 2025-04
 *
 */

#include "CameraCaptureImp.h"

#include <cmath>
#include <iostream>

namespace ccap
{
DefaultAllocator::~DefaultAllocator() = default;
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

ProviderImp::ProviderImp()
{
}

ProviderImp::~ProviderImp() = default;

bool ProviderImp::set(PropertyName prop, double value)
{
    auto lastProp = m_frameProp;
    switch (prop)
    {
    case PropertyName::Width:
        m_frameProp.width = static_cast<int>(value);
        break;
    case PropertyName::Height:
        m_frameProp.height = static_cast<int>(value);
        break;
    case PropertyName::FrameRate:
        m_frameProp.fps = value;
        break;
    case PropertyName::PixelFormat:
        m_frameProp.pixelFormat = static_cast<PixelFormat>(static_cast<int>(value));
        break;
    default:
        return false;
    }

    m_propertyChanged = (lastProp != m_frameProp);
    return true;
}

double ProviderImp::get(PropertyName prop)
{
    switch (prop)
    {
    case PropertyName::Width:
        return static_cast<double>(m_frameProp.width);
    case PropertyName::Height:
        return static_cast<double>(m_frameProp.height);
    case PropertyName::FrameRate:
        return m_frameProp.fps;
    case PropertyName::PixelFormat:
        return static_cast<double>(m_frameProp.pixelFormat);
    default:
        break;
    }
    return NAN;
}

void ProviderImp::setNewFrameCallback(std::function<bool(std::shared_ptr<Frame>)> callback)
{
    if (callback)
    {
        m_callback = std::make_shared<std::function<bool(std::shared_ptr<Frame>)>>(std::move(callback));
    }
    else
    {
        m_callback = nullptr;
    }
}

void ProviderImp::setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory)
{
    std::lock_guard<std::mutex> lock(m_poolMutex);
    m_allocatorFactory = std::move(allocatorFactory);
    m_framePool.clear();
}

std::shared_ptr<Frame> ProviderImp::grab(bool waitForNewFrame)
{
    std::unique_lock<std::mutex> lock(m_availableFrameMutex);

    if (m_availableFrames.empty() && waitForNewFrame)
    {
        if (!isStarted())
        {
            if (warningLogEnabled())
            {
                std::cerr << "ccap: Grab called when camera is not started!" << std::endl;
            }
            return nullptr;
        }

        m_grabFrameWaiting = true;
        m_frameCondition.wait(lock, [this]() {
            auto ret = m_grabFrameWaiting && !m_availableFrames.empty();
            if (!ret && verboseLogEnabled())
            {
                std::cerr << "ccap: Grab called with no frame available, waiting for new frame..." << std::endl;
            }
            return ret;
        });
        m_grabFrameWaiting = false;
    }

    if (!m_availableFrames.empty())
    {
        auto frame = std::move(m_availableFrames.front());
        m_availableFrames.pop();
        return frame;
    }
    return nullptr;
}

void ProviderImp::setMaxAvailableFrameSize(uint32_t size)
{
    m_maxAvailableFrameSize = size;
}

void ProviderImp::setMaxCacheFrameSize(uint32_t size)
{
    m_maxCacheFrameSize = size;
}

void ProviderImp::newFrameAvailable(std::shared_ptr<Frame> frame)
{
    bool retainNewFrame = true;
    if (auto c = m_callback; c && *c)
    { // 防止调用回调过程中回调被删除, callback 引用计数加 1
        retainNewFrame = (*c)(std::move(frame));
    }

    if (retainNewFrame)
    {
        std::lock_guard<std::mutex> lock(m_availableFrameMutex);

        m_availableFrames.push(std::move(frame));
        if (m_availableFrames.size() > m_maxAvailableFrameSize)
        {
            m_availableFrames.pop();
        }
    }

    if (m_grabFrameWaiting && !m_availableFrames.empty())
    {
        // 通知等待线程
        m_frameCondition.notify_all();
    }
}

std::shared_ptr<Frame> ProviderImp::getFreeFrame()
{
    std::lock_guard<std::mutex> lock(m_poolMutex);
    std::shared_ptr<Frame> frame;
    if (!m_framePool.empty())
    {
        auto ret = std::find_if(m_framePool.begin(), m_framePool.end(), [](const std::shared_ptr<Frame>& frame) {
            return frame.use_count() == 1;
        });

        if (ret != m_framePool.end())
        {
            frame = std::move(*ret);
            m_framePool.erase(ret);
        }
        else
        {
            if (m_framePool.size() > m_maxCacheFrameSize)
            {
                if (warningLogEnabled())
                {
                    std::cerr << "ccap: Frame pool is full, new frame allocated..." << std::endl;
                }
                m_framePool.erase(m_framePool.end());
            }
        }
    }

    if (!frame)
    {
        frame = std::make_shared<Frame>();
        m_framePool.push_back(frame);
    }
    return frame;
}
} // namespace ccap
