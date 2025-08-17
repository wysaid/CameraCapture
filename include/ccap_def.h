/**
 * @file ccap_def.h
 * @author wysaid (this@wysaid.org)
 * @brief Some basic type definitions
 * @date 2025-05
 * 
 * @note For C language, use ccap_c.h instead of this header.
 *
 */

#ifndef __cplusplus
#error "ccap_def.h is for C++ only. For C language, please use ccap_c.h instead."
#endif

#pragma once
#ifndef CCAP_DEF_H
#define CCAP_DEF_H

#if __APPLE__
#include <TargetConditionals.h>
#if (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define CCAP_IOS 1
#else
#define CCAP_MACOS 1
#endif

#elif defined(__ANDROID__)
#define CCAP_ANDROID 1
#elif defined(WIN32) || defined(_WIN32)
#define CCAP_WINDOWS 1
#if defined(_MSC_VER)
#define CCAP_WINDOWS_MSVC 1
#endif
#endif

#if !defined(CCAP_DESKTOP) && (CCAP_WINDOWS || CCAP_MACOS)
#define CCAP_DESKTOP 1
#endif

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ccap is short for (C)amera(CAP)ture
namespace ccap {
enum PixelFormatConstants : uint32_t {
    /// `kPixelFormatRGBBit` indicates that the pixel format is RGB or RGBA.
    kPixelFormatRGBBit = 1 << 3,
    /// `kPixelFormatRGBBit` indicates that the pixel format is BGR or BGRA.
    kPixelFormatBGRBit = 1 << 4,

    /// Color Bit Mask
    kPixelFormatYUVColorBit = 1 << 16,
    kPixelFormatFullRangeBit = 1 << 17,
    kPixelFormatYUVColorFullRangeBit = kPixelFormatFullRangeBit | kPixelFormatYUVColorBit,

    /// `kPixelFormatRGBColorBit` indicates that the pixel format is RGB/RGBA/BGR/BGRA.
    /// Which means it has RGB or RGBA color channels, and is not a YUV format.
    kPixelFormatRGBColorBit = 1 << 18,

    /// `kPixelFormatAlphaColorBit` is used to indicate whether there is an Alpha channel
    /// Which means the pixel format is RGBA or BGRA.
    kPixelFormatAlphaColorBit = 1 << 19,
    kPixelFormatRGBAColorBit = kPixelFormatRGBColorBit | kPixelFormatAlphaColorBit,
};

/**
 * @brief Pixel format. When used for setting, it may downgrade to other supported formats.
 *        The actual format should be determined by the pixelFormat of each Frame.
 * @note For Windows, BGR24 is the default format, while BGRA32 is the default format for macOS.
 *       The default PixelFormat usually provides support for ZeroCopy.
 *       For better performance, consider using the NV12v or NV12f formats. These two formats are
 *       often referred to as YUV formats and are supported by almost all platforms.
 */
enum class PixelFormat : uint32_t {
    Unknown = 0,

    /**
     * @brief YUV 4:2:0 semi-planar format. Generally provides good performance.
     *    On some devices, it is not possible to clearly determine whether it is FullRange or VideoRange.
     *    In such cases, the Frame can only indicate that it is NV12.
     *
     */
    NV12 = 1 | kPixelFormatYUVColorBit,

    /// @brief FullRange YUV 4:2:0 semi-planar format. Generally provides good performance.
    NV12f = NV12 | kPixelFormatYUVColorFullRangeBit,

    /**
     * @brief Not commonly used, likely unsupported, may fall back to NV12*
     *    On some devices, it is not possible to clearly determine whether it is FullRange or VideoRange.
     *    In such cases, the Frame can only indicate that it is NV12.
     *    In software design, you can implement a toggle option to allow users to choose whether
     *    the received Frame is FullRange or VideoRange based on what they observe.
     * @note This format is also known by other names, such as YUV420P or IYUV.
     * @refitem #NV12
     */
    I420 = 1 << 2 | kPixelFormatYUVColorBit,

    I420f = I420 | kPixelFormatYUVColorFullRangeBit,

    /**
     * @brief YUV 4:2:2 packed format (YUYV/YUY2). 2 bytes per pixel.
     * @note Common format for many USB cameras and video capture devices.
     *       This is a packed format where Y, U, and V components are interleaved.
     */
    YUYV = 1 << 3 | kPixelFormatYUVColorBit,

    /// @brief FullRange YUV 4:2:2 packed format (YUYV/YUY2)
    YUYVf = YUYV | kPixelFormatYUVColorFullRangeBit,

