/**
 * @file ccap_core.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#include "ccap_core.h"

#include "ccap_imp.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#ifdef _MSC_VER
#include <malloc.h>
#define ALIGNED_ALLOC(alignment, size) _aligned_malloc(size, alignment)
#define ALIGNED_FREE(ptr) _aligned_free(ptr)
#elif __MINGW32__
#define ALIGNED_ALLOC(alignment, size) __mingw_aligned_malloc(size, alignment)
#define ALIGNED_FREE(ptr) __mingw_aligned_free(ptr)
#else
#define ALIGNED_ALLOC(alignment, size) std::aligned_alloc(alignment, size)
#define ALIGNED_FREE(ptr) std::free(ptr)
#endif

namespace ccap {
ProviderImp* createProviderApple();
ProviderImp* createProviderDirectShow();
ProviderImp* createProviderV4L2();

// Global error callback storage
namespace {
    std::mutex g_errorCallbackMutex;
    ErrorCallback g_globalErrorCallback;
}

void setGlobalErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(g_errorCallbackMutex);
    g_globalErrorCallback = std::move(callback);
}

ErrorCallback getGlobalErrorCallback() {
    std::lock_guard<std::mutex> lock(g_errorCallbackMutex);
    return g_globalErrorCallback;
}

Allocator::~Allocator() { CCAP_LOG_V("ccap: Allocator::~Allocator() called, this=%p\n", this); }

DefaultAllocator::~DefaultAllocator() {
    if (m_data) ALIGNED_FREE(m_data);
    CCAP_LOG_V("ccap: DefaultAllocator destroyed, deallocated %zu bytes of memory at %p, this=%p\n", m_size, m_data, this);
}

void DefaultAllocator::resize(size_t size) {
    assert(size != 0);
    if (size <= m_size && size >= m_size / 2 && m_data != nullptr) return;

    if (m_data) ALIGNED_FREE(m_data);

    // 32字节对齐，满足主流SIMD指令集需求(AVX)
    size_t alignedSize = (size + 31) & ~size_t(31);
    m_data = static_cast<uint8_t*>(ALIGNED_ALLOC(32, alignedSize));
    m_size = alignedSize;
    CCAP_LOG_V("ccap: Allocated %zu bytes of memory at %p\n", m_size, m_data);
}

VideoFrame::VideoFrame() = default;
VideoFrame::~VideoFrame() { CCAP_LOG_V("ccap: VideoFrame::VideoFrameFrame() called, this=%p\n", this); }

void VideoFrame::detach() {
    if (!allocator || data[0] != allocator->data()) {
        if (!allocator) {
            allocator = std::make_shared<DefaultAllocator>();
        }

        allocator->resize(sizeInBytes);
        // Copy data to allocator
        std::memcpy(allocator->data(), data[0], sizeInBytes);

        // Update data pointers
        data[0] = allocator->data();
        if (stride[1] > 0) {
            data[1] = data[0] + stride[0] * height;
            if (stride[2] > 0) {
                // Currently, only I420 needs to use data[2]
                data[2] = data[1] + stride[1] * height / 2;
            } else {
                data[2] = nullptr;
            }
        } else {
            data[1] = nullptr;
            data[2] = nullptr;
        }

        nativeHandle = nullptr; // Detach native handle
    }
}

ProviderImp* createProvider(std::string_view extraInfo) {
#if __APPLE__
    return createProviderApple();
#elif defined(_MSC_VER) || defined(_WIN32)
    return createProviderDirectShow();
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    return createProviderV4L2();
#else
    if (warningLogEnabled()) {
        CCAP_LOG_W("ccap: Unsupported platform!\n");
    }
#endif
    return nullptr;
}

Provider::Provider() :
    m_imp(createProvider("")) {}

Provider::~Provider() {
    CCAP_LOG_V("ccap: Provider::~Provider() called, this=%p, imp=%p\n", this, m_imp);
    delete m_imp;
}

Provider::Provider(std::string_view deviceName, std::string_view extraInfo) :
    m_imp(createProvider(extraInfo)) {
    if (m_imp) {
        open(deviceName);
    }
}

Provider::Provider(int deviceIndex, std::string_view extraInfo) :
    m_imp(createProvider(extraInfo)) {
    if (m_imp) {
        open(deviceIndex);
    }
}

std::vector<std::string> Provider::findDeviceNames() { return m_imp ? m_imp->findDeviceNames() : std::vector<std::string>(); }

bool Provider::open(std::string_view deviceName, bool autoStart) {
    return m_imp && m_imp->open(deviceName) && (!autoStart || m_imp->start());
}

bool Provider::open(int deviceIndex, bool autoStart) {
    if (!m_imp) return false;

    std::string deviceName;
    if (deviceIndex >= 0) {
        auto deviceNames = findDeviceNames();
        if (!deviceNames.empty()) {
            deviceIndex = std::min(deviceIndex, static_cast<int>(deviceNames.size()) - 1);
            deviceName = deviceNames[deviceIndex];

            CCAP_LOG_V("ccap: input device index %d, selected device name: %s\n", deviceIndex, deviceName.c_str());
        }
    }

    return open(deviceName) && (!autoStart || m_imp->start());
}

bool Provider::isOpened() const { return m_imp && m_imp->isOpened(); }

std::optional<DeviceInfo> Provider::getDeviceInfo() const { return m_imp ? m_imp->getDeviceInfo() : std::nullopt; }

void Provider::close() { m_imp->close(); }

bool Provider::start() { return m_imp && m_imp->start(); }

void Provider::stop() {
    if (m_imp) m_imp->stop();
}

bool Provider::isStarted() const { return m_imp && m_imp->isStarted(); }

bool Provider::set(PropertyName prop, double value) { return m_imp->set(prop, value); }

double Provider::get(PropertyName prop) { return m_imp ? m_imp->get(prop) : NAN; }

std::shared_ptr<VideoFrame> Provider::grab(uint32_t timeoutInMs) { return m_imp->grab(timeoutInMs); }

void Provider::setNewFrameCallback(std::function<bool(const std::shared_ptr<VideoFrame>&)> callback) {
    m_imp->setNewFrameCallback(std::move(callback));
}

void Provider::setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory) {
    m_imp->setFrameAllocator(std::move(allocatorFactory));
}

void Provider::setMaxAvailableFrameSize(uint32_t size) { m_imp->setMaxAvailableFrameSize(size); }

void Provider::setMaxCacheFrameSize(uint32_t size) { m_imp->setMaxCacheFrameSize(size); }

} // namespace ccap
