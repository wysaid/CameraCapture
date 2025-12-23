/**
 * @file ccap_imp.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Common Imp of ccap::Provider class.
 * @date 2025-04
 *
 */

#include "ccap_imp.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace ccap {
void resetSharedAllocator();
ErrorCallback getErrorCallback(); // Forward declaration

uint8_t* DefaultAllocator::data() { return m_data; }

size_t DefaultAllocator::size() { return m_size; }

ProviderImp::ProviderImp() {}

ProviderImp::~ProviderImp() {
    ccap::resetSharedAllocator();
}

bool ProviderImp::set(PropertyName prop, double value) {
    auto lastProp = m_frameProp;
    switch (prop) {
    case PropertyName::Width:
        m_frameProp.width = static_cast<int>(value);
        break;
    case PropertyName::Height:
        m_frameProp.height = static_cast<int>(value);
        break;
    case PropertyName::FrameRate:
        m_frameProp.fps = value;
        break;
    case PropertyName::PixelFormatInternal: {
        auto intValue = static_cast<int>(value);
#if defined(_MSC_VER) || defined(_WIN32)
        intValue &= ~kPixelFormatFullRangeBit;
#endif
        m_frameProp.cameraPixelFormat = static_cast<PixelFormat>(intValue);
        break;
    }
    case PropertyName::PixelFormatOutput: {
        PixelFormat format = (PixelFormat) static_cast<uint32_t>(value);
#if defined(_MSC_VER) || defined(_WIN32)
        (uint32_t&)format &= ~kPixelFormatFullRangeBit;
#endif
        if ((format & kPixelFormatYUVColorBit) &&
            m_frameProp.cameraPixelFormat == PixelFormat::Unknown) { /// If the output is in YUV format and InternalFormat is not set, then
                                                                     /// Internal is automatically set to the appropriate YUV format
#ifdef __APPLE__
            m_frameProp.cameraPixelFormat = PixelFormat::NV12f;
#else
            m_frameProp.cameraPixelFormat = PixelFormat::NV12;
#endif
        }
        m_frameProp.outputPixelFormat = static_cast<PixelFormat>(format);
    } break;
    case PropertyName::FrameOrientation:
        m_frameOrientation = static_cast<FrameOrientation>(static_cast<int>(value));
        break;
    default:
        return false;
    }

    m_propertyChanged = (lastProp != m_frameProp);
    return true;
}

double ProviderImp::get(PropertyName prop) {
    switch (prop) {
    case PropertyName::Width:
        return static_cast<double>(m_frameProp.width);
    case PropertyName::Height:
        return static_cast<double>(m_frameProp.height);
    case PropertyName::FrameRate:
        return m_frameProp.fps;
    case PropertyName::PixelFormatInternal:
        return static_cast<double>(m_frameProp.cameraPixelFormat);
    case PropertyName::PixelFormatOutput:
        return static_cast<double>(m_frameProp.outputPixelFormat);
    default:
        break;
    }
    return NAN;
}

void ProviderImp::setNewFrameCallback(std::function<bool(const std::shared_ptr<VideoFrame>&)> callback) {
    if (callback) {
        m_callback = std::make_shared<std::function<bool(const std::shared_ptr<VideoFrame>&)>>(std::move(callback));
    } else {
        m_callback = nullptr;
    }
}

void ProviderImp::setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory) {
    std::lock_guard<std::mutex> lock(m_poolMutex);
    m_allocatorFactory = std::move(allocatorFactory);
    m_framePool.clear();
}

std::shared_ptr<VideoFrame> ProviderImp::grab(uint32_t timeoutInMs) {
    std::unique_lock<std::mutex> lock(m_availableFrameMutex);

    if (m_availableFrames.empty() && timeoutInMs > 0) {
        if (!isStarted()) {
            reportError(ErrorCode::DeviceStartFailed, "Grab called when camera is not started");
            return nullptr;
        }

        m_grabFrameWaiting = true;
        bool waitSuccess{};

        for (uint32_t waitedTime = 0; waitedTime < timeoutInMs;) {
            uint32_t remainingTime = timeoutInMs - waitedTime;
            uint32_t waitTime = (remainingTime < 1000) ? remainingTime : 1000;
            waitSuccess = m_frameCondition.wait_for(lock, std::chrono::milliseconds(waitTime),
                                                    [this]() { return m_grabFrameWaiting && !m_availableFrames.empty(); });
            if (waitSuccess) break;
            waitedTime += waitTime;
            CCAP_LOG_V("ccap: Waiting for new frame... %u ms\n", waitedTime);
        }

        m_grabFrameWaiting = false;
        if (!waitSuccess) {
            reportError(ErrorCode::FrameCaptureTimeout, "Grab timed out after " + std::to_string(timeoutInMs) + " ms");
            return nullptr;
        }
    }

    if (!m_availableFrames.empty()) {
        auto frame = std::move(m_availableFrames.front());
        m_availableFrames.pop();
        return frame;
    }
    return nullptr;
}

void ProviderImp::setMaxAvailableFrameSize(uint32_t size) { m_maxAvailableFrameSize = size; }

void ProviderImp::setMaxCacheFrameSize(uint32_t size) { m_maxCacheFrameSize = size; }

void ProviderImp::newFrameAvailable(std::shared_ptr<VideoFrame> frame) {
    bool dropFrame = false;
    if (auto c = m_callback; c && *c) { // Prevent callback from being deleted during invocation, increase callback ref count
        dropFrame = (*c)(frame);
    }

    if (!dropFrame) {
        std::lock_guard<std::mutex> lock(m_availableFrameMutex);

        m_availableFrames.push(std::move(frame));
        if (m_availableFrames.size() > m_maxAvailableFrameSize) {
            m_availableFrames.pop();
        }
    }

    if (m_grabFrameWaiting && !m_availableFrames.empty()) {
        // Notify waiting threads
        m_frameCondition.notify_all();
    }
}

bool ProviderImp::tooManyNewFrames() { return m_availableFrames.size() > m_maxAvailableFrameSize; }

std::shared_ptr<VideoFrame> ProviderImp::getFreeFrame() {
    std::lock_guard<std::mutex> lock(m_poolMutex);
    std::shared_ptr<VideoFrame> frame;
    if (!m_framePool.empty()) {
        auto ret = std::find_if(m_framePool.begin(), m_framePool.end(),
                                [](const std::shared_ptr<VideoFrame>& frame) { return frame.use_count() == 1; });

        if (ret != m_framePool.end()) {
            frame = *ret;
        } else {
            if (m_framePool.size() > m_maxCacheFrameSize) {
                CCAP_LOG_W("ccap: VideoFrame pool is full, new frame allocated...");
                m_framePool.pop_front();
            }
        }
    }

    if (!frame) {
        frame = std::make_shared<VideoFrame>();
        m_framePool.push_back(frame);
    }
    return frame;
}

void reportError(ErrorCode errorCode, std::string_view description) {
    if (ErrorCallback globalCallback = getErrorCallback()) {
        globalCallback(errorCode, description);
    } else if (ccap::errorLogEnabled()) {
        CCAP_LOG_E("ccap error code %s: %s\n", errorCodeToString(errorCode).data(), description.data());
    }
}

} // namespace ccap