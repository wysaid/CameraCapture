/**
 * @file ccap_android_utils.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Android-specific utility functions implementation
 * @date 2025-04
 */

#if defined(__ANDROID__)

#include "ccap_android.h"
#include "ccap_imp_android.h"
#include <android/log.h>

#define LOG_TAG "ccap_android_utils"
#define CCAP_ANDROID_LOG(level, fmt, ...) __android_log_print(level, LOG_TAG, fmt, ##__VA_ARGS__)

namespace ccap {
namespace android {

// Global Android context storage
static jobject g_applicationContext = nullptr;
static JavaVM* g_cachedJavaVM = nullptr;
static std::mutex g_contextMutex;

void setJavaVM(JavaVM* vm) {
    ProviderAndroid::setJavaVM(vm);
    g_cachedJavaVM = vm;
}

JavaVM* getJavaVM() {
    return ProviderAndroid::getJavaVM();
}

bool initialize(JNIEnv* env, jobject context) {
    std::lock_guard<std::mutex> lock(g_contextMutex);
    
    if (g_applicationContext) {
        env->DeleteGlobalRef(g_applicationContext);
    }
    
    // Store global reference to application context
    g_applicationContext = env->NewGlobalRef(context);
    
    if (!g_applicationContext) {
        CCAP_ANDROID_LOG(ANDROID_LOG_ERROR, "Failed to create global reference to context");
        return false;
    }
    
    CCAP_ANDROID_LOG(ANDROID_LOG_INFO, "Android camera system initialized");
    return true;
}

bool checkCameraPermissions(JNIEnv* env, jobject context) {
    // Get the Context class
    jclass contextClass = env->GetObjectClass(context);
    if (!contextClass) {
        return false;
    }
    
    // Get the checkSelfPermission method
    jmethodID checkPermissionMethod = env->GetMethodID(contextClass, "checkSelfPermission", 
                                                      "(Ljava/lang/String;)I");
    if (!checkPermissionMethod) {
        env->DeleteLocalRef(contextClass);
        return false;
    }
    
    // Create camera permission string
    jstring cameraPermission = env->NewStringUTF("android.permission.CAMERA");
    if (!cameraPermission) {
        env->DeleteLocalRef(contextClass);
        return false;
    }
    
    // Check permission
    jint result = env->CallIntMethod(context, checkPermissionMethod, cameraPermission);
    
    // Cleanup
    env->DeleteLocalRef(cameraPermission);
    env->DeleteLocalRef(contextClass);
    
    // Permission granted = 0 (PackageManager.PERMISSION_GRANTED)
    return result == 0;
}

CameraConfig getRecommendedConfig(const std::string& cameraId) {
    CameraConfig config;
    
    // Basic Android camera configuration recommendations
    if (cameraId == "0") {
        // Back camera - typically higher resolution
        config.width = 1920;
        config.height = 1080;
        config.frameRate = 30.0;
    } else if (cameraId == "1") {
        // Front camera - moderate resolution for video calls
        config.width = 1280;
        config.height = 720;
        config.frameRate = 30.0;
    } else {
        // Default configuration
        config.width = 1280;
        config.height = 720;
        config.frameRate = 30.0;
    }
    
    // Use YUV420P as default format (most compatible)
    config.pixelFormat = PixelFormat::YUV420P;
    config.bufferCount = 3; // Triple buffering
    
    return config;
}

jobject getApplicationContext() {
    std::lock_guard<std::mutex> lock(g_contextMutex);
    return g_applicationContext;
}

void cleanup() {
    std::lock_guard<std::mutex> lock(g_contextMutex);
    
    if (g_applicationContext && g_cachedJavaVM) {
        JNIEnv* env = nullptr;
        if (g_cachedJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
            env->DeleteGlobalRef(g_applicationContext);
        }
        g_applicationContext = nullptr;
    }
}

} // namespace android
} // namespace ccap

// Export C functions for easier JNI integration
extern "C" {

JNIEXPORT jboolean JNICALL Java_org_wysaid_ccap_CcapAndroidHelper_nativeInitialize
(JNIEnv* env, jclass clazz, jobject context) {
    return ccap::android::initialize(env, context) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_org_wysaid_ccap_CcapAndroidHelper_nativeCheckCameraPermissions
(JNIEnv* env, jclass clazz, jobject context) {
    return ccap::android::checkCameraPermissions(env, context) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_org_wysaid_ccap_CcapAndroidHelper_nativeCleanup
(JNIEnv* env, jclass clazz) {
    ccap::android::cleanup();
}

} // extern "C"

#endif // __ANDROID__