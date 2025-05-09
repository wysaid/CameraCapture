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
#include <string>
#include <string_view>
#include <vector>

// ccap is short for (C)amera(CAP)ture
namespace ccap
{
enum PixelFormatConstants : uint32_t
{
    /// `kPixelFormatRGBBit` and `kPixelFormatBGRBit` are used to distinguish the order of R-G-B channels
    kPixelFormatRGBBit = 1 << 3,
    kPixelFormatBGRBit = 1 << 4,

    /// Color Bit Mask
    kPixelFormatYUVColorBit = 1 << 16,
    kPixelFormatYUVColorVideoRangeBit = 1 << 17 | kPixelFormatYUVColorBit,
    kPixelFormatYUVColorFullRangeBit = 1 << 18 | kPixelFormatYUVColorBit,

    /// `kPixelFormatRGBColorBit` is used to indicate whether it is an RGB or RGBA format
    kPixelFormatRGBColorBit = 1 << 19,

    /// `kPixelFormatAlphaColorBit` is used to indicate whether there is an Alpha channel
    kPixelFormatAlphaColorBit = 1 << 20,
    kPixelFormatRGBAColorBit = kPixelFormatRGBColorBit | kPixelFormatAlphaColorBit,

    /// @brief When this flag is included in the format, it indicates that the format is a forcibly set format.
    kPixelFormatForceToSetBit = 1 << 30,
};

/**
 * @brief Pixel format. When used for setting, it may downgrade to other supported formats.
 *        The actual format should be determined by the pixelFormat of each Frame.
 * @note For Windows, BGR24 is the default format, while BGRA32 is the default format for macOS.
 *       The default PixelFormat usually provides support for ZeroCopy.
 *       For better performance, consider using the NV12v or NV12f formats. These two formats are
 *       often referred to as YUV formats and are supported by almost all platforms.
 */
enum class PixelFormat : uint32_t
{
    Unknown = 0,

    /**
     * @brief Best performance on all platforms. Always supported.
     *    On some devices, it is not possible to clearly determine whether it is FullRange or VideoRange.
     *    In such cases, the Frame can only indicate that it is NV12.
     *    In software design, you can implement a toggle option to allow users to choose whether the
     *    received Frame is FullRange or VideoRange based on what they observe.
     *
     */
    NV12 = 1,

    NV12v = NV12 | kPixelFormatYUVColorVideoRangeBit,
    NV12f = NV12 | kPixelFormatYUVColorFullRangeBit,

    /**
     * @brief Not commonly used, likely unsupported, may fall back to NV12*
     *    On some devices, it is not possible to clearly determine whether it is FullRange or VideoRange.
     *    In such cases, the Frame can only indicate that it is NV12.
     *    In software design, you can implement a toggle option to allow users to choose whether
     *    the received Frame is FullRange or VideoRange based on what they observe.
     * @refitem #NV12
     */
    NV21 = 1 << 1,

    NV21v = NV21 | kPixelFormatYUVColorVideoRangeBit,
    NV21f = NV21 | kPixelFormatYUVColorFullRangeBit,

    /**
     * @brief Not commonly used, likely unsupported, may fall back to NV12*
     *    On some devices, it is not possible to clearly determine whether it is FullRange or VideoRange.
     *    In such cases, the Frame can only indicate that it is NV12.
     *    In software design, you can implement a toggle option to allow users to choose whether
     *    the received Frame is FullRange or VideoRange based on what they observe.
     * @refitem #NV12
     */
    I420 = 1 << 2,

    I420v = I420 | kPixelFormatYUVColorVideoRangeBit,
    I420f = I420 | kPixelFormatYUVColorFullRangeBit,

    /// @brief Not commonly used, likely unsupported, may fall back to BGR24 (Windows) or BGRA32 (MacOS)
    RGB24 = kPixelFormatRGBBit | kPixelFormatRGBColorBit, /// 3 bytes per pixel

    /// @brief Always supported on all platforms. Simple to use.
    BGR24 = kPixelFormatBGRBit | kPixelFormatRGBColorBit, /// 3 bytes per pixel

    /**
     * @brief RGBA32 format, 4 bytes per pixel, alpha channel is filled with 0xFF
     * @note Not commonly used, likely unsupported, may fall back to BGR24
     */
    RGBA32 = RGB24 | kPixelFormatRGBAColorBit,

