/**
 * @file ccap_c.h
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface header file for ccap, supports calling from pure C language.
 * @date 2025-05
 * 
 * @note For additional utility functions in C, also include:
 *       - ccap_convert_c.h (pixel conversion functions)
 *       - ccap_utils_c.h (file I/O and string utilities)
 *
 */

#pragma once
#ifndef CCAP_C_H
#define CCAP_C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Forward Declarations ========== */

/** @brief Opaque pointer to ccap::Provider C++ object */
typedef struct CcapProvider CcapProvider;

/** @brief Opaque pointer to ccap::VideoFrame C++ object */
typedef struct CcapVideoFrame CcapVideoFrame;

/* ========== Enumerations ========== */

/** @brief Pixel format enumeration, compatible with ccap::PixelFormat */
typedef enum {
    CCAP_PIXEL_FORMAT_UNKNOWN = 0,
    CCAP_PIXEL_FORMAT_NV12 = 1 | (1 << 16),
    CCAP_PIXEL_FORMAT_NV12F = CCAP_PIXEL_FORMAT_NV12 | (1 << 17),
    CCAP_PIXEL_FORMAT_I420 = (1 << 2) | (1 << 16),
    CCAP_PIXEL_FORMAT_I420F = CCAP_PIXEL_FORMAT_I420 | (1 << 17),
    CCAP_PIXEL_FORMAT_RGB24 = (1 << 3) | (1 << 18),
    CCAP_PIXEL_FORMAT_BGR24 = (1 << 4) | (1 << 18),
    CCAP_PIXEL_FORMAT_RGBA32 = CCAP_PIXEL_FORMAT_RGB24 | (1 << 19),
    CCAP_PIXEL_FORMAT_BGRA32 = CCAP_PIXEL_FORMAT_BGR24 | (1 << 19)
} CcapPixelFormat;

/** @brief Frame orientation enumeration */
typedef enum {
    CCAP_FRAME_ORIENTATION_TOP_TO_BOTTOM = 0,
    CCAP_FRAME_ORIENTATION_BOTTOM_TO_TOP = 1
} CcapFrameOrientation;

/** @brief Property name enumeration for camera configuration */
typedef enum {
    CCAP_PROPERTY_WIDTH = 0x10001,
    CCAP_PROPERTY_HEIGHT = 0x10002,
    CCAP_PROPERTY_FRAME_RATE = 0x20000,
    CCAP_PROPERTY_PIXEL_FORMAT_INTERNAL = 0x30001,
    CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT = 0x30002,
    CCAP_PROPERTY_FRAME_ORIENTATION = 0x40000
} CcapPropertyName;

/** @brief Error codes for camera capture operations */
typedef enum {
    CCAP_ERROR_NONE = 0,                        /**< No error occurred */
    CCAP_ERROR_NO_DEVICE_FOUND = 0x1001,       /**< No camera device found or device discovery failed */
    CCAP_ERROR_INVALID_DEVICE = 0x1002,        /**< Invalid device name or device index */
    CCAP_ERROR_DEVICE_OPEN_FAILED = 0x1003,    /**< Camera device open failed */
    CCAP_ERROR_DEVICE_START_FAILED = 0x1004,   /**< Camera start failed */
    CCAP_ERROR_DEVICE_STOP_FAILED = 0x1005,    /**< Camera stop failed */
    CCAP_ERROR_UNSUPPORTED_RESOLUTION = 0x2001, /**< Requested resolution is not supported */
    CCAP_ERROR_UNSUPPORTED_PIXEL_FORMAT = 0x2002, /**< Requested pixel format is not supported */
    CCAP_ERROR_FRAME_RATE_SET_FAILED = 0x2003,  /**< Frame rate setting failed */
    CCAP_ERROR_PROPERTY_SET_FAILED = 0x2004,    /**< Property setting failed */
    CCAP_ERROR_FRAME_CAPTURE_TIMEOUT = 0x3001, /**< Frame capture timeout */
    CCAP_ERROR_FRAME_CAPTURE_FAILED = 0x3002,  /**< Frame capture failed */
    CCAP_ERROR_MEMORY_ALLOCATION_FAILED = 0x4001, /**< Memory allocation failed */
    CCAP_ERROR_INTERNAL_ERROR = 0x9999,        /**< Unknown or internal error */
} CcapErrorCode;

/** @brief Error callback function type for C interface */
typedef void (*CcapErrorCallback)(CcapErrorCode errorCode, const char* errorDescription, void* userData);

/* ========== Constants ========== */

/** @brief Maximum number of camera devices */
#define CCAP_MAX_DEVICES 32

