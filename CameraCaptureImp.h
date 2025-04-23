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

#include <deque>
#include <queue>

namespace ccap
{
class ProviderImp : public Provider
{
public:
    ~ProviderImp() override;
    bool set(PropertyName prop, double value) override;
    double get(PropertyName prop) override;
    void setNewFrameCallback(std::function<bool(std::shared_ptr<Frame>)> callback) override;
    void setFrameAllocator(std::shared_ptr<Allocator> allocator) override;
    std::shared_ptr<Frame> grab(bool waitForNewFrame) override;
    void setMaxAvailableFrameSize(uint32_t size) override;
    void setMaxCacheFrameSize(uint32_t size) override;

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

protected:
    void newFrameAvailable(std::shared_ptr<Frame> frame);
    std::shared_ptr<Frame> getFreeFrame();
    void updateFrameInfo(Frame& frame);

    uint32_t m_maxAvailableFrameSize{ DEFAULT_MAX_AVAILABLE_FRAME_SIZE };
    uint32_t m_maxCacheFrameSize{ DEFAULT_MAX_CACHE_FRAME_SIZE };

private:
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
    bool m_propertyChanged{ false };
    bool m_grabFrameWaiting{ false };
};

} // namespace ccap

#endif
