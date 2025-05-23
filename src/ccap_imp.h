/**
 * @file ccap_imp.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for common imp of ccap::Provider class.
 * @date 2025-04
 *
 */

#pragma once

#ifndef CAMERA_CAPTURE_IMP_H
#define CAMERA_CAPTURE_IMP_H

#include "ccap_core.h"
#include "ccap_utils.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <queue>

#if defined(_WIN32) || defined(_MSC_VER)
#ifndef _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#define _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR 1
#endif

#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#endif

namespace ccap
{
struct FrameProperty
{
    double fps{ 30.0 };

    PixelFormat cameraPixelFormat = PixelFormat::Unknown;

    PixelFormat outputPixelFormat{
#ifdef __APPLE__
        PixelFormat::BGRA32 ///< MacOS default
#else
        PixelFormat::BGR24 ///< Windows default
#endif
    };

    int width{ 640 };
    int height{ 480 };

    inline bool operator==(const FrameProperty& prop) const
    {
        return fps == prop.fps && cameraPixelFormat == prop.cameraPixelFormat && outputPixelFormat == prop.outputPixelFormat &&
            width == prop.width && height == prop.height;
    }
    inline bool operator!=(const FrameProperty& prop) const
    {
        return !(*this == prop);
    }
};

class ProviderImp
{
public:
    ProviderImp();
    virtual ~ProviderImp();
    bool set(PropertyName prop, double value);
    double get(PropertyName prop);
    void setNewFrameCallback(std::function<bool(const std::shared_ptr<Frame>&)> callback);
    void setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory);
    std::shared_ptr<Frame> grab(uint32_t timeoutInMs);
    void setMaxAvailableFrameSize(uint32_t size);
    void setMaxCacheFrameSize(uint32_t size);

    virtual std::vector<std::string> findDeviceNames() = 0;
    virtual bool open(std::string_view deviceName) = 0;
    virtual bool isOpened() const = 0;
    virtual std::optional<DeviceInfo> getDeviceInfo() const = 0;
    virtual void close() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isStarted() const = 0;

    inline FrameProperty& getFrameProperty() { return m_frameProp; }
    inline const FrameProperty& getFrameProperty() const { return m_frameProp; }

    inline std::atomic_uint32_t& frameIndex() { return m_frameIndex; }

    inline const std::function<std::shared_ptr<Allocator>()>& getAllocatorFactory() const { return m_allocatorFactory; }

    bool tooManyNewFrames();

protected:
    void newFrameAvailable(std::shared_ptr<Frame> frame);
    std::shared_ptr<Frame> getFreeFrame();

protected:
    // Callback function for new data frames
    std::shared_ptr<std::function<bool(const std::shared_ptr<Frame>&)>> m_callback;
    std::function<std::shared_ptr<Allocator>()> m_allocatorFactory;

    /// Frames from camera. If not taken or no callback is set, they will accumulate here. Max length is MAX_AVAILABLE_FRAME_SIZE.
    std::queue<std::shared_ptr<Frame>> m_availableFrames;

    /// All frames for reuse. Max length is MAX_CACHE_FRAME_SIZE.
    std::deque<std::shared_ptr<Frame>> m_framePool;
    std::mutex m_poolMutex, m_availableFrameMutex;
    std::condition_variable m_frameCondition;

    FrameProperty m_frameProp;

    uint32_t m_maxAvailableFrameSize{ DEFAULT_MAX_AVAILABLE_FRAME_SIZE };
    uint32_t m_maxCacheFrameSize{ DEFAULT_MAX_CACHE_FRAME_SIZE };

    bool m_propertyChanged{ false };
    bool m_grabFrameWaiting{ false };
    FrameOrientation m_frameOrientation = FrameOrientation::Default;

    std::atomic_uint32_t m_frameIndex{};
};

/// A lightweight class used to call the deleter function in the destructor of Frame
class FakeFrame : std::enable_shared_from_this<FakeFrame>
{
public:
    explicit FakeFrame(std::function<void()> deleter) :
        m_deleter(std::move(deleter))
    {
    }
    ~FakeFrame()
    {
        if (m_deleter)
            m_deleter();
    }

private:
    std::function<void()> m_deleter;
};

inline bool operator&(PixelFormat lhs, PixelFormatConstants rhs)
{
    return (static_cast<uint32_t>(lhs) & rhs) != 0;
}

} // namespace ccap

#endif
