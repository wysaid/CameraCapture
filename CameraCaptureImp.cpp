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
extern LogLevel g_logLevel;

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

void ProviderImp::setFrameAllocator(std::shared_ptr<Allocator> allocator)
{
    std::lock_guard<std::mutex> lock(m_poolMutex);
    m_allocator = std::move(allocator);
    m_framePool.clear();
}

std::shared_ptr<Frame> ProviderImp::grab(bool waitForNewFrame)
{
    std::unique_lock<std::mutex> lock(m_availableFrameMutex);

    if (m_availableFrames.empty() && waitForNewFrame)
    {
        m_grabFrameWaiting = true;
        m_frameCondition.wait(lock, [this]() { return m_grabFrameWaiting && !m_availableFrames.empty(); });
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
                if (g_logLevel & LogLevel::Warning)
                {
                    std::cerr << "Frame pool is full, new frame allocated..." << std::endl;
                }
                m_framePool.erase(m_framePool.end());
            }
        }
    }

    if (!frame)
    {
        frame = std::make_shared<Frame>();
        frame->allocator = m_allocator;
        m_framePool.push_back(frame);
    }
    return frame;
}

void ProviderImp::updateFrameInfo(Frame& frame)
{
    frame.width = m_frameProp.width;
    frame.height = m_frameProp.height;
    frame.pixelFormat = m_frameProp.pixelFormat;
    frame.frameIndex = m_frameProp.frameIndex++;

    if (frame.pixelFormat & PixelFormat::YUVColorBit)
    {
        auto bytes = frame.width * frame.height * 3 / 2;
        frame.sizeInBytes = bytes;
        frame.allocator->resize(bytes);
        frame.data[0] = frame.allocator->data();
        frame.data[1] = frame.data[0] + frame.width * frame.height;
        if (frame.pixelFormat == PixelFormat::YUV420P)
        {
            frame.data[2] = frame.data[1] + frame.width * frame.height / 4;
        }
        else
        {
            frame.data[2] = nullptr;
        }
    }
    else
    {
        // RGB24 格式
        int channels = frame.pixelFormat & PixelFormat::RGBColorBit ? 3 : 4;
        auto bytes = frame.width * frame.height * channels;
        frame.sizeInBytes = bytes;
        frame.allocator->resize(bytes);
        frame.data[0] = frame.allocator->data();
        frame.data[1] = nullptr;
        frame.data[2] = nullptr;
    }
}

} // namespace ccap
