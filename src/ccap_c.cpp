/**
 * @file ccap_c.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface implementation for ccap, supports calling from pure C language.
 * @date 2025-05
 *
 */

#include "ccap_c.h"

#include "ccap.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

extern "C" {

/* ========== Internal Helper Functions ========== */

namespace {

// Convert C++ PixelFormat to C enum
CcapPixelFormat convert_pixel_format_to_c(ccap::PixelFormat format) {
    return static_cast<CcapPixelFormat>(static_cast<uint32_t>(format));
}

// Convert C enum to C++ PixelFormat
ccap::PixelFormat convert_pixel_format_from_c(CcapPixelFormat format) {
    return static_cast<ccap::PixelFormat>(static_cast<uint32_t>(format));
}

// Convert C++ PropertyName to C enum
CcapPropertyName convert_property_name_to_c(ccap::PropertyName prop) {
    return static_cast<CcapPropertyName>(static_cast<uint32_t>(prop));
}

// Convert C enum to C++ PropertyName
ccap::PropertyName convert_property_name_from_c(CcapPropertyName prop) {
    return static_cast<ccap::PropertyName>(static_cast<uint32_t>(prop));
}

// Convert C++ FrameOrientation to C enum
CcapFrameOrientation convert_frame_orientation_to_c(ccap::FrameOrientation orientation) {
    return static_cast<CcapFrameOrientation>(static_cast<uint32_t>(orientation));
}

// Helper to allocate and copy C string
char* allocate_c_string(const std::string& str) {
    if (str.empty()) return nullptr;
    char* result = static_cast<char*>(malloc(str.length() + 1));
    if (result) {
        strcpy(result, str.c_str());
    }
    return result;
}

// Wrapper struct for callback management
struct CallbackWrapper {
    CcapNewFrameCallback callback;
    void* userData;

    CallbackWrapper(CcapNewFrameCallback cb, void* data) :
        callback(cb), userData(data) {}
};

} // anonymous namespace

/* ========== Provider Lifecycle ========== */

CcapProvider* ccap_provider_create(void) {
    try {
        return reinterpret_cast<CcapProvider*>(new ccap::Provider());
    } catch (...) {
        return nullptr;
    }
}

CcapProvider* ccap_provider_create_with_device(const char* deviceName, const char* extraInfo) {
    try {
        std::string_view deviceNameView = deviceName ? deviceName : "";
        std::string_view extraInfoView = extraInfo ? extraInfo : "";
        return reinterpret_cast<CcapProvider*>(new ccap::Provider(deviceNameView, extraInfoView));
    } catch (...) {
        return nullptr;
    }
}

CcapProvider* ccap_provider_create_with_index(int deviceIndex, const char* extraInfo) {
    try {
        std::string_view extraInfoView = extraInfo ? extraInfo : "";
        return reinterpret_cast<CcapProvider*>(new ccap::Provider(deviceIndex, extraInfoView));
    } catch (...) {
        return nullptr;
    }
}

void ccap_provider_destroy(CcapProvider* provider) {
    if (provider) {
        delete reinterpret_cast<ccap::Provider*>(provider);
    }
}

/* ========== Device Discovery ========== */

bool ccap_provider_find_device_names(CcapProvider* provider, char*** deviceNames, size_t* count) {
    if (!provider || !deviceNames || !count) return false;

    auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
    auto devices = cppProvider->findDeviceNames();

    *count = devices.size();
    if (*count == 0) {
        *deviceNames = nullptr;
        return true;
    }

    char** names = static_cast<char**>(malloc(*count * sizeof(char*)));
    if (!names) return false;

    for (size_t i = 0; i < *count; ++i) {
        names[i] = allocate_c_string(devices[i]);
        if (!names[i]) {
            // Cleanup on failure
            for (size_t j = 0; j < i; ++j) {
                free(names[j]);
            }
            free(names);
            return false;
        }
    }

    *deviceNames = names;
    return true;
}

void ccap_provider_free_device_names(char** deviceNames, size_t count) {
    if (deviceNames) {
        for (size_t i = 0; i < count; ++i) {
            free(deviceNames[i]);
        }
        free(deviceNames);
    }
}

/* ========== Device Management ========== */

bool ccap_provider_open(CcapProvider* provider, const char* deviceName, bool autoStart) {
    if (!provider) return false;
    
    try {
        auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
        std::string_view deviceNameView = deviceName ? deviceName : "";
        return cppProvider->open(deviceNameView, autoStart);
    } catch (...) {
        return false;
    }
}

bool ccap_provider_open_by_index(CcapProvider* provider, int deviceIndex, bool autoStart) {
    if (!provider) return false;
    
    try {
        auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
        return cppProvider->open(deviceIndex, autoStart);
    } catch (...) {
        return false;
    }
}

bool ccap_provider_is_opened(const CcapProvider* provider) {
    if (!provider) return false;

    auto* cppProvider = reinterpret_cast<const ccap::Provider*>(provider);
    return cppProvider->isOpened();
}

bool ccap_provider_get_device_info(const CcapProvider* provider, CcapDeviceInfo* deviceInfo) {
    if (!provider || !deviceInfo) return false;

    auto* cppProvider = reinterpret_cast<const ccap::Provider*>(provider);
    auto infoOpt = cppProvider->getDeviceInfo();

    if (!infoOpt.has_value()) return false;

    const auto& info = infoOpt.value();

    // Initialize structure
    memset(deviceInfo, 0, sizeof(CcapDeviceInfo));

    // Copy device name
    deviceInfo->deviceName = allocate_c_string(info.deviceName);

    // Copy supported pixel formats
    deviceInfo->pixelFormatCount = info.supportedPixelFormats.size();
    if (deviceInfo->pixelFormatCount > 0) {
        deviceInfo->supportedPixelFormats = static_cast<CcapPixelFormat*>(
            malloc(deviceInfo->pixelFormatCount * sizeof(CcapPixelFormat)));

        if (deviceInfo->supportedPixelFormats) {
            for (size_t i = 0; i < deviceInfo->pixelFormatCount; ++i) {
                deviceInfo->supportedPixelFormats[i] = convert_pixel_format_to_c(info.supportedPixelFormats[i]);
            }
        }
    }

    // Copy supported resolutions
    deviceInfo->resolutionCount = info.supportedResolutions.size();
    if (deviceInfo->resolutionCount > 0) {
        deviceInfo->supportedResolutions = static_cast<CcapResolution*>(
            malloc(deviceInfo->resolutionCount * sizeof(CcapResolution)));

        if (deviceInfo->supportedResolutions) {
            for (size_t i = 0; i < deviceInfo->resolutionCount; ++i) {
                deviceInfo->supportedResolutions[i].width = info.supportedResolutions[i].width;
                deviceInfo->supportedResolutions[i].height = info.supportedResolutions[i].height;
            }
        }
    }

    return true;
}

void ccap_provider_free_device_info(CcapDeviceInfo* deviceInfo) {
    if (deviceInfo) {
        free(deviceInfo->deviceName);
        free(deviceInfo->supportedPixelFormats);
        free(deviceInfo->supportedResolutions);
        memset(deviceInfo, 0, sizeof(CcapDeviceInfo));
    }
}

void ccap_provider_close(CcapProvider* provider) {
    if (provider) {
        auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
        cppProvider->close();
    }
}

/* ========== Capture Control ========== */

bool ccap_provider_start(CcapProvider* provider) {
    if (!provider) return false;
    
    try {
        auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
        return cppProvider->start();
    } catch (...) {
        return false;
    }
}

void ccap_provider_stop(CcapProvider* provider) {
    if (provider) {
        auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
        cppProvider->stop();
    }
}

bool ccap_provider_is_started(const CcapProvider* provider) {
    if (!provider) return false;

    auto* cppProvider = reinterpret_cast<const ccap::Provider*>(provider);
    return cppProvider->isStarted();
}

/* ========== Property Configuration ========== */

bool ccap_provider_set_property(CcapProvider* provider, CcapPropertyName prop, double value) {
    if (!provider) return false;

    auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
    return cppProvider->set(convert_property_name_from_c(prop), value);
}

double ccap_provider_get_property(CcapProvider* provider, CcapPropertyName prop) {
    if (!provider) return NAN;

    auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
    return cppProvider->get(convert_property_name_from_c(prop));
}

/* ========== Frame Capture ========== */

CcapVideoFrame* ccap_provider_grab(CcapProvider* provider, uint32_t timeoutMs) {
    if (!provider) return nullptr;

    auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
    auto frame = cppProvider->grab(timeoutMs);

    if (!frame) return nullptr;

    // Transfer ownership to a heap-allocated shared_ptr
    auto* framePtr = new std::shared_ptr<ccap::VideoFrame>(std::move(frame));
    return reinterpret_cast<CcapVideoFrame*>(framePtr);
}

bool ccap_provider_set_new_frame_callback(CcapProvider* provider, CcapNewFrameCallback callback, void* userData) {
    if (!provider) return false;

    auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);

    if (callback) {
        // Create wrapper for the C callback
        auto wrapper = std::make_shared<CallbackWrapper>(callback, userData);

        cppProvider->setNewFrameCallback([wrapper](const std::shared_ptr<ccap::VideoFrame>& frame) -> bool {
            if (wrapper->callback) {
                // Transfer ownership to a heap-allocated shared_ptr for the callback
                auto* framePtr = new std::shared_ptr<ccap::VideoFrame>(frame);
                bool result = wrapper->callback(reinterpret_cast<CcapVideoFrame*>(framePtr), wrapper->userData);

                // Always clean up the frame pointer regardless of callback result
                // The callback result only determines whether the frame should be consumed by the underlying system
                delete framePtr;

                return result;
            }
            return false;
        });
    } else {
        // Remove callback
        cppProvider->setNewFrameCallback(nullptr);
    }

    return true;
}