    /**
     *  @brief BGRA32 format, 4 bytes per pixel, alpha channel is filled with 0xFF
     *  @note This format is always supported on MacOS.
     */
    BGRA32 = BGR24 | kPixelFormatRGBAColorBit,

    /// ↓ Several formats for forced settings. After setting, it will definitely take effect.
    /// ↓ If the hardware does not support it, additional conversion will be performed.

    /// @brief RGB24 with forced conversion if unsupported.
    RGB24_Force = RGB24 | kPixelFormatForceToSetBit,

    /// @brief BGR24 with forced conversion if unsupported.
    BGR24_Force = BGR24 | kPixelFormatForceToSetBit,

    /// @brief RGBA32 with forced conversion if unsupported.
    RGBA32_Force = RGBA32 | kPixelFormatForceToSetBit,

    /// @brief BGRA32 with forced conversion if unsupported.
    BGRA32_Force = BGRA32 | kPixelFormatForceToSetBit,
};

inline bool operator&(PixelFormat lhs, PixelFormatConstants rhs)
{
    return (static_cast<uint32_t>(lhs) & rhs) != 0;
}

/// check if the pixel format `lhs` includes all bits of the pixel format `rhs`.
inline bool pixelFormatInclude(PixelFormat lhs, uint32_t rhs)
{
    return (static_cast<uint32_t>(lhs) & rhs) == rhs;
}

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

struct Frame : std::enable_shared_from_this<Frame>
{
    Frame();
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    /**
     * @brief Frame data, stored the raw bytes of a frame.
     *     For pixel format I420: `data[0]` contains Y, `data[1]` contains U, and `data[2]` contains V.
     *     For pixel format NV12/NV21: `data[0]` contains Y, `data[1]` contains interleaved UV, and `data[2]` is nullptr.
     *     For other formats: `data[0]` contains the data, while `data[1]` and `data[2]` are nullptr.
     */
    uint8_t* data[3] = {};

    /**
     * @brief Frame data stride.
     */
    uint32_t stride[3] = {};

    /// @brief The pixel format of the frame.
    PixelFormat pixelFormat = PixelFormat::Unknown;

    /// @brief The width of the frame in pixels.
    uint32_t width = 0;

    /// @brief The height of the frame in pixels.
    uint32_t height = 0;

    /// @brief The size of the frame data in bytes.
    uint32_t sizeInBytes = 0;

    /// @brief The timestamp of the frame in nanoseconds.
    uint64_t timestamp = 0;

    /// @brief The unique, incremental index of the frame.
    uint64_t frameIndex = 0;

    /**
     * @brief Memory allocator for Frame::data. When zero-copy is achievable, `ccap::Provider` will not use this allocator.
     *        If zero-copy is not achievable, this allocator will be used to allocate memory.
     *        When the allocator is not in use, this field will be set to nullptr.
     *        Users can customize this allocator through the `ccap::Provider::setFrameAllocator` method.
     */
    std::shared_ptr<Allocator> allocator;
};

enum class PropertyName
{
    /**
     * @brief The width of the frame.
     * @note When used to set the capture resolution, the closest available resolution will be chosen.
     *       If possible, a resolution with both width and height greater than or equal to the specified values will be selected.
     *       Example: For supported resolutions 1024x1024, 800x800, 800x600, and 640x480, setting 600x600 results in 800x600.
     */
    Width = 0x10001,

    /**
     * @brief The height of the frame.
     * @note When used to set the capture resolution, the closest available resolution will be chosen.
     *       If possible, a resolution with both width and height greater than or equal to the specified values will be selected.
     *       Example: For supported resolutions 1024x1024, 800x800, 800x600, and 640x480, setting 600x600 results in 800x600.
     */
    Height = 0x10002,

    /// @brief The frame rate of the camera, aka FPS (frames per second).
    FrameRate = 0x10003,

    /// @brief The pixel format of the frame.
    PixelFormat = 0x10004
};

enum
{
    /// @brief The default maximum number of frames that can be cached.
    DEFAULT_MAX_CACHE_FRAME_SIZE = 15,