    /**
     * @brief YUV 4:2:2 packed format (UYVY). 2 bytes per pixel.
     * @note Similar to YUYV but with different component ordering.
     */
    UYVY = 1 << 4 | kPixelFormatYUVColorBit,

    /// @brief FullRange YUV 4:2:2 packed format (UYVY)
    UYVYf = UYVY | kPixelFormatYUVColorFullRangeBit,

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
};

enum class FrameOrientation {
    /**
     * @brief The frame is laid out in a top-to-bottom format.
     *     The first row of data corresponds to the first row of the image.
     *     In other words, the image's (0, 0) point aligns with the data's (0, 0) point.
     *     YUV formats are usually in this format.
     *     RGB formats are usually in this format on macOS.
     *     This is the most common layout.
     */
    TopToBottom = 0,

    /**
     * @brief The frame is laid out in a bottom-to-top format.
     *     The first row of data corresponds to the last row of the image.
     *     In other words, the image's (0, 0) point aligns with the data's (0, height - 1) point.
     *     On Windows, when the data format is RGB or similar, this field is often true.
     */
    BottomToTop = 1,

    Default = TopToBottom,
};

/// check if the pixel format `lhs` includes all bits of the pixel format `rhs`.
inline bool pixelFormatInclude(PixelFormat lhs, PixelFormatConstants rhs) {
    return (static_cast<uint32_t>(lhs) & rhs) == rhs;
}

inline bool pixelFormatInclude(PixelFormat lhs, PixelFormat rhs) {
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) == static_cast<uint32_t>(rhs);
}

enum class PropertyName {
    /**
     * @brief The width of the frame.
     * @note When used to set the capture resolution, the closest available resolution will be chosen.
     *       If possible, a resolution with both width and height greater than or equal to the specified values will be selected.
     *       Example: For supported resolutions 1024x1024, 800x800, 800x600, and 640x480, setting 600x600 results in 800x600.
     *       When used with the get method, the value may not be accurate. Please refer to the actual Frame obtained.
     */
    Width = 0x10001,

    /**
     * @brief The height of the frame.
     * @note When used to set the capture resolution, the closest available resolution will be chosen.
     *       If possible, a resolution with both width and height greater than or equal to the specified values will be selected.
     *       Example: For supported resolutions 1024x1024, 800x800, 800x600, and 640x480, setting 600x600 results in 800x600.
     *       When used with the get method, the value may not be accurate. Please refer to the actual Frame obtained.
     */
    Height = 0x10002,

    /**
     * @brief The frame rate of the camera, also known as FPS (frames per second).
     * @note When used with get, the value may not be accurate and depends on the underlying camera driver implementation.
     */
    FrameRate = 0x20000,

    /**
     * @brief The actual pixel format used by the camera. If not set, it will be selected automatically.
     * @note Example: On Windows, if the camera only supports MJPG and PixelFormatInternal is not set,
     *       BGR24 will be chosen by default unless you explicitly specify another format like BGRA32.
     */
    PixelFormatInternal = 0x30001,

    /**
     * @brief The output pixel format of ccap. Can be different from PixelFormatInternal.
     * @note If PixelFormatInternal is RGB(A), PixelFormatOutput cannot be set to a YUV format.
     *       If PixelFormatInternal is YUV and PixelFormatOutput is RGB(A), BT.601 will be used for conversion.
     *       For other cases, there are no issues.
     *       If PixelFormatInternal and PixelFormatOutput are the same format, data conversion will be skipped and the original data will be used directly.
     *       In general, setting both PixelFormatInternal and PixelFormatOutput to YUV formats can achieve better performance.
     */
    PixelFormatOutput = 0x30002,

    /**
     * @brief The frame orientation. Will correct the orientation in RGB* PixelFormat, which may incur additional performance overhead.
     * @attention When the camera output pixel format is YUV, this property has no effect.
     *      It is recommended that users do not set this option, but instead adapt to the orientation information obtained from the Frame.
     */
    FrameOrientation = 0x40000,
};

/**
 * @brief Error codes for camera capture operations
 */
enum class ErrorCode {
    /// No error occurred
    None = 0,
    
    /// No camera device found or device discovery failed
    NoDeviceFound = 0x1001,
    
    /// Invalid device name or device index
    InvalidDevice = 0x1002,
    
    /// Camera device open failed
    DeviceOpenFailed = 0x1003,
    
    /// Camera start failed
    DeviceStartFailed = 0x1004,
    
    /// Camera stop failed
    DeviceStopFailed = 0x1005,
    