/* ========== Frame Management ========== */

bool ccap_video_frame_get_info(const CcapVideoFrame* frame, CcapVideoFrameInfo* frameInfo) {
    if (!frame || !frameInfo) return false;

    auto* framePtr = reinterpret_cast<const std::shared_ptr<ccap::VideoFrame>*>(frame);
    const auto& cppFrame = **framePtr;

    // Copy frame information
    for (int i = 0; i < 3; ++i) {
        frameInfo->data[i] = cppFrame.data[i];
        frameInfo->stride[i] = cppFrame.stride[i];
    }

    frameInfo->pixelFormat = convert_pixel_format_to_c(cppFrame.pixelFormat);
    frameInfo->width = cppFrame.width;
    frameInfo->height = cppFrame.height;
    frameInfo->sizeInBytes = cppFrame.sizeInBytes;
    frameInfo->timestamp = cppFrame.timestamp;
    frameInfo->frameIndex = cppFrame.frameIndex;
    frameInfo->orientation = convert_frame_orientation_to_c(cppFrame.orientation);
    frameInfo->nativeHandle = cppFrame.nativeHandle;

    return true;
}

void ccap_video_frame_release(CcapVideoFrame* frame) {
    if (frame) {
        auto* framePtr = reinterpret_cast<std::shared_ptr<ccap::VideoFrame>*>(frame);
        delete framePtr;
    }
}

/* ========== Advanced Configuration ========== */

void ccap_provider_set_max_available_frame_size(CcapProvider* provider, uint32_t size) {
    if (provider) {
        auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
        cppProvider->setMaxAvailableFrameSize(size);
    }
}

void ccap_provider_set_max_cache_frame_size(CcapProvider* provider, uint32_t size) {
    if (provider) {
        auto* cppProvider = reinterpret_cast<ccap::Provider*>(provider);
        cppProvider->setMaxCacheFrameSize(size);
    }
}

/* ========== Utility Functions ========== */

const char* ccap_get_version(void) {
    // You may want to define this version string elsewhere
    return "1.0.0";
}

bool ccap_pixel_format_is_rgb(CcapPixelFormat format) {
    const uint32_t RGB_COLOR_BIT = 1 << 18;
    return (static_cast<uint32_t>(format) & RGB_COLOR_BIT) != 0;
}

bool ccap_pixel_format_is_yuv(CcapPixelFormat format) {
    const uint32_t YUV_COLOR_BIT = 1 << 16;
    return (static_cast<uint32_t>(format) & YUV_COLOR_BIT) != 0;
}

} // extern "C"