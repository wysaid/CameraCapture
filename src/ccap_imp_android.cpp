/**
 * @file ccap_imp_android.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Android implementation of ccap::Provider class using Camera2 API.
 * @date 2025-04
 *
 */

#if defined(__ANDROID__)

#include "ccap_imp_android.h"
#include "ccap_convert.h"
#include "ccap_utils.h"

#include <android/log.h>
#include <cassert>
#include <chrono>
#include <climits>
#include <cstring>

#define LOG_TAG "ccap_android"
#define CCAP_ANDROID_LOG(level, fmt, ...) __android_log_print(level, LOG_TAG, fmt, ##__VA_ARGS__)

namespace ccap {

// Global JavaVM storage for Android
static JavaVM* g_javaVM = nullptr;
static std::mutex g_javaVMMutex;

// Common Android camera formats
enum AndroidFormat {
    ANDROID_FORMAT_YUV_420_888 = 0x23,
    ANDROID_FORMAT_NV16 = 0x10,
    ANDROID_FORMAT_NV21 = 0x11,
    ANDROID_FORMAT_RGB_565 = 0x4,
    ANDROID_FORMAT_RGBA_8888 = 0x1
};

ProviderAndroid::ProviderAndroid() {
    CCAP_LOG_V("ccap: ProviderAndroid created\n");
    m_lifeHolder = std::make_shared<int>(1); // Keep the provider alive while frames are being processed
    
    // Get global JavaVM
    {
        std::lock_guard<std::mutex> lock(g_javaVMMutex);
        m_javaVM = g_javaVM;
    }
    
    if (!initializeJNI()) {
        CCAP_LOG_E("ccap: Failed to initialize JNI\n");
        reportError(ErrorCode::InitializationFailed, "Failed to initialize JNI for Android camera");
    }
}

ProviderAndroid::~ProviderAndroid() {
    std::weak_ptr<void> holder = m_lifeHolder;
    m_lifeHolder.reset(); // Release the life holder to allow cleanup
    while (!holder.expired()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wait for cleanup
        CCAP_LOG_W("ccap: life holder is in use, waiting for cleanup...\n");
    }

    close();
    releaseJNI();
    CCAP_LOG_V("ccap: ProviderAndroid destroyed\n");
}

std::vector<std::string> ProviderAndroid::findDeviceNames() {
    if (!m_cameraHelperInstance) {
        CCAP_LOG_E("ccap: Camera helper not initialized\n");
        return {};
    }

    return getCameraIdList();
}

bool ProviderAndroid::open(std::string_view deviceName) {
    if (m_isOpened) {
        CCAP_LOG_W("ccap: Camera already opened\n");
        return true;
    }

    m_cameraId = std::string(deviceName);
    m_deviceName = std::string(deviceName);

    if (!setupCamera()) {
        CCAP_LOG_E("ccap: Failed to setup camera %s\n", m_cameraId.c_str());
        return false;
    }

    if (!openCameraDevice(m_cameraId)) {
        CCAP_LOG_E("ccap: Failed to open camera device %s\n", m_cameraId.c_str());
        return false;
    }

    m_isOpened = true;
    CCAP_LOG_I("ccap: Camera %s opened successfully\n", m_cameraId.c_str());
    return true;
}

bool ProviderAndroid::isOpened() const {
    return m_isOpened;
}

std::optional<DeviceInfo> ProviderAndroid::getDeviceInfo() const {
    if (!m_isOpened) {
        return std::nullopt;
    }

    DeviceInfo deviceInfo;
    deviceInfo.name = m_deviceName;
    deviceInfo.description = "Android Camera " + m_cameraId;
    deviceInfo.resolutions = m_supportedResolutions;
    deviceInfo.pixelFormats = {PixelFormat::YUV420P, PixelFormat::NV21, PixelFormat::RGBA};

    return deviceInfo;
}

void ProviderAndroid::close() {
    if (!m_isOpened) {
        return;
    }

    stop();
    closeCameraDevice();
    m_isOpened = false;
    CCAP_LOG_I("ccap: Camera %s closed\n", m_cameraId.c_str());
}

bool ProviderAndroid::start() {
    if (!m_isOpened) {
        CCAP_LOG_E("ccap: Camera not opened\n");
        reportError(ErrorCode::DeviceNotOpen, "Camera not opened");
        return false;
    }

    if (m_isCapturing) {
        CCAP_LOG_W("ccap: Camera already capturing\n");
        return true;
    }

    if (!configureSession()) {
        CCAP_LOG_E("ccap: Failed to configure capture session\n");
        return false;
    }

    if (!startCapture()) {
        CCAP_LOG_E("ccap: Failed to start capture\n");
        return false;
    }

    m_shouldStop = false;
    m_startTime = std::chrono::steady_clock::now();
    m_frameIndex = 0;

    // Start capture thread
    m_captureThread = std::make_unique<std::thread>(&ProviderAndroid::captureThread, this);

    m_isCapturing = true;
    CCAP_LOG_I("ccap: Camera capture started\n");
    return true;
}

void ProviderAndroid::stop() {
    if (!m_isCapturing) {
        return;
    }

    m_shouldStop = true;

    // Wake up capture thread
    {
        std::lock_guard<std::mutex> lock(m_captureMutex);
        m_captureCondition.notify_all();
    }

    // Wait for capture thread to finish
    if (m_captureThread && m_captureThread->joinable()) {
        m_captureThread->join();
        m_captureThread.reset();
    }

    stopCapture();
    m_isCapturing = false;
    CCAP_LOG_I("ccap: Camera capture stopped\n");
}

bool ProviderAndroid::isStarted() const {
    return m_isCapturing;
}

bool ProviderAndroid::setupCamera() {
    if (!getCameraCharacteristics(m_cameraId)) {
        CCAP_LOG_E("ccap: Failed to get camera characteristics for %s\n", m_cameraId.c_str());
        return false;
    }

    // Set default format and resolution
    if (!m_supportedResolutions.empty()) {
        const auto& res = m_supportedResolutions[0];
        m_currentWidth = res.width;
        m_currentHeight = res.height;
    } else {
        m_currentWidth = 640;
        m_currentHeight = 480;
    }

    m_currentFormat = ANDROID_FORMAT_YUV_420_888; // Default format
    return true;
}

bool ProviderAndroid::configureSession() {
    return createCaptureSession();
}

void ProviderAndroid::releaseCamera() {
    destroyCaptureSession();
    closeCameraDevice();
}

bool ProviderAndroid::startCapture() {
    if (!m_cameraHelperInstance) {
        return false;
    }

    JNIEnv* env = nullptr;
    if (m_javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return false;
    }

    jboolean result = env->CallBooleanMethod(m_cameraHelperInstance, m_startCaptureMethod);
    return result == JNI_TRUE;
}

void ProviderAndroid::stopCapture() {
    if (!m_cameraHelperInstance) {
        return;
    }

    JNIEnv* env = nullptr;
    if (m_javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return;
    }

    env->CallVoidMethod(m_cameraHelperInstance, m_stopCaptureMethod);
}

void ProviderAndroid::captureThread() {
    CCAP_LOG_V("ccap: Capture thread started\n");

    while (!m_shouldStop) {
        std::unique_lock<std::mutex> lock(m_captureMutex);
        
        // Wait for frame or stop signal
        m_captureCondition.wait(lock, [this] { 
            return m_shouldStop || !m_frameQueue.empty(); 
        });

        if (m_shouldStop) {
            break;
        }

        // Process available frames
        while (!m_frameQueue.empty() && !m_shouldStop) {
            AndroidBuffer buffer = m_frameQueue.front();
            m_frameQueue.pop_front();
            lock.unlock();

            processFrame(buffer);

            lock.lock();
        }
    }

    CCAP_LOG_V("ccap: Capture thread stopped\n");
}

bool ProviderAndroid::processFrame(AndroidBuffer& buffer) {
    // Create VideoFrame from Android buffer
    auto frame = getFreeFrame();
    if (!frame) {
        CCAP_LOG_W("ccap: No free frame available\n");
        return false;
    }

    // Set frame properties
    frame->width = buffer.width;
    frame->height = buffer.height;
    frame->pixelFormat = androidFormatToCcapFormat(buffer.format);
    frame->timestamp = buffer.timestamp;
    frame->frameIndex = m_frameIndex++;
    frame->orientation = FrameOrientation::Portrait; // Default for mobile

    // Copy image data
    frame->sizeInBytes = buffer.size;
    
    if (buffer.format == ANDROID_FORMAT_YUV_420_888) {
        // Handle YUV420 with potential padding/stride
        frame->stride[0] = buffer.stride[0];
        frame->stride[1] = buffer.stride[1];
        frame->stride[2] = buffer.stride[2];
        frame->data[0] = buffer.planes[0];
        frame->data[1] = buffer.planes[1];
        frame->data[2] = buffer.planes[2];
    } else {
        // Single plane format
        frame->stride[0] = buffer.stride[0];
        frame->data[0] = buffer.data;
    }

    // Set frame lifetime management
    std::weak_ptr<void> lifeHolder = m_lifeHolder;
    frame->nativeHandle = nullptr;

    // Deliver frame
    newFrameAvailable(frame);
    return true;
}

bool ProviderAndroid::initializeJNI() {
    // For Android, the JavaVM should be set globally or passed from application
    // This is a simplified implementation - in real usage, the JavaVM would be
    // obtained from the Android application context
    if (!m_javaVM) {
        CCAP_LOG_E("ccap: JavaVM not initialized. Call setJavaVM() first.\n");
        return false;
    }

    // Get the current JNI environment
    JNIEnv* env = nullptr;
    if (m_javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        CCAP_LOG_E("ccap: Failed to get JNI environment\n");
        return false;
    }

    m_jniEnv = env;

    // Find the camera helper class (this would be implemented in Java)
    m_cameraHelperClass = env->FindClass("org/wysaid/ccap/CameraHelper");
    if (!m_cameraHelperClass) {
        CCAP_LOG_E("ccap: Failed to find CameraHelper class\n");
        return false;
    }

    // Get method IDs
    jmethodID constructor = env->GetMethodID(m_cameraHelperClass, "<init>", "(J)V");
    m_getCameraListMethod = env->GetMethodID(m_cameraHelperClass, "getCameraList", "()[Ljava/lang/String;");
    m_openCameraMethod = env->GetMethodID(m_cameraHelperClass, "openCamera", "(Ljava/lang/String;)Z");
    m_closeCameraMethod = env->GetMethodID(m_cameraHelperClass, "closeCamera", "()V");
    m_startCaptureMethod = env->GetMethodID(m_cameraHelperClass, "startCapture", "()Z");
    m_stopCaptureMethod = env->GetMethodID(m_cameraHelperClass, "stopCapture", "()V");
    m_getCameraSizesMethod = env->GetMethodID(m_cameraHelperClass, "getSupportedSizes", "(Ljava/lang/String;)[I");

    if (!constructor || !m_getCameraListMethod || !m_openCameraMethod || 
        !m_closeCameraMethod || !m_startCaptureMethod || !m_stopCaptureMethod || !m_getCameraSizesMethod) {
        CCAP_LOG_E("ccap: Failed to get method IDs from CameraHelper\n");
        return false;
    }

    // Create helper instance with native pointer
    m_cameraHelperInstance = env->NewObject(m_cameraHelperClass, constructor, (jlong)this);
    if (!m_cameraHelperInstance) {
        CCAP_LOG_E("ccap: Failed to create CameraHelper instance\n");
        return false;
    }

    // Make it a global reference so it doesn't get GC'd
    m_cameraHelperInstance = env->NewGlobalRef(m_cameraHelperInstance);

    return true;
}

void ProviderAndroid::releaseJNI() {
    if (m_jniEnv && m_cameraHelperInstance) {
        m_jniEnv->DeleteGlobalRef(m_cameraHelperInstance);
        m_cameraHelperInstance = nullptr;
    }
    m_jniEnv = nullptr;
}

std::vector<std::string> ProviderAndroid::getCameraIdList() {
    std::vector<std::string> cameraIds;
    
    if (!m_jniEnv || !m_cameraHelperInstance) {
        return cameraIds;
    }

    jobjectArray cameraArray = (jobjectArray)m_jniEnv->CallObjectMethod(
        m_cameraHelperInstance, m_getCameraListMethod);
    
    if (!cameraArray) {
        return cameraIds;
    }

    jsize length = m_jniEnv->GetArrayLength(cameraArray);
    for (jsize i = 0; i < length; i++) {
        jstring cameraId = (jstring)m_jniEnv->GetObjectArrayElement(cameraArray, i);
        const char* cameraIdStr = m_jniEnv->GetStringUTFChars(cameraId, nullptr);
        cameraIds.emplace_back(cameraIdStr);
        m_jniEnv->ReleaseStringUTFChars(cameraId, cameraIdStr);
        m_jniEnv->DeleteLocalRef(cameraId);
    }

    m_jniEnv->DeleteLocalRef(cameraArray);
    return cameraIds;
}

bool ProviderAndroid::openCameraDevice(const std::string& cameraId) {
    if (!m_jniEnv || !m_cameraHelperInstance) {
        return false;
    }

    jstring jCameraId = m_jniEnv->NewStringUTF(cameraId.c_str());
    jboolean result = m_jniEnv->CallBooleanMethod(m_cameraHelperInstance, m_openCameraMethod, jCameraId);
    m_jniEnv->DeleteLocalRef(jCameraId);

    return result == JNI_TRUE;
}

void ProviderAndroid::closeCameraDevice() {
    if (!m_jniEnv || !m_cameraHelperInstance) {
        return;
    }

    m_jniEnv->CallVoidMethod(m_cameraHelperInstance, m_closeCameraMethod);
}

bool ProviderAndroid::createCaptureSession() {
    // This would typically configure the capture session parameters
    // For now, we assume the Java side handles session creation
    return true;
}

void ProviderAndroid::destroyCaptureSession() {
    // Cleanup capture session
}

bool ProviderAndroid::getCameraCharacteristics(const std::string& cameraId) {
    if (!m_jniEnv || !m_cameraHelperInstance) {
        return false;
    }

    // Get supported resolutions
    jstring jCameraId = m_jniEnv->NewStringUTF(cameraId.c_str());
    jintArray sizesArray = (jintArray)m_jniEnv->CallObjectMethod(
        m_cameraHelperInstance, m_getCameraSizesMethod, jCameraId);
    m_jniEnv->DeleteLocalRef(jCameraId);

    if (!sizesArray) {
        return false;
    }

    jsize length = m_jniEnv->GetArrayLength(sizesArray);
    jint* sizes = m_jniEnv->GetIntArrayElements(sizesArray, nullptr);

    m_supportedResolutions.clear();
    for (jsize i = 0; i < length; i += 2) {
        if (i + 1 < length) {
            DeviceInfo::Resolution res;
            res.width = sizes[i];
            res.height = sizes[i + 1];
            m_supportedResolutions.push_back(res);
        }
    }

    m_jniEnv->ReleaseIntArrayElements(sizesArray, sizes, JNI_ABORT);
    m_jniEnv->DeleteLocalRef(sizesArray);

    // Add common supported formats
    m_supportedFormats = {ANDROID_FORMAT_YUV_420_888, ANDROID_FORMAT_NV21, ANDROID_FORMAT_RGBA_8888};

    return true;
}

PixelFormat ProviderAndroid::androidFormatToCcapFormat(int32_t androidFormat) {
    switch (androidFormat) {
        case ANDROID_FORMAT_YUV_420_888:
            return PixelFormat::YUV420P;
        case ANDROID_FORMAT_NV21:
            return PixelFormat::NV21;
        case ANDROID_FORMAT_NV16:
            return PixelFormat::NV16;
        case ANDROID_FORMAT_RGB_565:
            return PixelFormat::RGB565;
        case ANDROID_FORMAT_RGBA_8888:
            return PixelFormat::RGBA;
        default:
            return PixelFormat::YUV420P; // Fallback
    }
}

int32_t ProviderAndroid::ccapFormatToAndroidFormat(PixelFormat ccapFormat) {
    switch (ccapFormat) {
        case PixelFormat::YUV420P:
            return ANDROID_FORMAT_YUV_420_888;
        case PixelFormat::NV21:
            return ANDROID_FORMAT_NV21;
        case PixelFormat::NV16:
            return ANDROID_FORMAT_NV16;
        case PixelFormat::RGB565:
            return ANDROID_FORMAT_RGB_565;
        case PixelFormat::RGBA:
            return ANDROID_FORMAT_RGBA_8888;
        default:
            return ANDROID_FORMAT_YUV_420_888; // Fallback
    }
}

const char* ProviderAndroid::getFormatName(int32_t androidFormat) {
    switch (androidFormat) {
        case ANDROID_FORMAT_YUV_420_888: return "YUV_420_888";
        case ANDROID_FORMAT_NV21: return "NV21";
        case ANDROID_FORMAT_NV16: return "NV16";
        case ANDROID_FORMAT_RGB_565: return "RGB_565";
        case ANDROID_FORMAT_RGBA_8888: return "RGBA_8888";
        default: return "UNKNOWN";
    }
}

// JNI callback handlers - these would be called from the Java CameraHelper
void ProviderAndroid::onImageAvailable(JNIEnv* env, jobject thiz, jlong nativePtr, jobject image) {
    ProviderAndroid* provider = reinterpret_cast<ProviderAndroid*>(nativePtr);
    if (provider) {
        provider->handleImageAvailable(image);
    }
}

void ProviderAndroid::onCameraDisconnected(JNIEnv* env, jobject thiz, jlong nativePtr) {
    ProviderAndroid* provider = reinterpret_cast<ProviderAndroid*>(nativePtr);
    if (provider) {
        provider->handleCameraDisconnected();
    }
}

void ProviderAndroid::onCameraError(JNIEnv* env, jobject thiz, jlong nativePtr, jint error) {
    ProviderAndroid* provider = reinterpret_cast<ProviderAndroid*>(nativePtr);
    if (provider) {
        provider->handleCameraError(error);
    }
}

void ProviderAndroid::handleImageAvailable(jobject image) {
    // This would extract image data from Android Image object
    // For now, we create a placeholder buffer
    AndroidBuffer buffer{};
    buffer.width = m_currentWidth;
    buffer.height = m_currentHeight;
    buffer.format = m_currentFormat;
    buffer.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // In a real implementation, you would extract the actual image data here
    // using JNI calls to get the Image.Plane data

    {
        std::lock_guard<std::mutex> lock(m_captureMutex);
        if (m_frameQueue.size() >= kMaxFrameQueue) {
            m_frameQueue.pop_front(); // Drop oldest frame
        }
        m_frameQueue.push_back(buffer);
        m_captureCondition.notify_one();
    }
}

void ProviderAndroid::handleCameraDisconnected() {
    CCAP_LOG_W("ccap: Camera disconnected\n");
    reportError(ErrorCode::DeviceDisconnected, "Camera disconnected");
}

void ProviderAndroid::handleCameraError(int error) {
    CCAP_LOG_E("ccap: Camera error: %d\n", error);
    reportError(ErrorCode::FrameCaptureFailed, "Camera error: " + std::to_string(error));
}

// Factory function
ProviderImp* createProviderAndroid() {
    return new ProviderAndroid();
}

// Global JavaVM management functions
void ProviderAndroid::setJavaVM(JavaVM* vm) {
    std::lock_guard<std::mutex> lock(g_javaVMMutex);
    g_javaVM = vm;
}

JavaVM* ProviderAndroid::getJavaVM() {
    std::lock_guard<std::mutex> lock(g_javaVMMutex);
    return g_javaVM;
}

} // namespace ccap

// JNI function registration - this would be called from Java when the library is loaded
extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // Store the JavaVM pointer globally
    ccap::ProviderAndroid::setJavaVM(vm);
    return JNI_VERSION_1_6;
}

// Native methods that would be called from Java CameraHelper
JNIEXPORT void JNICALL Java_org_wysaid_ccap_CameraHelper_nativeOnImageAvailable
(JNIEnv* env, jobject obj, jlong nativePtr, jobject image) {
    ccap::ProviderAndroid::onImageAvailable(env, obj, nativePtr, image);
}

JNIEXPORT void JNICALL Java_org_wysaid_ccap_CameraHelper_nativeOnCameraDisconnected
(JNIEnv* env, jobject obj, jlong nativePtr) {
    ccap::ProviderAndroid::onCameraDisconnected(env, obj, nativePtr);
}

JNIEXPORT void JNICALL Java_org_wysaid_ccap_CameraHelper_nativeOnCameraError
(JNIEnv* env, jobject obj, jlong nativePtr, jint error) {
    ccap::ProviderAndroid::onCameraError(env, obj, nativePtr, error);
}

} // extern "C"

#endif // __ANDROID__