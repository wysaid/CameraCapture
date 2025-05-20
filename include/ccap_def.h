/**
 * @file ccap_def.h
 * @author wysaid (this@wysaid.org)
 * @brief Some basic type definitions
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_DEF_H
#define CCAP_DEF_H

// #if defined(_WIN32) || defined(_MSC_VER)
// #ifndef _CRT_SECURE_NO_WARNINGS
// #define _CRT_SECURE_NO_WARNINGS 1
// #endif
// #endif

#include <cstdint>
#include <memory>
#include <string>
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
    kPixelFormatFullRangeBit = 1 << 17,
    kPixelFormatYUVColorFullRangeBit = kPixelFormatFullRangeBit | kPixelFormatYUVColorBit,

    /// `kPixelFormatRGBColorBit` is used to indicate whether it is an RGB or RGBA format
    kPixelFormatRGBColorBit = 1 << 18,

    /// `kPixelFormatAlphaColorBit` is used to indicate whether there is an Alpha channel
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
enum class PixelFormat : uint32_t
{
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

enum class FrameOrientation
{
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
inline bool pixelFormatInclude(PixelFormat lhs, PixelFormatConstants rhs)
{
    return (static_cast<uint32_t>(lhs) & rhs) == rhs;
}

inline bool pixelFormatInclude(PixelFormat lhs, PixelFormat rhs)
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) == static_cast<uint32_t>(rhs);
}

enum class PropertyName
{
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
     */
    PixelFormatOutput = 0x30002,

    /**
     * @brief Disable PixelFormat conversion; prefer fallback to other hardware-supported formats.
     *        When set to a nonzero value, pixel format conversion is not allowed.
     *        Note: The frame's pixelFormat may differ from the requested pixelFormat.
     */
    DisablePixelFormatConvert = 0x30003,

    /**
     * @brief The frame orientation. Will correct the orientation in RGB* PixelFormat, which may incur additional performance overhead.
     * @attention When the camera output pixel format is YUV, this property has no effect.
     *      It is recommended that users do not set this option, but instead adapt to the orientation information obtained from the Frame.
     */
    FrameOrientation = 0x40000,
};

/**
 * @brief Interface for memory allocation, primarily used to allocate the `data` field in `ccap::Frame`.
 * @note If you want to implement your own Allocator, you need to ensure that the allocated memory is 32-byte aligned to enable SIMD instruction set acceleration.
 */
class Allocator
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

struct Frame
{
    Frame();
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

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
};

/**
 * @brief Device information structure. This structure contains some information about the device.
 */
struct DeviceInfo
{
    std::string deviceName;

    /**
     * @brief Pixel formats supported by hardware. Choosing formats from this list avoids data conversion and provides better performance.
     */
    std::vector<PixelFormat> supportedPixelFormats;

    struct Resolution
    {
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