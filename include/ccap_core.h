/**
 * @file ccap_core.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 * 
 * @note For C language, use ccap_c.h instead of this header.
 *
 */

#ifndef __cplusplus
#error "ccap_core.h is for C++ only. For C language, please use ccap_c.h instead."
#endif

#pragma once
#ifndef CCAP_H_
#define CCAP_H_

#include "ccap_def.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// ccap is short for (C)amera(CAP)ture
namespace ccap {
/// A default allocator
class DefaultAllocator : public Allocator {
public:
    ~DefaultAllocator() override;
    void resize(size_t size) override;
    uint8_t* data() override;
    size_t size() override;

private:
    uint8_t* m_data = nullptr;
    size_t m_size = 0;
};

enum {
    /// @brief The default maximum number of frames that can be cached.
    DEFAULT_MAX_CACHE_FRAME_SIZE = 15,

    /// @brief The default maximum number of frames that can be available.
    DEFAULT_MAX_AVAILABLE_FRAME_SIZE = 3
};

class ProviderImp;

/**
 * @brief Camera capture provider. This class is used to open a camera device and capture frames from it.
 *        The actual implementation is platform-dependent.
 * @note This class is not thread-safe. It is recommended to use it in a single thread.
 *       If you need to use it in multiple threads, consider using a mutex or other synchronization methods.
 */
class Provider final {
public:
    /// @brief Default constructor. The camera device is not opened yet.
    ///        You can use the `open` method to open a camera device later.
    Provider();

    /**
     * @brief Construct a new Provider object, and open the camera device.
     * @param deviceName The name of the device to open. @see #open
     * @param extraInfo Currently unused.
     */
    explicit Provider(std::string_view deviceName, std::string_view extraInfo = "");

    /**
     * @brief Construct a new Provider object, and open the camera device.
     * @param deviceIndex The index of the device to open. @see #open
     * @param extraInfo Currently unused.
     */
    explicit Provider(int deviceIndex, std::string_view extraInfo = "");

    /**
     * @brief Retrieves the names of all available capture devices. Will perform a scan.
     * @return std::vector<std::string> A list of device names, usable with the `open` method.
     * @note The first device in the list is not necessarily the default device.
     *       To use the default device, pass an empty string to the `open` method.
     *       This method attempts to place real cameras at the beginning of the list and virtual cameras at the end.
     */
    std::vector<std::string> findDeviceNames();

    /**
     * @brief Opens a capture device.
     *
     * @param deviceName The name of the device to open. The format is platform-dependent. Pass an empty string to use the default device.
     * @param autoStart Whether to start capturing frames automatically after opening the device. Default is true.
     * @return true if the device was successfully opened, false otherwise.
     * @note The device name can be obtained using the `findDeviceNames` method.
     */
    bool open(std::string_view deviceName = "", bool autoStart = true);

    /**
     * @brief Opens a camera by index.
     * @param deviceIndex Camera index from findDeviceNames(). A negative value indicates using the default device,
     *              and a value exceeding the number of devices indicates using the last device.
     * @param autoStart Whether to start capturing frames automatically after opening the device. Default is true.
     * @return true if successful, false otherwise.
     */
    bool open(int deviceIndex, bool autoStart = true);

    /**
     * @return true if the capture device is currently open, false otherwise.
     */
    bool isOpened() const;

    /**
     * @brief Get device info, including current device name, supported resolutions, supported pixel formats, etc.
     * @return DeviceInfo. Should be called after `open` succeeds. If the device is not opened, returns std::nullopt.
     */
    std::optional<DeviceInfo> getDeviceInfo() const;

    /**
     * @brief Closes the capture device. After calling this, the object should no longer be used.
     * @note This function is automatically called when the object is destroyed.
     *       You can also call it manually to release resources.
     */
    void close();

    /**
     * @brief Starts capturing frames.
     * @return true if capturing started successfully, false otherwise.
     */
    bool start();

    /**
     * @brief Stop frame capturing. You can call `start` to resume capturing later.
     */
    void stop();

