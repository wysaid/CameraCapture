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
#include <cstdio>

#ifdef _MSC_VER
#include <malloc.h>
#define ALIGNED_ALLOC(alignment, size) _aligned_malloc(size, alignment)
#define ALIGNED_FREE(ptr) _aligned_free(ptr)
#else
#define ALIGNED_ALLOC(alignment, size) std::aligned_alloc(alignment, size)
#define ALIGNED_FREE(ptr) std::free(ptr)
#endif

namespace ccap {
ProviderImp* createProviderApple();
ProviderImp* createProviderDirectShow();

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

ProviderImp* createProvider(std::string_view extraInfo) {
#if __APPLE__
    return createProviderApple();
#elif defined(_MSC_VER) || defined(_WIN32)
    return createProviderDirectShow();
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