    /// @brief The default maximum number of frames that can be available.
    DEFAULT_MAX_AVAILABLE_FRAME_SIZE = 3
};

class ProviderImp;

class Provider
{
public:
    explicit Provider(ProviderImp* imp);
    ~Provider();

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
     * @return true if the device was successfully opened, false otherwise.
     * @note The device name can be obtained using the `findDeviceNames` method.
     */
    bool open(std::string_view deviceName = "");

    /**
     * @brief Opens a camera by index.
     * @param deviceIndex Camera index from findDeviceNames(). A negative value indicates using the default device,
     *              and a value exceeding the number of devices indicates using the last device.
     * @return true if successful, false otherwise.
     */
    bool open(int deviceIndex);

    /**
     * @return true if the capture device is currently open, false otherwise.
     */
    bool isOpened() const;

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
    bool set(PropertyName prop, T value)
    {
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
     * @param waitForNewFrame If true, wait for a new frame to be available.
     *       If false, return nullptr immediately when no new frame available.
     *       If the provider is not opened or paused, errors will be printed to `stderr`.
     * @return a valid `shared_ptr<Frame>` if a new frame is available, nullptr otherwise.
     * @note The returned frame is a shared pointer, and the caller can hold and use it later in any thread.
     *       You don't need to deep copy this `std::shared_ptr<Frame>` object, even if you want to use it in
     *       different threads or at different times. Just save the smart pointer.
     *       The frame will be automatically reused when the last reference is released.
     */
    std::shared_ptr<Frame> grab(bool waitForNewFrame);

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
    void setNewFrameCallback(std::function<bool(std::shared_ptr<Frame>)> callback);

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

private:
    std::unique_ptr<ProviderImp> m_imp;
};

Provider* createProvider();

//////////////////// Utils //////////////////

/**
 * @brief Saves a Frame as a BMP or YUV file.
 *        If the Frame's pixelFormat is a YUV format,
 *        it will be saved as a YUV file; otherwise, it will be saved as a BMP file.
 * @param frame The frame to be dumped.
 * @param fileNameWithNoSuffix The name of the file to save the frame data.
 *        The suffix will be automatically added based on the pixel format.
 * @return The full path of the saved file if successful, or an empty string if the operation failed.
 */
std::string dumpFrameToFile(Frame* frame, std::string_view fileNameWithNoSuffix);

/**
 * @brief Saves a Frame as a BMP or YUV file.
 *        If the Frame's pixelFormat is a YUV format,
 *        it will be saved as a YUV file; otherwise, it will be saved as a BMP file.
 * @param frame The frame to be dumped.
 * @param directory The directory to save the frame data.
 *        The file name will be automatically generated based on the current time and frame index.
 * @return The full path of the saved file if successful, or an empty string if the operation failed.
 */
std::string dumpFrameToDirectory(Frame* frame, std::string_view directory);

/**
 * @brief Save RGB data as BMP file.
 * @param isBGR Indicates if the data is in B-G-R bytes order. If true, the data is in B-G-R order; otherwise, it is in R-G-B order.
 * @param hasAlpha Indicates if the data has an alpha channel. The alpha channel is always at the end of the pixel byte order.
 * @return true if the operation was successful, false otherwise.
 */
bool saveRgbDataAsBMP(const char* filename, const unsigned char* data, uint32_t w, uint32_t stride, uint32_t h, bool isBGR, bool hasAlpha);

//////////////////// Log ////////////////////

#ifndef CCAP_NO_LOG ///< Define this macro to remove log code during compilation.
#define _CCAP_LOG_ENABLED_ 1
#else
#define _CCAP_LOG_ENABLED_ 0
#endif

enum LogLevelConstants
{
    kLogLevelErrorBit = 1,
    kLogLevelWarningBit = 2,
    kLogLevelInfoBit = 4,
    kLogLevelVerboseBit = 8
};

enum class LogLevel
{
    /// @brief No log output.
    None = 0,
    /// @brief Error log level. Will output to `stderr` if an error occurs.
    Error = kLogLevelErrorBit,
    /// @brief Warning log level.
    Warning = Error | kLogLevelWarningBit,
    /// @brief Info log level.
    Info = Warning | kLogLevelInfoBit,
    /// @brief Debug log level.
    Verbose = Info | kLogLevelVerboseBit,
};

void setLogLevel(LogLevel level);
} // namespace ccap

#endif // CAMERA_CAPTURE_H