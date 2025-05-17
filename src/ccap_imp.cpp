/**
 * @file ccap_imp.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Common Imp of ccap::Provider class.
 * @date 2025-04
 *
 */

#include "ccap_imp.h"

#include <cassert>
#include <cmath>

#ifdef _MSC_VER
#include <malloc.h>
#define ALIGNED_ALLOC(alignment, size) _aligned_malloc(size, alignment)
#define ALIGNED_FREE(ptr) _aligned_free(ptr)
#else
#include <cstdlib>
#define ALIGNED_ALLOC(alignment, size) std::aligned_alloc(alignment, size)
#define ALIGNED_FREE(ptr) std::free(ptr)
#endif

namespace ccap
{
DefaultAllocator::~DefaultAllocator()
{
    if (m_data)
        ALIGNED_FREE(m_data);
}

void DefaultAllocator::resize(size_t size)
{
    assert(size != 0);
    if (size <= m_size && size >= m_size / 2 && m_data != nullptr)
        return;

    if (m_data)
        ALIGNED_FREE(m_data);

    // 64字节对齐，size必须是对齐的倍数
    size_t alignedSize = (size + 63) & ~size_t(63);
    m_data = static_cast<uint8_t*>(ALIGNED_ALLOC(64, alignedSize));
    m_size = alignedSize;
}

uint8_t* DefaultAllocator::data()
{
    return m_data;
}

size_t DefaultAllocator::size()
{
    return m_size;
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
    case PropertyName::PixelFormat: {
        auto intValue = static_cast<int>(value);
#if defined(_MSC_VER) || defined(_WIN32)
        intValue &= ~kPixelFormatFullRangeBit; // Full range is currently not supported on Windows
#endif
        m_frameProp.pixelFormat = static_cast<PixelFormat>(intValue);
    }
    break;
    case PropertyName::DisablePixelFormatConvert:
        m_tryConvertPixelFormat = static_cast<bool>(value);
        break;
    case PropertyName::FrameOrientation:
        m_frameOrientation = static_cast<FrameOrientation>(static_cast<int>(value));
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

std::shared_ptr<Frame> ProviderImp::grab(uint32_t timeoutInMs)
{
    std::unique_lock<std::mutex> lock(m_availableFrameMutex);

    if (m_availableFrames.empty() && timeoutInMs > 0)
    {
        if (!isStarted())
        {
            CCAP_LOG_W("ccap: Grab called when camera is not started!");
            return nullptr;
        }

        m_grabFrameWaiting = true;

        if (!m_frameCondition.wait_for(lock, std::chrono::milliseconds(timeoutInMs), [this]() {
                auto ret = m_grabFrameWaiting && !m_availableFrames.empty();
                if (!ret)
                {
                    CCAP_LOG_V("ccap: Grab called with no frame available, waiting for new frame...\n");
                }
                return ret;
            }))
        {
            m_grabFrameWaiting = false;
            CCAP_LOG_V("ccap: Grab timed out after %u ms\n", timeoutInMs);
            return nullptr;
        }
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
    { // Prevent callback from being deleted during invocation, increase callback ref count
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
        // Notify waiting threads
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
                CCAP_LOG_W("ccap: Frame pool is full, new frame allocated...");
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
