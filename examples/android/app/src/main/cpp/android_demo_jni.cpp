/**
 * @file android_demo_jni.cpp
 * @author wysaid (this@wysaid.org)
 * @brief JNI wrapper for Android CCAP demo
 * @date 2025-09
 */

#include <jni.h>
#include <android/log.h>
#include <ccap_c.h>
#include <ccap_android.h>
#include <memory>
#include <vector>
#include <string>
#include <atomic>

#define LOG_TAG "CcapDemo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global demo instance
static CcapProvider* g_provider = nullptr;
static std::atomic<bool> g_isCapturing{false};
static std::atomic<uint64_t> g_frameCount{0};
static JavaVM* g_vm = nullptr;

// Frame callback
static bool frameCallback(const CcapVideoFrame* frame, void* userData) {
    g_frameCount++;
    
    // In a real application, you would process the frame here
    if (g_frameCount % 60 == 0) {  // Log every 60 frames
        LOGI("Received frame %lu", (unsigned long)g_frameCount.load());
    }
    
    return true;  // Continue capturing
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeInitialize(JNIEnv* env, jobject thiz) {
    // Initialize Android context  
    jclass activityClass = env->GetObjectClass(thiz);
    jmethodID getApplicationContextMethod = env->GetMethodID(activityClass, 
        "getApplicationContext", "()Landroid/content/Context;");
    jobject applicationContext = env->CallObjectMethod(thiz, getApplicationContextMethod);
    
    if (!ccap::android::initialize(env, applicationContext)) {
        LOGE("Failed to initialize Android context");
        return JNI_FALSE;
    }
    
    // Create provider using C API
    g_provider = ccap_provider_create();
    if (!g_provider) {
        LOGE("Failed to create camera provider");
        return JNI_FALSE;
    }
    
    LOGI("CCAP Android demo initialized successfully");
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_example_ccap_MainActivity_nativeGetCameraList(JNIEnv* env, jobject thiz) {
    if (!g_provider) {
        return nullptr;
    }
    
    CcapDeviceNamesList deviceList;
    if (!ccap_provider_find_device_names_list(g_provider, &deviceList)) {
        return nullptr;
    }
    
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(deviceList.deviceCount, stringClass, nullptr);
    
    for (size_t i = 0; i < deviceList.deviceCount; ++i) {
        jstring cameraId = env->NewStringUTF(deviceList.deviceNames[i]);
        env->SetObjectArrayElement(result, i, cameraId);
        env->DeleteLocalRef(cameraId);
    }
    
    env->DeleteLocalRef(stringClass);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeOpenCamera(JNIEnv* env, jobject thiz, jstring cameraId) {
    if (!g_provider) {
        return JNI_FALSE;
    }
    
    const char* cameraIdStr = env->GetStringUTFChars(cameraId, nullptr);
    
    // 打开摄像头（第三个参数false表示不自动开始捕获）
    bool result = ccap_provider_open(g_provider, cameraIdStr, false);
    if (!result) {
        LOGE("Failed to open camera: %s", cameraIdStr);
        env->ReleaseStringUTFChars(cameraId, cameraIdStr);
        return JNI_FALSE;
    }
    
    // 设置回调函数
    if (!ccap_provider_set_new_frame_callback(g_provider, frameCallback, nullptr)) {
        LOGE("Failed to set frame callback");
        ccap_provider_close(g_provider);
        env->ReleaseStringUTFChars(cameraId, cameraIdStr);
        return JNI_FALSE;
    }
    
    LOGI("Camera opened successfully: %s", cameraIdStr);
    env->ReleaseStringUTFChars(cameraId, cameraIdStr);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeStartCapture(JNIEnv* env, jobject thiz) {
    if (!g_provider) {
        return JNI_FALSE;
    }
    
    g_frameCount = 0;
    g_isCapturing = true;
    
    if (!ccap_provider_start(g_provider)) {
        LOGE("Failed to start capture");
        g_isCapturing = false;
        return JNI_FALSE;
    }
    
    LOGI("Capture started successfully");
    return JNI_TRUE;
}
}

JNIEXPORT void JNICALL
Java_com_example_ccap_MainActivity_nativeStopCapture(JNIEnv* env, jobject thiz) {
    if (g_provider && g_isCapturing) {
        ccap_provider_stop(g_provider);
        g_isCapturing = false;
        LOGI("Capture stopped, total frames: %lu", (unsigned long)g_frameCount.load());
    }
}

JNIEXPORT void JNICALL
Java_com_example_ccap_MainActivity_nativeCloseCamera(JNIEnv* env, jobject thiz) {
    if (g_provider) {
        if (g_isCapturing) {
            ccap_provider_stop(g_provider);
            g_isCapturing = false;
        }
        ccap_provider_close(g_provider);
        LOGI("Camera closed");
    }
}

JNIEXPORT jlong JNICALL
Java_com_example_ccap_MainActivity_nativeGetFrameCount(JNIEnv* env, jobject thiz) {
    return static_cast<jlong>(g_frameCount.load());
}

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeIsCapturing(JNIEnv* env, jobject thiz) {
    return g_isCapturing ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_example_ccap_MainActivity_nativeCleanup(JNIEnv* env, jobject thiz) {
    if (g_provider) {
        if (g_isCapturing) {
            ccap_provider_stop(g_provider);
            g_isCapturing = false;
        }
        ccap_provider_close(g_provider);
        ccap_provider_destroy(g_provider);
        g_provider = nullptr;
    }
    // Android cleanup is handled automatically
    LOGI("CCAP Android demo cleaned up");
}