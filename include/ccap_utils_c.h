/**
 * @file ccap_utils_c.h
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface for utility functions in ccap library.
 * @date 2025-05
 *
 * @note This is the C interface for utility functions. For C++, use ccap_utils.h instead.
 */

#pragma once
#ifndef CCAP_UTILS_C_H
#define CCAP_UTILS_C_H

#include "ccap_c.h"
#include <stdint.h>
#include <stdbool.h>

// CCAP_EXPORT is already defined in ccap_c.h

#ifdef __cplusplus
extern "C" {
#endif

/* ========== String Utilities ========== */

/**
 * @brief Get string representation of pixel format
 * @param format Pixel format
 * @param buffer Output buffer to store the string (caller must allocate)
 * @param buffer_size Size of the output buffer
 * @return Number of characters written (excluding null terminator), or -1 on error
 * @note If buffer is NULL, returns the required buffer size (including null terminator)
 */
CCAP_EXPORT int ccap_pixel_format_to_string(CcapPixelFormat format, char* buffer, size_t buffer_size);

/* ========== File Utilities ========== */

/**
 * @brief Save a video frame as BMP or YUV file
 * @param frame Pointer to video frame
 * @param filename_no_suffix Filename without extension (extension will be added automatically)
 * @param output_path Buffer to store the full output path (caller must allocate)
 * @param output_path_size Size of the output_path buffer
 * @return Number of characters written to output_path (excluding null terminator), or -1 on error
 * @note If output_path is NULL, returns the required buffer size (including null terminator)
 *       YUV formats will be saved as .yuv files, RGB formats as .bmp files
 *       This function is for debugging purposes and not performance optimized
 */
CCAP_EXPORT int ccap_dump_frame_to_file(const CcapVideoFrame* frame, const char* filename_no_suffix, 
                            char* output_path, size_t output_path_size);

/**
 * @brief Save a video frame to directory with auto-generated filename
 * @param frame Pointer to video frame
 * @param directory Directory path to save the file
 * @param output_path Buffer to store the full output path (caller must allocate)
 * @param output_path_size Size of the output_path buffer
 * @return Number of characters written to output_path (excluding null terminator), or -1 on error
 * @note If output_path is NULL, returns the required buffer size (including null terminator)
 *       Filename will be generated based on current time and frame index
 *       This function is for debugging purposes and not performance optimized
 */
CCAP_EXPORT int ccap_dump_frame_to_directory(const CcapVideoFrame* frame, const char* directory,
                                 char* output_path, size_t output_path_size);

/**
 * @brief Save RGB data as BMP file
 * @param filename Output filename
 * @param data RGB/BGR image data
 * @param width Image width in pixels
 * @param line_offset Bytes per line (stride)
 * @param height Image height in pixels
 * @param is_bgr true if data is in BGR order, false for RGB
 * @param has_alpha true if data has alpha channel
 * @param is_top_to_bottom true if data is top-to-bottom, false for bottom-to-top
 * @return true on success, false on failure
 */
CCAP_EXPORT bool ccap_save_rgb_data_as_bmp(const char* filename, const unsigned char* data, 
                               uint32_t width, uint32_t line_offset, uint32_t height,
                               bool is_bgr, bool has_alpha, bool is_top_to_bottom);

/* ========== Memory Utilities ========== */

/**
 * @brief Free string allocated by ccap C functions
 * @param str String to free (can be NULL)
 */
CCAP_EXPORT void ccap_free_string(char* str);

/**
 * @brief Duplicate a string (similar to strdup)
 * @param str Source string
 * @return Duplicated string (caller must free with ccap_free_string), or NULL on failure
 */
CCAP_EXPORT char* ccap_strdup(const char* str);

/**
 * @brief Get the length of a string safely
 * @param str String to measure (can be NULL)
 * @param max_len Maximum length to check
 * @return String length, or 0 if str is NULL
 */
CCAP_EXPORT size_t ccap_strnlen(const char* str, size_t max_len);

/* ========== Logging Utilities ========== */

/**
 * @brief Log level enumeration for C interface
 */
typedef enum {
    CCAP_LOG_LEVEL_NONE = 0,              /**< No log output */
    CCAP_LOG_LEVEL_ERROR = 1,             /**< Error log level */
    CCAP_LOG_LEVEL_WARNING = 3,           /**< Warning log level (Error | Warning) */
    CCAP_LOG_LEVEL_INFO = 7,              /**< Info log level (Error | Warning | Info) */
    CCAP_LOG_LEVEL_VERBOSE = 15           /**< Verbose log level (Error | Warning | Info | Verbose) */
} CcapLogLevel;

/**
 * @brief Set the log level for ccap library
 * @param level Log level to set
 * @note This affects both C and C++ interfaces logging output
 */
CCAP_EXPORT void ccap_set_log_level(CcapLogLevel level);

#ifdef __cplusplus
}
#endif

#endif /* CCAP_UTILS_C_H */
