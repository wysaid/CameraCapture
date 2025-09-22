/**
 * @file android_camera_example.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Example showing how to use CCAP with Android Camera2 backend
 * @date 2025-04
 */

#if defined(__ANDROID__)

#include <ccap.h>
#include <ccap_android.h>
#include <android/log.h>
#include <jni.h>
#include <memory>
#include <vector>

#define LOG_TAG "AndroidCameraExample"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class AndroidCameraExample {
private:
    std::unique_ptr<ccap::Provider> m_provider;
    std::atomic<bool> m_isCapturing{false};
    std::atomic<uint64_t> m_frameCount{0};

public:
    AndroidCameraExample() = default;
    ~AndroidCameraExample() = default;

    bool initialize() {
        // Create provider - will automatically use Android backend on Android
        m_provider = std::make_unique<ccap::Provider>();
        
        if (!m_provider) {
            LOGE("Failed to create camera provider");
            return false;
        }

        LOGI("Camera provider created successfully");
        return true;
    }

    std::vector<std::string> listCameras() {
        if (!m_provider) {
            return {};
        }

        auto cameras = m_provider->findDeviceNames();
        LOGI("Found %zu cameras:", cameras.size());
        for (size_t i = 0; i < cameras.size(); ++i) {
            LOGI("  Camera %zu: %s", i, cameras[i].c_str());
        }
        
        return cameras;
    }

    bool openCamera(const std::string& cameraId) {
        if (!m_provider) {
            LOGE("Provider not initialized");
            return false;
        }

        // Get recommended configuration for this camera
        auto config = ccap::android::getRecommendedConfig(cameraId);
        
        // Configure camera properties
        m_provider->set(ccap::PropertyName::Width, config.width);
        m_provider->set(ccap::PropertyName::Height, config.height);
        m_provider->set(ccap::PropertyName::FrameRate, config.frameRate);
        m_provider->set(ccap::PropertyName::PixelFormatOutput, 
                       static_cast<double>(config.pixelFormat));

        // Open camera
        if (!m_provider->open(cameraId)) {
            LOGE("Failed to open camera: %s", cameraId.c_str());
            return false;
        }

        // Get device info
        auto deviceInfo = m_provider->getDeviceInfo();
        if (deviceInfo) {
            LOGI("Camera opened: %s", deviceInfo->name.c_str());
            LOGI("Description: %s", deviceInfo->description.c_str());
            LOGI("Supported resolutions: %zu", deviceInfo->resolutions.size());
        }

        return true;
    }

    bool startCapture() {
        if (!m_provider || !m_provider->isOpened()) {
            LOGE("Camera not opened");
            return false;
        }

        // Set frame callback
        m_provider->setNewFrameCallback([this](const std::shared_ptr<ccap::VideoFrame>& frame) {
            return this->onNewFrame(frame);
        });

        // Start capture
        if (!m_provider->start()) {
            LOGE("Failed to start capture");
            return false;
        }

        m_isCapturing = true;
        m_frameCount = 0;
        LOGI("Camera capture started");
        return true;
    }

    void stopCapture() {
        if (m_provider && m_isCapturing) {
            m_provider->stop();
            m_isCapturing = false;
            LOGI("Camera capture stopped. Total frames: %llu", 
                 (unsigned long long)m_frameCount.load());
        }
    }

    void closeCamera() {
        stopCapture();
        if (m_provider) {
            m_provider->close();
            LOGI("Camera closed");
        }
    }

    uint64_t getFrameCount() const {
        return m_frameCount.load();
    }

    bool isCapturing() const {
        return m_isCapturing.load();
    }

