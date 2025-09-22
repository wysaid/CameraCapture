/**
 * @file ccap_android.h
 * @author wysaid (this@wysaid.org)
 * @brief Android-specific API extensions for ccap
 * @date 2025-04
 *
 * This header provides Android-specific functionality for the CCAP library.
 * Include this in addition to ccap.h when building Android applications.
 */

#pragma once
#ifndef CCAP_ANDROID_H
#define CCAP_ANDROID_H

#if defined(__ANDROID__)

#include "ccap.h"
#include <jni.h>

namespace ccap {

/**
 * @brief Android-specific initialization functions
 */
namespace android {

/**
 * @brief Set the JavaVM pointer for JNI operations
 * 
 * This function must be called before creating any Provider instances on Android.
 * Typically called from JNI_OnLoad or in your Application.onCreate().
 *
 * @param vm JavaVM pointer obtained from JNI
 */
void setJavaVM(JavaVM* vm);

/**
 * @brief Get the current JavaVM pointer
 * @return JavaVM pointer or nullptr if not set
 */
JavaVM* getJavaVM();

/**
 * @brief Initialize Android camera system with application context
 * 
 * This function should be called once with the Android application context.
 * The context is needed for accessing the Android Camera2 service.
 *
 * @param env JNI environment
 * @param context Android Context object (typically Application or Activity)
 * @return true if initialization successful
 */
bool initialize(JNIEnv* env, jobject context);

/**
 * @brief Check if Android camera permissions are granted
 * 
 * @param env JNI environment
 * @param context Android Context object
 * @return true if camera permissions are granted
 */
bool checkCameraPermissions(JNIEnv* env, jobject context);

/**
 * @brief Get recommended camera configuration for Android devices
 * 
 * Returns optimized camera settings based on device capabilities.
 *
 * @param cameraId Camera ID to get configuration for
 * @return Recommended configuration parameters
 */
struct CameraConfig {
    int width = 1280;
    int height = 720;
    PixelFormat pixelFormat = PixelFormat::YUV420P;
    double frameRate = 30.0;
    int bufferCount = 3;
};

CameraConfig getRecommendedConfig(const std::string& cameraId);

} // namespace android

} // namespace ccap

#endif // __ANDROID__
#endif // CCAP_ANDROID_H