    /**
     * @brief Determines whether the camera is currently in a started state. Even if not manually stopped,
     *      the camera may stop due to reasons such as the device going offline (e.g., USB camera being unplugged).
     * @return true if the capture device is open and actively capturing frames, false otherwise.
     */
    bool isStarted() const;

    /**
     * @brief Sets a property of the camera.
     * @param prop The property to set. See #Property.
     * @param value The value to assign to the property. The value type is double, but the actual type depends on the property.
     *          Not all properties can be set.
     * @return true if the property was successfully set, false otherwise.
     * @note Some properties may require the camera to be restarted to take effect.
     */
    bool set(PropertyName prop, double value);

    template <class T>
    bool set(PropertyName prop, T value) {
        return set(prop, static_cast<double>(value));
    }

    /**
     * @brief Get a property of the camera.
     * @param prop See #Property
     * @return The value of the property.
     *   The value type is double, but the actual type depends on the property.
     *   Not all properties support being get.
     */
    double get(PropertyName prop);

    /**
     * @brief Grab a new frame. Can be called from any thread, but avoid concurrent calls.
     *      This method will block the current thread until a new frame is available.
     * @param timeoutInMs The maximum wait time (milliseconds). 0 means return immediately. The default is 0xffffffff (wait indefinitely).
     * @return a valid `shared_ptr<Frame>` if a new frame is available, nullptr otherwise.
     * @note The returned frame is a shared pointer, and the caller can hold and use it later in any thread.
     *       You don't need to deep copy this `std::shared_ptr<Frame>` object, even if you want to use it in
     *       different threads or at different times. Just save the smart pointer.
     *       The frame will be automatically reused when the last reference is released.
     */
    std::shared_ptr<VideoFrame> grab(uint32_t timeoutInMs = 0xffffffff);

    /**
     * @brief Registers a callback to receive new frames.
     * @param callback The function to be invoked when a new frame is available.
     *    The callback returns true to indicate that the frame has been processed and does not need to be retained.
     *    In this case, the next call to grab() will not return this frame.
     *    The callback returns false to indicate that the frame should be retained.
     *    In this case, the next call to grab() may return this frame.
     * @note The callback is executed in a background thread.
     *       The provided frame is a shared pointer, allowing the caller to retain and use it in any thread.
     *       You don't need to deep copy this `std::shared_ptr<Frame>` object, even if you want to use it in
     *       different threads or at different times. Just save the smart pointer.
     *       The frame will be automatically reused when the last reference is released.
     */
    void setNewFrameCallback(std::function<bool(const std::shared_ptr<VideoFrame>&)> callback);

    /**
     * @brief Sets the frame allocator factory. After calling this method, the default Allocator implementation will be overridden.
     * @refitem #Frame::allocator
     * @param allocatorFactory A factory function that returns a shared pointer to an Allocator instance.
     * @note Please call this method before `start()`. Otherwise, errors may occur.
     */
    void setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory);

    /**
     * @brief Sets the maximum number of available frames in the cache. If this limit is exceeded, the oldest frames will be discarded.
     * @param size The new maximum number of available frames in the cache.
     *     It is recommended to set this to at least 1 to avoid performance degradation.
     *     The default value is DEFAULT_MAX_AVAILABLE_FRAME_SIZE (3).
     */
    void setMaxAvailableFrameSize(uint32_t size);

    /**
     * @brief Sets the maximum number of frames in the internal cache. This affects performance.
     *     Setting it too high will consume excessive memory, while setting it too low may cause frequent memory allocations, reducing performance.
     * @param size The new maximum number of frames in the cache.
     *     It is recommended to set this to at least 3 to avoid performance degradation.
     *     The default value is DEFAULT_MAX_CACHE_FRAME_SIZE (15).
     */
    void setMaxCacheFrameSize(uint32_t size);

    // ↓ This part is not relevant to the user ↓
    Provider(Provider&&) = default;
    Provider& operator=(Provider&&) = default;
    ~Provider();

private:
    ProviderImp* m_imp;
};

} // namespace ccap

#endif // CCAP_H_