private:
    bool onNewFrame(const std::shared_ptr<ccap::VideoFrame>& frame) {
        if (!frame) {
            return false;
        }

        // Count frames
        uint64_t count = ++m_frameCount;
        
        // Log every 30th frame to avoid spam
        if (count % 30 == 0) {
            LOGI("Frame #%llu: %dx%d, format=%d, timestamp=%llu",
                 (unsigned long long)count,
                 frame->width, frame->height,
                 static_cast<int>(frame->pixelFormat),
                 (unsigned long long)frame->timestamp);
        }

        // Process frame data here
        // frame->data[0] contains the image data
        // frame->stride[0] contains the row stride
        // For YUV formats, frame->data[1] and frame->data[2] contain U and V planes

        // Example: Save every 100th frame (pseudo-code)
        if (count % 100 == 0) {
            saveFrameToFile(frame);
        }

        return true; // Return true to continue receiving frames
    }

    void saveFrameToFile(const std::shared_ptr<ccap::VideoFrame>& frame) {
        // In a real implementation, you would save the frame to storage
        // This could involve:
        // 1. Converting to JPEG/PNG using Android's bitmap APIs
        // 2. Saving to external storage with proper permissions
        // 3. Using MediaStore APIs for gallery integration
        
        LOGI("Saving frame %llu to file (simulated)", 
             (unsigned long long)frame->frameIndex);
    }
};

// Global example instance
static std::unique_ptr<AndroidCameraExample> g_example;

extern "C" {

// JNI functions called from Java

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeInitialize(JNIEnv* env, jobject thiz, jobject context) {
    // Initialize CCAP Android system
    if (!ccap::android::initialize(env, context)) {
        LOGE("Failed to initialize CCAP Android system");
        return JNI_FALSE;
    }

    // Create example instance
    g_example = std::make_unique<AndroidCameraExample>();
    if (!g_example->initialize()) {
        LOGE("Failed to initialize camera example");
        g_example.reset();
        return JNI_FALSE;
    }

    LOGI("Android camera example initialized successfully");
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_example_ccap_MainActivity_nativeGetCameraList(JNIEnv* env, jobject thiz) {
    if (!g_example) {
        return nullptr;
    }

    auto cameras = g_example->listCameras();
    
    // Convert to Java string array
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(cameras.size(), stringClass, nullptr);
    
    for (size_t i = 0; i < cameras.size(); ++i) {
        jstring cameraId = env->NewStringUTF(cameras[i].c_str());
        env->SetObjectArrayElement(result, i, cameraId);
        env->DeleteLocalRef(cameraId);
    }
    
    env->DeleteLocalRef(stringClass);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeOpenCamera(JNIEnv* env, jobject thiz, jstring cameraId) {
    if (!g_example) {
        return JNI_FALSE;
    }

    const char* cameraIdStr = env->GetStringUTFChars(cameraId, nullptr);
    bool result = g_example->openCamera(cameraIdStr);
    env->ReleaseStringUTFChars(cameraId, cameraIdStr);
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeStartCapture(JNIEnv* env, jobject thiz) {
    if (!g_example) {
        return JNI_FALSE;
    }
    
    return g_example->startCapture() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_example_ccap_MainActivity_nativeStopCapture(JNIEnv* env, jobject thiz) {
    if (g_example) {
        g_example->stopCapture();
    }
}

JNIEXPORT void JNICALL
Java_com_example_ccap_MainActivity_nativeCloseCamera(JNIEnv* env, jobject thiz) {
    if (g_example) {
        g_example->closeCamera();
    }
}

JNIEXPORT jlong JNICALL
Java_com_example_ccap_MainActivity_nativeGetFrameCount(JNIEnv* env, jobject thiz) {
    if (!g_example) {
        return 0;
    }
    
    return static_cast<jlong>(g_example->getFrameCount());
}

JNIEXPORT jboolean JNICALL
Java_com_example_ccap_MainActivity_nativeIsCapturing(JNIEnv* env, jobject thiz) {
    if (!g_example) {
        return JNI_FALSE;
    }
    
    return g_example->isCapturing() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_example_ccap_MainActivity_nativeCleanup(JNIEnv* env, jobject thiz) {
    g_example.reset();
    ccap::android::cleanup();
    LOGI("Android camera example cleaned up");
}

} // extern "C"

#endif // __ANDROID__