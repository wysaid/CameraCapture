/**
 * @file ccap_utils.h
 * @author wysaid (this@wysaid.org)
 * @brief Some utility functions for ccap.
 * @date 2025-05
 *
 */

#pragma once
#ifndef CCAP_UTILS_H
#define CCAP_UTILS_H

#include "ccap_core.h"

#include <string>
#include <string_view>

// ccap is short for (C)amera(CAP)ture
namespace ccap
{
std::string_view pixelFormatToString(PixelFormat format);

//////////////////// File Utils ///////////////////

/**
 * @brief Saves a Frame as a BMP or YUV file.
 *        If the Frame's pixelFormat is a YUV format,
 *        it will be saved as a YUV file; otherwise, it will be saved as a BMP file.
 * @param frame The frame to be dumped.
 * @param fileNameWithNoSuffix The name of the file to save the frame data.
 *        The suffix will be automatically added based on the pixel format.
 * @return The full path of the saved file if successful, or an empty string if the operation failed.
 * @note Note: This method uses a simple way to save data for debugging purposes. Not performance optimized. Do not use in performance-sensitive code.
 */
std::string dumpFrameToFile(VideoFrame* frame, std::string_view fileNameWithNoSuffix);

/**
 * @brief Saves a Frame as a BMP or YUV file.
 *        If the Frame's pixelFormat is a YUV format,
 *        it will be saved as a YUV file; otherwise, it will be saved as a BMP file.
 * @param frame The frame to be dumped.
 * @param directory The directory to save the frame data.
 *        The file name will be automatically generated based on the current time and frame index.
 * @return The full path of the saved file if successful, or an empty string if the operation failed.
 * @note Note: This method uses a simple way to save data for debugging purposes. Not performance optimized. Do not use in performance-sensitive code.
 */
std::string dumpFrameToDirectory(VideoFrame* frame, std::string_view directory);

/**
 * @brief Save RGB data as BMP file.
 * @param isBGR Indicates if the data is in B-G-R bytes order. If true, the data is in B-G-R order; otherwise, it is in R-G-B order.
 * @param hasAlpha Indicates if the data has an alpha channel. The alpha channel is always at the end of the pixel byte order.
 * @param isTopToBottom Indicates if the data is in top-to-bottom order.
 * @return true if the operation was successful, false otherwise.
 */
bool saveRgbDataAsBMP(const char* filename, const unsigned char* data, uint32_t w, uint32_t lineOffset, uint32_t h, bool isBGR, bool hasAlpha, bool isTopToBottom = false);

//////////////////// Log ////////////////////

#ifndef CCAP_NO_LOG          ///< Define this macro to remove log code during compilation.
#define _CCAP_LOG_ENABLED_ 1 // NOLINT(*-reserved-identifier)
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

#if _CCAP_LOG_ENABLED_
/// For internal use.
extern LogLevel globalLogLevel;

inline bool operator&(LogLevel lhs, LogLevelConstants rhs)
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline bool errorLogEnabled() { return globalLogLevel & kLogLevelErrorBit; }
inline bool warningLogEnabled() { return globalLogLevel & kLogLevelWarningBit; }
inline bool infoLogEnabled() { return globalLogLevel & kLogLevelInfoBit; }
inline bool verboseLogEnabled() { return globalLogLevel & kLogLevelVerboseBit; }

#define CCAP_CALL_LOG(logLevel, ...)                   \
    do                                                 \
    {                                                  \
        if ((static_cast<uint32_t>(logLevel) &         \
             static_cast<uint32_t>(globalLogLevel)) == \
            static_cast<uint32_t>(logLevel))           \
        {                                              \
            __VA_ARGS__;                               \
        }                                              \
    } while (0)

#define CCAP_LOG(logLevel, ...) CCAP_CALL_LOG(logLevel, fprintf(stderr, __VA_ARGS__))

#define CCAP_LOG_E(...) CCAP_LOG(LogLevel::Error, __VA_ARGS__)
#define CCAP_LOG_W(...) CCAP_LOG(LogLevel::Warning, __VA_ARGS__)
#define CCAP_LOG_I(...) CCAP_LOG(LogLevel::Info, __VA_ARGS__)
#define CCAP_LOG_V(...) CCAP_LOG(LogLevel::Verbose, __VA_ARGS__)
#else

#if __cplusplus >= 201703L
#define CCAP_CONSTEXPR constexpr
#else
#define CCAP_CONSTEXPR
#endif

inline CCAP_CONSTEXPR bool errorLogEnabled() { return false; }
inline CCAP_CONSTEXPR bool warningLogEnabled() { return false; }
inline CCAP_CONSTEXPR bool infoLogEnabled() { return false; }
inline CCAP_CONSTEXPR bool verboseLogEnabled() { return false; }

#define CCAP_LOG_E(...) ((void)0)
#define CCAP_LOG_W(...) ((void)0)
#define CCAP_LOG_I(...) ((void)0)
#define CCAP_LOG_V(...) ((void)0)
#endif

} // namespace ccap

#endif // CCAP_UTILS_H