    /// Requested resolution is not supported
    UnsupportedResolution = 0x2001,
    
    /// Requested pixel format is not supported
    UnsupportedPixelFormat = 0x2002,
    
    /// Frame rate setting failed
    FrameRateSetFailed = 0x2003,
    
    /// Property setting failed
    PropertySetFailed = 0x2004,
    
    /// Frame capture timeout
    FrameCaptureTimeout = 0x3001,
    
    /// Frame capture failed
    FrameCaptureFailed = 0x3002,
    
    /// Memory allocation failed
    MemoryAllocationFailed = 0x4001,
    
    /// Unknown or internal error
    InternalError = 0x9999,
};

/**
 * @brief Error callback function type for C++ interface
 * @param errorCode The error code that occurred
 * @param errorDescription English description of the error
 */
using ErrorCallback = std::function<void(ErrorCode errorCode, std::string_view errorDescription)>;

/**
 * @brief Convert error code to English string description
 * @param errorCode The error code to convert
 * @return English description of the error
 */
std::string_view errorCodeToString(ErrorCode errorCode);

/**
 * @brief Interface for memory allocation, primarily used to allocate the `data` field in `ccap::Frame`.
 * @note If you want to implement your own Allocator, you need to ensure that the allocated memory is 32-byte aligned to enable SIMD instruction set acceleration.
 */
class Allocator {
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

struct VideoFrame {
    VideoFrame();
    ~VideoFrame();
    VideoFrame(const VideoFrame&) = delete;
    VideoFrame& operator=(const VideoFrame&) = delete;

    /**
     * @brief Frame data, stored the raw bytes of a frame.
     *     For pixel format I420: `data[0]` contains Y, `data[1]` contains U, and `data[2]` contains V.
     *     For pixel format NV12: `data[0]` contains Y, `data[1]` contains interleaved UV, and `data[2]` is nullptr.
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

    /// @brief The orientation of the frame. @see #FrameOrientation
    FrameOrientation orientation = FrameOrientation::Default;

    /**
     * @brief Memory allocator for Frame::data. When zero-copy is achievable, `ccap::Provider` will not use this allocator.
     *        If zero-copy is not achievable, this allocator will be used to allocate memory.
     *        When the allocator is not in use, this field will be set to nullptr.
     *        Users can customize this allocator through the `ccap::Provider::setFrameAllocator` method.
     * @attention Normally, users do not need to care about this field.
     */
    std::shared_ptr<Allocator> allocator;

    /**
     * @brief Native handle for the frame, used for platform-specific operations.
     *        This field is optional and may be nullptr if not needed.
     * @note Currently defined as follows:
     *     - Windows: When the backend is DirectShow, the actual type of nativeHandle is `IMediaSample*`
     *     - macOS/iOS: The actual type of nativeHandle is `CMSampleBufferRef`
     *     - Linux: The actual type is uint32_t, stands for `v4l2_buffer::index`.
     */
    void* nativeHandle = nullptr; ///< Native handle for the frame, used for platform-specific operations

    /**
     * @brief When (allocator == nullptr || data[0] != allocator->data()), the data is stored in a hardware buffer.
     *    If you hold multiple VideoFrame objects for a long time, it may prevent the camera hardware buffer from being reused,
     *    affecting performance or causing the camera to stop working.
     *    Therefore, if you need to hold a VideoFrame object for a long time, you should call the `detach()` method to release nativeHandle.
     *    If data[0] == allocator->data(), calling `detach()` has no extra cost.
     *    If data[0] != allocator->data(), calling `detach()` will copy the data into the allocator.
     *    After calling detach, nativeHandle will be set to nullptr, and data[0] will point to allocator->data().
     *
     * @note Best practice: If you need to pass a std::shared_ptr<VideoFrame> object across threads or hold it across frames,
     *    you should call `detach()` immediately after obtaining the std::shared_ptr<VideoFrame> object.
     *
     */
    void detach();
};

/**
 * @brief Device information structure. This structure contains some information about the device.
 */
struct DeviceInfo {
    std::string deviceName;

    /**
     * @brief Pixel formats supported by hardware. Choosing formats from this list avoids data conversion and provides better performance.
     */
    std::vector<PixelFormat> supportedPixelFormats;

    struct Resolution {
        uint32_t width;
        uint32_t height;
    };

    /**
     * @brief Resolutions supported by hardware. Choosing resolutions from this list avoids resolution conversion and provides better performance.
     */
    std::vector<Resolution> supportedResolutions;
};

} // namespace ccap

#endif