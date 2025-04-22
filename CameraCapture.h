/**
 * @file CameraCapture.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#pragma once
#ifndef CAMERA_CAPTURE_H
#define CAMERA_CAPTURE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

// ccap is short for CameraCAPture
namespace ccap
{
enum class PixelFormat : uint8_t
{
    Unknown = 0,
    YUV420P = 1,
    YUV422P = 2,
    YUV444P = 3,
    NV12 = 4,
    NV21 = 5,
    RGB888 = 6,
    BGR888 = 7,
    RGBA8888 = 7, /// Alpha channel is filled with 0xFF
    BGRA8888 = 8, /// Alpha channel is filled with 0xFF
};
/// @brief Interface for memory allocation, primarily used to allocate the `data` field in `ccap::Frame`.
class Allocator : std::enable_shared_from_this<Allocator>
{
public:
    virtual ~Allocator() = 0;

    /// @brief Allocates memory, which can be accessed using the `data` field.
    virtual void resize(size_t size) = 0;

    /// @brief Provides access to the allocated memory.
    /// @note The pointer becomes valid only after calling `resize`.
    ///       If `resize` is called again, the pointer value may change, so it needs to be retrieved again.
    virtual uint8_t* data() = 0;

    /// @brief Returns the size of the allocated memory.
    virtual size_t size() = 0;
};

/// A default allocator, implemented using `std::vector<uint8_t>`.
class DefaultAllocator : public Allocator
{
public:
    ~DefaultAllocator() override;
    void resize(size_t size) override;
    uint8_t* data() override;
    size_t size() override;

private:
    std::vector<uint8_t> m_data;
};

struct Frame : std::enable_shared_from_this<Frame>
{
    Frame();
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    /// @brief Frame data, stored using the allocator.
    /// For pixel format I420: `data[0]` contains Y, `data[1]` contains U, and `data[2]` contains V.
    /// For pixel format NV12/NV21: `data[0]` contains Y, `data[1]` contains interleaved UV, and `data[2]` is nullptr.
    /// For other formats: `data[0]` contains the data, while `data[1]` and `data[2]` are nullptr.
    void* data[3] = {};

    /// @brief The pixel format of the frame.
    PixelFormat pixelFormat = PixelFormat::Unknown;

    /// @brief The width of the frame in pixels.
    int width = 0;

    /// @brief The height of the frame in pixels.
    int height = 0;

    /// @brief The size of the frame data in bytes.
    int sizeInBytes = 0;

    /// @brief The timestamp of the frame in nanoseconds.
    uint64_t timestamp = 0;

    /// @brief The unique, incremental index of the frame.
    uint64_t frameIndex = 0;

    /// @brief Allocator used for managing frame data memory. Defaults to `DefaultAllocator` if not provided.
    /// @note The allocator is responsible for allocating memory for the frame data.
    std::shared_ptr<Allocator> allocator;
};

enum class Property
{
    /// @brief The width of the frame.
    Width = 0,
    /// @brief The height of the frame.
    Height = 1,
    /// @brief The frame rate of the camera.
    FrameRate = 3,

    /// @brief The pixel format of the frame.
    PixelFormat = 4
};

class Provider
{
public:
    Provider();
    virtual ~Provider() = 0;

    /**
     * @brief Opens a capture device.
     *
     * @param deviceName The name of the device to open. The format is platform-dependent. Pass an empty string to use the default device.
     * @return true if the device was successfully opened, false otherwise.
     */
    virtual bool open(std::string_view deviceName) = 0;

    /**
     * @return true if the capture device is currently open, false otherwise.
     */
    virtual bool isOpened() const = 0;

    /**
     * @brief Closes the capture device. After calling this, the object should no longer be used.
     * @note This function is automatically called when the object is destroyed.
     *       You can also call it manually to release resources.
     */
    virtual void close() = 0;

    /**
     * @brief Starts capturing frames.
     * @return true if capturing started successfully, false otherwise.
     */
    virtual bool start() = 0;

    /**
     * @brief Pauses frame capturing. You can call `start` to resume capturing later.
     */
    virtual void pause() = 0;

    /**
     * @return true if the capture device is open and actively capturing frames, false otherwise.
     */
    virtual bool isStarted() const = 0;

    /**
     * @brief Sets a property of the camera.
     * @param prop The property to set. See #Property.
     * @param value The value to assign to the property. The value type is double, but the actual type depends on the property. Not all properties can be set.
     * @return true if the property was successfully set, false otherwise.
     */
    virtual bool set(Property prop, double value) = 0;

    /**
     * @brief Get a property of the camera.
     * @param prop See #Property
     * @return The value of the property.
     *   The value type is double, but the actual type depends on the property.
     *   Not all properties support being get.
     */
    virtual double get(Property prop) = 0;

    /**
     * @brief Grab a new frame. Can be called from any thread, but avoid concurrent calls.
     * @param waitNewFrame If true, wait for a new frame to be available. If false, return nullptr immediately. If the provider is not opened or paused, errors will be printed to `stderr`.
     * @return a valid `shared_ptr<Frame>` if a new frame is available, nullptr otherwise.
     * @note The returned frame is a shared pointer, and the caller can hold and use it later in any thread.
     *       The frame will be automatically reused when the last reference is released.
     */
    virtual std::shared_ptr<Frame> grab(bool waitNewFrame) = 0;

    /**
     * @brief Registers a callback to receive new frames.
     * @param callback The function to be invoked when a new frame is available.
     * @note The callback is executed in a background thread.
     *       The provided frame is a shared pointer, allowing the caller to retain and use it in any thread.
     *       The frame will be automatically recycled once the last reference is released.
     */
    virtual void setNewFrameCallback(std::function<void(std::shared_ptr<Frame>)> callback) = 0;

    /**
     * @brief Set the frame allocator.
     * @param allocator The allocator to be used for frame data. If not specified, `DefaultAllocator` will be used.
     */
    virtual void setFrameAllocator(std::shared_ptr<Allocator> allocator) = 0;
};

std::shared_ptr<Provider> createProvider();

} // namespace ccap

#endif // CAMERA_CAPTURE_H