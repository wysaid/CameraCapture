/**
 * @file ccap_config.h
 * @author wysaid (this@wysaid.org)
 * @brief Common configuration and platform-specific definitions for ccap library.
 * @date 2025-09
 * 
 * @note This file contains common macros, constants, and platform detection
 *       that are shared across both C++ and C interfaces of the library.
 */

#pragma once
#ifndef CCAP_CONFIG_H
#define CCAP_CONFIG_H

/* ========== Export/Import Macro Definitions ========== */

// Define CCAP_EXPORT macro for symbol export/import
#if defined(CCAP_SHARED)
    #if defined(_WIN32) || defined(__CYGWIN__)
        #ifdef CCAP_BUILDING_DLL
            #ifdef __GNUC__
                #define CCAP_EXPORT __attribute__ ((dllexport))
            #else
                #define CCAP_EXPORT __declspec(dllexport)
            #endif
        #else
            #ifdef __GNUC__
                #define CCAP_EXPORT __attribute__ ((dllimport))
            #else
                #define CCAP_EXPORT __declspec(dllimport)
            #endif
        #endif
    #else
        #if __GNUC__ >= 4
            #define CCAP_EXPORT __attribute__ ((visibility ("default")))
        #else
            #define CCAP_EXPORT
        #endif
    #endif
#else
    // Static library - no export needed
    #define CCAP_EXPORT
#endif

/* ========== Platform Detection ========== */

#if __APPLE__
#include <TargetConditionals.h>
#if (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define CCAP_IOS 1
#else
#define CCAP_MACOS 1
#endif
#elif defined(__ANDROID__)
#define CCAP_ANDROID 1
#elif defined(_WIN32)
#define CCAP_WINDOWS 1
#if defined(_MSC_VER)
#define CCAP_WINDOWS_MSVC 1
#endif
#endif

#if !defined(CCAP_IOS) && !defined(CCAP_ANDROID)
#define CCAP_DESKTOP 1
#endif

/* ========== Common Constants ========== */

// Maximum number of devices supported
#define CCAP_MAX_DEVICES 32

// Maximum length for device name strings  
#define CCAP_MAX_DEVICE_NAME_LENGTH 128

// Maximum number of pixel formats per device
#define CCAP_MAX_PIXEL_FORMATS 32

// Maximum number of resolutions per device
#define CCAP_MAX_RESOLUTIONS 64

/* ========== Compatibility Macros ========== */

#ifdef __cplusplus
    #if __cplusplus >= 201703L
        #define CCAP_CONSTEXPR constexpr
    #else
        #define CCAP_CONSTEXPR
    #endif
#else
    #define CCAP_CONSTEXPR
#endif

#endif /* CCAP_CONFIG_H */