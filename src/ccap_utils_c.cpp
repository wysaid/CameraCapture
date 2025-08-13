/**
 * @file ccap_utils_c.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface implementation for utility functions in ccap library.
 * @date 2025-05
 *
 */

#include "ccap_utils_c.h"

#include "ccap.h"
#include "ccap_utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

extern "C" {

/* ========== Internal Helper Functions ========== */

namespace {

// Convert C pixel format to C++ pixel format
ccap::PixelFormat convertPixelFormat(CcapPixelFormat format) {
    return static_cast<ccap::PixelFormat>(format);
}

// Convert C++ VideoFrame to C VideoFrame (get from CcapVideoFrame)
ccap::VideoFrame* getVideoFrameFromHandle(const CcapVideoFrame* frame) {
    if (!frame) return nullptr;
    // CcapVideoFrame is an opaque pointer that actually points to std::shared_ptr<ccap::VideoFrame>
    auto* framePtr = reinterpret_cast<const std::shared_ptr<ccap::VideoFrame>*>(frame);
    return framePtr->get();
}

// Safe string copy with size checking
int safeCopyString(const std::string& src, char* dest, size_t dest_size) {
    if (dest == nullptr) {
        // Return required size including null terminator
        return static_cast<int>(src.size() + 1);
    }

    if (dest_size == 0) {
        return -1; // Invalid buffer size
    }

    size_t copy_len = std::min(src.size(), dest_size - 1);
    std::memcpy(dest, src.c_str(), copy_len);
    dest[copy_len] = '\0';

    return static_cast<int>(copy_len);
}

} // anonymous namespace

/* ========== String Utilities ========== */

int ccap_pixel_format_to_string(CcapPixelFormat format, char* buffer, size_t buffer_size) {
    std::string_view format_str = ccap::pixelFormatToString(convertPixelFormat(format));
    std::string format_string(format_str);
    return safeCopyString(format_string, buffer, buffer_size);
}

/* ========== File Utilities ========== */

int ccap_dump_frame_to_file(const CcapVideoFrame* frame, const char* filename_no_suffix,
                            char* output_path, size_t output_path_size) {
    if (!frame || !filename_no_suffix) {
        return -1;
    }

    ccap::VideoFrame* cpp_frame = getVideoFrameFromHandle(frame);
    if (!cpp_frame) {
        return -1;
    }

    std::string result = ccap::dumpFrameToFile(cpp_frame, filename_no_suffix);
    return safeCopyString(result, output_path, output_path_size);
}

int ccap_dump_frame_to_directory(const CcapVideoFrame* frame, const char* directory,
                                 char* output_path, size_t output_path_size) {
    if (!frame || !directory) {
        return -1;
    }

    ccap::VideoFrame* cpp_frame = getVideoFrameFromHandle(frame);
    if (!cpp_frame) {
        return -1;
    }

    std::string result = ccap::dumpFrameToDirectory(cpp_frame, directory);
    return safeCopyString(result, output_path, output_path_size);
}

bool ccap_save_rgb_data_as_bmp(const char* filename, const unsigned char* data,
                               uint32_t width, uint32_t line_offset, uint32_t height,
                               bool is_bgr, bool has_alpha, bool is_top_to_bottom) {
    if (!filename || !data) {
        return false;
    }

    return ccap::saveRgbDataAsBMP(filename, data, width, line_offset, height,
                                  is_bgr, has_alpha, is_top_to_bottom);
}

/* ========== Memory Utilities ========== */

void ccap_free_string(char* str) {
    if (str) {
        std::free(str);
    }
}

char* ccap_strdup(const char* str) {
    if (!str) {
        return nullptr;
    }

    size_t len = std::strlen(str);
    char* result = static_cast<char*>(std::malloc(len + 1));
    if (result) {
        std::memcpy(result, str, len + 1);
    }
    return result;
}

size_t ccap_strnlen(const char* str, size_t max_len) {
    if (!str) {
        return 0;
    }

    size_t len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }
    return len;
}

/* ========== Logging Utilities ========== */

void ccap_set_log_level(CcapLogLevel level) {
    ccap::LogLevel cppLogLevel;

    switch (level) {
    case CCAP_LOG_LEVEL_NONE:
        cppLogLevel = ccap::LogLevel::None;
        break;
    case CCAP_LOG_LEVEL_ERROR:
        cppLogLevel = ccap::LogLevel::Error;
        break;
    case CCAP_LOG_LEVEL_WARNING:
        cppLogLevel = ccap::LogLevel::Warning;
        break;
    case CCAP_LOG_LEVEL_INFO:
        cppLogLevel = ccap::LogLevel::Info;
        break;
    case CCAP_LOG_LEVEL_VERBOSE:
        cppLogLevel = ccap::LogLevel::Verbose;
        break;
    default:
        cppLogLevel = ccap::LogLevel::None;
        break;
    }

    ccap::setLogLevel(cppLogLevel);
}

} // extern "C"