/** @brief Maximum length for device name including null terminator */
#define CCAP_MAX_DEVICE_NAME_LENGTH 128

/** @brief Maximum number of supported pixel formats */
#define CCAP_MAX_PIXEL_FORMATS 32

/** @brief Maximum number of supported resolutions */
#define CCAP_MAX_RESOLUTIONS 64

/* ========== Data Structures ========== */

/** @brief Video frame data structure for C interface */
typedef struct {
    uint8_t* data[3];                   /**< Pointers to frame data planes */
    uint32_t stride[3];                 /**< Stride (bytes per row) for each plane */
    CcapPixelFormat pixelFormat;        /**< Pixel format of the frame */
    uint32_t width;                     /**< Frame width in pixels */
    uint32_t height;                    /**< Frame height in pixels */
    uint32_t sizeInBytes;               /**< Total size of frame data in bytes */
    uint64_t timestamp;                 /**< Frame timestamp in nanoseconds */
    uint64_t frameIndex;                /**< Unique, incremental frame index */
    CcapFrameOrientation orientation;   /**< Frame orientation */
    void* nativeHandle;                 /**< Platform-specific native handle */
} CcapVideoFrameInfo;

/** @brief Resolution structure */
typedef struct {
    uint32_t width;
    uint32_t height;
} CcapResolution;

/** @brief Device names list structure */
typedef struct {
    char deviceNames[CCAP_MAX_DEVICES][CCAP_MAX_DEVICE_NAME_LENGTH]; /**< Array of device names */
    size_t deviceCount;                                              /**< Number of devices found */
} CcapDeviceNamesList;

/** @brief Device information structure */
typedef struct {
    char deviceName[CCAP_MAX_DEVICE_NAME_LENGTH];      /**< Device name */
    CcapPixelFormat supportedPixelFormats[CCAP_MAX_PIXEL_FORMATS]; /**< Array of supported pixel formats */
    size_t pixelFormatCount;                           /**< Number of supported pixel formats */
    CcapResolution supportedResolutions[CCAP_MAX_RESOLUTIONS];   /**< Array of supported resolutions */
    size_t resolutionCount;                            /**< Number of supported resolutions */
} CcapDeviceInfo;

/** @brief Callback function type for new frame notifications */
typedef bool (*CcapNewFrameCallback)(const CcapVideoFrame* frame, void* userData);

/* ========== Provider Lifecycle ========== */

/**
 * @brief Create a new camera provider instance
 * @return Pointer to CcapProvider instance, or NULL on failure
 */
CcapProvider* ccap_provider_create(void);

/**
 * @brief Create a camera provider and open specified device
 * @param deviceName Device name to open (NULL for default device)
 * @param extraInfo Extra information (currently unused, can be NULL)
 * @return Pointer to CcapProvider instance, or NULL on failure
 */
CcapProvider* ccap_provider_create_with_device(const char* deviceName, const char* extraInfo);

/**
 * @brief Create a camera provider and open device by index
 * @param deviceIndex Device index (negative for default device)
 * @param extraInfo Extra information (currently unused, can be NULL)
 * @return Pointer to CcapProvider instance, or NULL on failure
 */
CcapProvider* ccap_provider_create_with_index(int deviceIndex, const char* extraInfo);

/**
 * @brief Destroy a camera provider instance and release all resources
 * @param provider Pointer to CcapProvider instance
 */
void ccap_provider_destroy(CcapProvider* provider);

/* ========== Device Discovery ========== */

/**
 * @brief Find all available camera device names
 * @param provider Pointer to CcapProvider instance
 * @param deviceList Output parameter for device names list
 * @return true on success, false on failure
 */
bool ccap_provider_find_device_names_list(CcapProvider* provider, CcapDeviceNamesList* deviceList);

/* ========== Device Management ========== */

/**
 * @brief Open a camera device
 * @param provider Pointer to CcapProvider instance
 * @param deviceName Device name (NULL for default device)
 * @param autoStart Whether to start capturing automatically
 * @return true on success, false on failure
 */
bool ccap_provider_open(CcapProvider* provider, const char* deviceName, bool autoStart);

/**
 * @brief Open a camera device by index
 * @param provider Pointer to CcapProvider instance
 * @param deviceIndex Device index (negative for default device)
 * @param autoStart Whether to start capturing automatically
 * @return true on success, false on failure
 */
bool ccap_provider_open_by_index(CcapProvider* provider, int deviceIndex, bool autoStart);

/**
 * @brief Check if camera device is opened
 * @param provider Pointer to CcapProvider instance
 * @return true if opened, false otherwise
 */
bool ccap_provider_is_opened(const CcapProvider* provider);

