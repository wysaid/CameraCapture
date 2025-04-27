/**
 * @file CameraCaptureImp.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for common imp of ccap::Provider class.
 * @date 2025-04
 *
 */

#pragma once

#ifndef CAMERA_CAPTURE_IMP_H
#define CAMERA_CAPTURE_IMP_H

#include "CameraCapture.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>

namespace ccap
{
struct FrameProperty
{
    double fps{ 30.0 };
    PixelFormat pixelFormat{ PixelFormat::BGR888 };
    uint32_t frameIndex{};
    int width{ 640 };
    int height{ 480 };

    inline bool operator==(const FrameProperty& prop) const
    {
        return fps == prop.fps && pixelFormat == prop.pixelFormat &&
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
    void setNewFrameCallback(std::function<bool(std::shared_ptr<Frame>)> callback);
    void setFrameAllocator(std::shared_ptr<Allocator> allocator);
    std::shared_ptr<Frame> grab(bool waitForNewFrame);
    void setMaxAvailableFrameSize(uint32_t size);
    void setMaxCacheFrameSize(uint32_t size);

    virtual bool open(std::string_view deviceName) = 0;
    virtual bool isOpened() const = 0;
    virtual void close() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isStarted() const = 0;

protected:
    void newFrameAvailable(std::shared_ptr<Frame> frame);
    std::shared_ptr<Frame> getFreeFrame();
    void updateFrameInfo(Frame& frame);

protected:
    // 回调和分配器
    std::shared_ptr<std::function<bool(std::shared_ptr<Frame>)>> m_callback;
    std::shared_ptr<Allocator> m_allocator;

    /// 相机的帧, 如果没有被取走, 也没有设置 callback 去获取, 那么会积压到这里, 最大长度为 MAX_AVAILABLE_FRAME_SIZE
    std::queue<std::shared_ptr<Frame>> m_availableFrames;

    /// 保存所有的帧, 以便于复用, 最大长度为 MAX_CACHE_FRAME_SIZE
    std::deque<std::shared_ptr<Frame>> m_framePool;
    std::mutex m_poolMutex, m_availableFrameMutex;
    std::condition_variable m_frameCondition;

    FrameProperty m_frameProp;

    uint32_t m_maxAvailableFrameSize{ DEFAULT_MAX_AVAILABLE_FRAME_SIZE };
    uint32_t m_maxCacheFrameSize{ DEFAULT_MAX_CACHE_FRAME_SIZE };

    bool m_propertyChanged{ false };
    bool m_grabFrameWaiting{ false };
};

/// For internal use.
extern LogLevel globalLogLevel;

inline bool operator&(LogLevel lhs, LogLevel rhs)
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

inline bool errorLogEnabled() { return globalLogLevel & LogLevel::Error; }
inline bool warningLogEnabled() { return globalLogLevel & LogLevel::Warning; }
inline bool infoLogEnabled() { return globalLogLevel & LogLevel::Info; }
inline bool verboseLogEnabled() { return globalLogLevel & LogLevel::Verbose; }

} // namespace ccap

#endif
