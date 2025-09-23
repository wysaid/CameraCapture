/**
 * @file ccap_imp_android.h
 * @author wysaid (this@wysaid.org)
 * @brief Header file for Android implementation of ccap::Provider class using Camera2 API.
 * @date 2025-04
 *
 */

#pragma once
#ifndef CAMERA_CAPTURE_ANDROID_H
#define CAMERA_CAPTURE_ANDROID_H

#if defined(__ANDROID__)

#include "ccap_imp.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <jni.h>

namespace ccap {

/**
 * @brief Camera2-based camera provider implementation for Android
 */
class ProviderAndroid : public ProviderImp {
public:
    ProviderAndroid();
    ~ProviderAndroid() override;

    // ProviderImp interface implementation
    std::vector<std::string> findDeviceNames() override;
    bool open(std::string_view deviceName) override;
    bool isOpened() const override;
    std::optional<DeviceInfo> getDeviceInfo() const override;
    void close() override;
    bool start() override;
    void stop() override;
    bool isStarted() const override;

private:
    struct AndroidBuffer {
        uint8_t* data;
        size_t size;
        int64_t timestamp;
        int32_t width;
        int32_t height;
        int32_t format;
        size_t stride[3];
        uint8_t* planes[3];
    };

    // Internal helper methods
    bool setupCamera();
    bool configureSession();
    void releaseCamera();
    bool startCapture();
    void stopCapture();
    void captureThread();
    bool processFrame(AndroidBuffer& buffer);

    // JNI helper methods
    bool initializeJNI();
    void releaseJNI();
    bool createCameraManager();
    bool getCameraCharacteristics(const std::string& cameraId);
    std::vector<std::string> getCameraIdList();
    bool openCameraDevice(const std::string& cameraId);
    void closeCameraDevice();
    bool createCaptureSession();
    void destroyCaptureSession();
    
    // Format conversion helpers
    PixelFormat androidFormatToCcapFormat(int32_t androidFormat);
    int32_t ccapFormatToAndroidFormat(PixelFormat ccapFormat);
    const char* getFormatName(int32_t androidFormat);
    std::vector<DeviceInfo::Resolution> getSupportedResolutions(const std::string& cameraId);

    void handleImageAvailable(jobject image);
    void handleCameraDisconnected();
    void handleCameraError(int error);

public:
    // JNI callback handlers (called from Java side)
    static void onImageAvailable(JNIEnv* env, jobject thiz, jlong nativePtr, jobject image);
    static void onCameraDisconnected(JNIEnv* env, jobject thiz, jlong nativePtr);
    static void onCameraError(JNIEnv* env, jobject thiz, jlong nativePtr, jint error);

    // Global access functions for JavaVM
    static void setJavaVM(JavaVM* vm);
    static JavaVM* getJavaVM();

private:
    // Device state
    bool m_isOpened = false;
    bool m_isCapturing = false;
    std::string m_cameraId;
    std::string m_deviceName;

    // JNI state
    JavaVM* m_javaVM = nullptr;
    JNIEnv* m_jniEnv = nullptr;
    jclass m_cameraHelperClass = nullptr;
    jobject m_cameraHelperInstance = nullptr;
    jmethodID m_getCameraListMethod = nullptr;
    jmethodID m_openCameraMethod = nullptr;
    jmethodID m_closeCameraMethod = nullptr;
    jmethodID m_startCaptureMethod = nullptr;
    jmethodID m_stopCaptureMethod = nullptr;
    jmethodID m_getCameraSizesMethod = nullptr;

    // Camera characteristics
    std::vector<DeviceInfo::Resolution> m_supportedResolutions;
    std::vector<int32_t> m_supportedFormats;
    
    // Current configuration
    int32_t m_currentWidth = 0;
    int32_t m_currentHeight = 0;
    int32_t m_currentFormat = 0;

    // Capture thread and synchronization
    std::unique_ptr<std::thread> m_captureThread;
    std::atomic<bool> m_shouldStop{ false };
    std::mutex m_captureMutex;
    std::condition_variable m_captureCondition;
    std::deque<AndroidBuffer> m_frameQueue;
    static constexpr size_t kMaxFrameQueue = 3;

    // Frame management
    std::chrono::steady_clock::time_point m_startTime{};
    uint64_t m_frameIndex{ 0 };
    
    std::shared_ptr<int> m_lifeHolder; // To keep the provider alive while frames are being processed
};

// Factory function
ProviderImp* createProviderAndroid();

} // namespace ccap

#endif // __ANDROID__
#endif // CAMERA_CAPTURE_ANDROID_H