/**
 * @brief Get device information
 * @param provider Pointer to CcapProvider instance
 * @param deviceInfo Output parameter for device information
 * @return true on success, false on failure
 */
bool ccap_provider_get_device_info(const CcapProvider* provider, CcapDeviceInfo* deviceInfo);

/**
 * @brief Close camera device
 * @param provider Pointer to CcapProvider instance
 */
void ccap_provider_close(CcapProvider* provider);

/* ========== Capture Control ========== */

/**
 * @brief Start frame capturing
 * @param provider Pointer to CcapProvider instance
 * @return true on success, false on failure
 */
bool ccap_provider_start(CcapProvider* provider);

/**
 * @brief Stop frame capturing
 * @param provider Pointer to CcapProvider instance
 */
void ccap_provider_stop(CcapProvider* provider);

/**
 * @brief Check if capture is started
 * @param provider Pointer to CcapProvider instance
 * @return true if started, false otherwise
 */
bool ccap_provider_is_started(const CcapProvider* provider);

/* ========== Property Configuration ========== */

/**
 * @brief Set camera property
 * @param provider Pointer to CcapProvider instance
 * @param prop Property name
 * @param value Property value
 * @return true on success, false on failure
 */
bool ccap_provider_set_property(CcapProvider* provider, CcapPropertyName prop, double value);

/**
 * @brief Get camera property
 * @param provider Pointer to CcapProvider instance
 * @param prop Property name
 * @return Property value, or NaN on failure
 */
double ccap_provider_get_property(CcapProvider* provider, CcapPropertyName prop);

/* ========== Frame Capture ========== */

/**
 * @brief Grab a new frame (synchronous)
 * @param provider Pointer to CcapProvider instance
 * @param timeoutMs Timeout in milliseconds (0xFFFFFFFF for infinite)
 * @return Pointer to CcapVideoFrame, or NULL on failure/timeout
 * @note The returned frame must be released using ccap_video_frame_release
 */
CcapVideoFrame* ccap_provider_grab(CcapProvider* provider, uint32_t timeoutMs);

/**
 * @brief Set callback for new frame notifications (asynchronous)
 * @param provider Pointer to CcapProvider instance
 * @param callback Callback function (NULL to remove callback)
 * @param userData User data passed to callback
 * @return true on success, false on failure
 */
bool ccap_provider_set_new_frame_callback(CcapProvider* provider, CcapNewFrameCallback callback, void* userData);

/* ========== Frame Management ========== */

/**
 * @brief Get frame information
 * @param frame Pointer to CcapVideoFrame instance
 * @param frameInfo Output parameter for frame information
 * @return true on success, false on failure
 */
bool ccap_video_frame_get_info(const CcapVideoFrame* frame, CcapVideoFrameInfo* frameInfo);

/**
 * @brief Release a video frame
 * @param frame Pointer to CcapVideoFrame instance
 */
void ccap_video_frame_release(CcapVideoFrame* frame);

/* ========== Advanced Configuration ========== */

/**
 * @brief Set maximum number of available frames in cache
 * @param provider Pointer to CcapProvider instance
 * @param size Maximum number of available frames
 */
void ccap_provider_set_max_available_frame_size(CcapProvider* provider, uint32_t size);

/**
 * @brief Set maximum number of frames in internal cache
 * @param provider Pointer to CcapProvider instance
 * @param size Maximum number of cached frames
 */
void ccap_provider_set_max_cache_frame_size(CcapProvider* provider, uint32_t size);

/* ========== Error Callback ========== */

/**
 * @brief Set error callback for a specific provider
 * @param provider Pointer to CcapProvider instance
 * @param callback Error callback function (NULL to remove callback)
 * @param userData User data passed to callback
 * @return true on success, false on failure
 */
bool ccap_provider_set_error_callback(CcapProvider* provider, CcapErrorCallback callback, void* userData);

/* ========== Utility Functions ========== */

/**
 * @brief Convert error code to English string description
 * @param errorCode The error code to convert
 * @return Error description string
 */
const char* ccap_error_code_to_string(CcapErrorCode errorCode);

/**
 * @brief Get library version string
 * @return Version string
 */
const char* ccap_get_version(void);

/**
 * @brief Check if a pixel format has RGB color
 * @param format Pixel format
 * @return true if RGB format, false otherwise
 */
bool ccap_pixel_format_is_rgb(CcapPixelFormat format);

/**
 * @brief Check if a pixel format has YUV color
 * @param format Pixel format
 * @return true if YUV format, false otherwise
 */
bool ccap_pixel_format_is_yuv(CcapPixelFormat format);

#ifdef __cplusplus
}
#endif

#endif /* CCAP_C_H */