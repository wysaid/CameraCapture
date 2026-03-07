/**
 * @file ccap_core.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#include "ccap_core.h"

#include "ccap_imp.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

#if defined(_WIN32) || defined(_MSC_VER)
#include <windows.h>
#endif

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
ProviderImp* createProviderMSMF();
ProviderImp* createProviderV4L2();

// Global error callback storage
namespace {
std::mutex g_errorCallbackMutex;
ErrorCallback g_globalErrorCallback;
std::mutex g_providerStateMutex;

struct ProviderCachedState {
    std::string extraInfo;
    std::function<bool(const std::shared_ptr<VideoFrame>&)> frameCallback;
    std::function<std::shared_ptr<Allocator>()> allocatorFactory;
    uint32_t maxAvailableFrameSize = DEFAULT_MAX_AVAILABLE_FRAME_SIZE;
    uint32_t maxCacheFrameSize = DEFAULT_MAX_CACHE_FRAME_SIZE;
    int requestedWidth = 640;
    int requestedHeight = 480;
    double requestedFrameRate = 0.0;
    PixelFormat requestedInternalFormat = PixelFormat::Unknown;
    PixelFormat requestedOutputFormat{
#ifdef __APPLE__
        PixelFormat::BGRA32
#else
        PixelFormat::BGR24
#endif
    };
    bool hasFrameOrientationOverride = false;
    FrameOrientation requestedFrameOrientation = FrameOrientation::Default;
};

std::unordered_map<ProviderImp*, ProviderCachedState> g_providerStates;

ProviderCachedState makeProviderCachedState(std::string_view extraInfo = {}) {
    ProviderCachedState state;
    state.extraInfo = std::string(extraInfo);
    return state;
}

ProviderCachedState copyProviderState(ProviderImp* imp) {
    if (imp == nullptr) {
        return makeProviderCachedState();
    }

    std::lock_guard<std::mutex> lock(g_providerStateMutex);
    auto iterator = g_providerStates.find(imp);
    return iterator != g_providerStates.end() ? iterator->second : makeProviderCachedState();
}

void storeProviderState(ProviderImp* imp, ProviderCachedState state) {
    if (imp == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_providerStateMutex);
    g_providerStates[imp] = std::move(state);
}

template <typename Fn>
void updateProviderState(ProviderImp* imp, Fn&& updater) {
    if (imp == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_providerStateMutex);
    auto [iterator, inserted] = g_providerStates.emplace(imp, makeProviderCachedState());
    (void)inserted;
    updater(iterator->second);
}

void eraseProviderState(ProviderImp* imp) {
    if (imp == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_providerStateMutex);
    g_providerStates.erase(imp);
}

void transferProviderState(ProviderImp* from, ProviderImp* to) {
    if (to == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_providerStateMutex);
    auto node = g_providerStates.extract(from);
    if (node.empty()) {
        g_providerStates.emplace(to, makeProviderCachedState());
        return;
    }

    node.key() = to;
    g_providerStates.insert(std::move(node));
}

#if defined(_WIN32) || defined(_MSC_VER)
enum class WindowsBackendPreference {
    Auto,
    MSMF,
    DirectShow,
};

std::string toLowerCopy(std::string_view input) {
    std::string normalized;
    normalized.reserve(input.size());
    for (char ch : input) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return normalized;
}

std::optional<WindowsBackendPreference> parseWindowsBackendPreferenceValue(std::string_view value) {
    std::string normalized = toLowerCopy(value);
    if (normalized.empty()) {
        return std::nullopt;
    }

    constexpr const char* kBackendPrefix = "backend=";
    if (normalized.rfind(kBackendPrefix, 0) == 0) {
        normalized.erase(0, std::strlen(kBackendPrefix));
    }

    if (normalized == "auto") {
        return WindowsBackendPreference::Auto;
    }
    if (normalized == "msmf" || normalized == "mediafoundation") {
        return WindowsBackendPreference::MSMF;
    }
    if (normalized == "dshow" || normalized == "directshow") {
        return WindowsBackendPreference::DirectShow;
    }
    return std::nullopt;
}

std::string getEnvironmentValue(std::string_view name) {
#if defined(_WIN32) || defined(_MSC_VER)
    if (name.empty()) {
        return {};
    }

    std::string envName(name);
    DWORD required = GetEnvironmentVariableA(envName.c_str(), nullptr, 0);
    if (required == 0) {
        return {};
    }

    std::string value(required > 0 ? required - 1 : 0, '\0');
    if (GetEnvironmentVariableA(envName.c_str(), value.data(), required) == 0) {
        return {};
    }

    return value;
#else
    const char* rawValue = std::getenv(std::string(name).c_str());
    return rawValue != nullptr ? std::string(rawValue) : std::string();
#endif
}

WindowsBackendPreference resolveWindowsBackendPreference(std::string_view extraInfo) {
    if (auto parsed = parseWindowsBackendPreferenceValue(extraInfo)) {
        return *parsed;
    }

    std::string envValue = getEnvironmentValue("CCAP_WINDOWS_BACKEND");

    if (!envValue.empty()) {
        if (auto parsed = parseWindowsBackendPreferenceValue(envValue)) {
            return *parsed;
        }
    }

    return WindowsBackendPreference::Auto;
}

bool probeLibraryExport(const wchar_t* libraryName, const char* exportName) {
    HMODULE module = LoadLibraryW(libraryName);
    if (module == nullptr) {
        return false;
    }

    bool ok = GetProcAddress(module, exportName) != nullptr;
    FreeLibrary(module);
    return ok;
}

bool isMediaFoundationCameraBackendAvailable() {
    static const bool s_available = []() {
        return probeLibraryExport(L"mf.dll", "MFEnumDeviceSources") && probeLibraryExport(L"mfplat.dll", "MFStartup") &&
            probeLibraryExport(L"mfreadwrite.dll", "MFCreateSourceReaderFromMediaSource");
    }();
    return s_available;
}

ProviderImp* createWindowsProvider(std::string_view extraInfo) {
    WindowsBackendPreference preference = resolveWindowsBackendPreference(extraInfo);

    if (preference == WindowsBackendPreference::MSMF && isMediaFoundationCameraBackendAvailable()) {
        return createProviderMSMF();
    }

    return createProviderDirectShow();
}

ProviderImp* createWindowsProvider(WindowsBackendPreference preference) {
    switch (preference) {
    case WindowsBackendPreference::MSMF:
        return isMediaFoundationCameraBackendAvailable() ? createProviderMSMF() : nullptr;
    case WindowsBackendPreference::DirectShow:
    case WindowsBackendPreference::Auto:
        return createProviderDirectShow();
    }

    return createProviderDirectShow();
}

int virtualCameraRank(std::string_view name) {
    std::string normalized = toLowerCopy(name);
    constexpr std::string_view keywords[] = {
        "obs",
        "virtual",
        "fake",
    };

    for (size_t index = 0; index < std::size(keywords); ++index) {
        if (normalized.find(keywords[index]) != std::string::npos) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

void sortDeviceNamesForDisplay(std::vector<std::string>& deviceNames) {
    std::stable_sort(deviceNames.begin(), deviceNames.end(), [](const std::string& lhs, const std::string& rhs) {
        return virtualCameraRank(lhs) < virtualCameraRank(rhs);
    });
}

std::vector<std::string> mergeDeviceNames(std::vector<std::string> preferred, const std::vector<std::string>& fallback) {
    for (const std::string& name : fallback) {
        if (std::find(preferred.begin(), preferred.end(), name) == preferred.end()) {
            preferred.push_back(name);
        }
    }

    sortDeviceNamesForDisplay(preferred);
    return preferred;
}

std::vector<std::string> collectDeviceNamesFromBackend(WindowsBackendPreference preference) {
    std::unique_ptr<ProviderImp> provider(createWindowsProvider(preference));
    return provider ? provider->findDeviceNames() : std::vector<std::string>();
}

bool deviceNameExistsInBackend(std::string_view deviceName, WindowsBackendPreference preference) {
    if (deviceName.empty()) {
        return false;
    }

    std::vector<std::string> deviceNames = collectDeviceNamesFromBackend(preference);
    return std::find(deviceNames.begin(), deviceNames.end(), deviceName) != deviceNames.end();
}

std::vector<WindowsBackendPreference> getAutoBackendCandidates(std::string_view deviceName) {
    std::vector<WindowsBackendPreference> candidates;

    if (deviceName.empty()) {
        candidates.push_back(WindowsBackendPreference::DirectShow);
        if (isMediaFoundationCameraBackendAvailable()) {
            candidates.push_back(WindowsBackendPreference::MSMF);
        }
        return candidates;
    }

    const bool inDirectShow = deviceNameExistsInBackend(deviceName, WindowsBackendPreference::DirectShow);
    const bool inMSMF = deviceNameExistsInBackend(deviceName, WindowsBackendPreference::MSMF);

    if (inDirectShow) {
        candidates.push_back(WindowsBackendPreference::DirectShow);
    }
    if (inMSMF) {
        candidates.push_back(WindowsBackendPreference::MSMF);
    }

    if (!candidates.empty()) {
        return candidates;
    }

    candidates.push_back(WindowsBackendPreference::DirectShow);
    if (isMediaFoundationCameraBackendAvailable()) {
        candidates.push_back(WindowsBackendPreference::MSMF);
    }
    return candidates;
}
#endif
} // namespace

CCAP_EXPORT void setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(g_errorCallbackMutex);
    g_globalErrorCallback = std::move(callback);
}

CCAP_EXPORT ErrorCallback getErrorCallback() {
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

    // 32-byte alignment to meet mainstream SIMD instruction set requirements (AVX)
    size_t alignedSize = (size + 31) & ~size_t(31);
    m_data = static_cast<uint8_t*>(ALIGNED_ALLOC(32, alignedSize));
    if (!m_data) {
        reportError(ErrorCode::MemoryAllocationFailed, "Failed to allocate " + std::to_string(alignedSize) + " bytes of aligned memory");
        m_size = 0;
        return;
    }
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
    return createWindowsProvider(extraInfo);
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    return createProviderV4L2();
#else
    if (warningLogEnabled()) {
        CCAP_LOG_W("ccap: Unsupported platform!\n");
    }
    reportError(ErrorCode::InitializationFailed, "Unsupported platform");
#endif
    return nullptr;
}

Provider::Provider() :
    m_imp(createProvider("")) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::FAILED_TO_CREATE_PROVIDER);
    } else {
        storeProviderState(m_imp, makeProviderCachedState());
        applyCachedState(m_imp);
    }
}

Provider::Provider(Provider&& other) noexcept :
    m_imp(other.m_imp) {
    other.m_imp = nullptr;
}

Provider& Provider::operator=(Provider&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    eraseProviderState(m_imp);
    delete m_imp;

    m_imp = other.m_imp;
    other.m_imp = nullptr;
    return *this;
}

Provider::~Provider() {
    CCAP_LOG_V("ccap: Provider::~Provider() called, this=%p, imp=%p\n", this, m_imp);
    eraseProviderState(m_imp);
    delete m_imp;
}

Provider::Provider(std::string_view deviceName, std::string_view extraInfo) :
    m_imp(createProvider(extraInfo)) {
    if (m_imp) {
        storeProviderState(m_imp, makeProviderCachedState(extraInfo));
        applyCachedState(m_imp);
        open(deviceName);
    } else {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::FAILED_TO_CREATE_PROVIDER);
    }
}

Provider::Provider(int deviceIndex, std::string_view extraInfo) :
    m_imp(createProvider(extraInfo)) {
    if (m_imp) {
        storeProviderState(m_imp, makeProviderCachedState(extraInfo));
        applyCachedState(m_imp);
        open(deviceIndex);
    } else {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::FAILED_TO_CREATE_PROVIDER);
    }
}

void Provider::applyCachedState(ProviderImp* imp) const {
    if (!imp) {
        return;
    }

    ProviderCachedState state = copyProviderState(m_imp);

    imp->setMaxAvailableFrameSize(state.maxAvailableFrameSize);
    imp->setMaxCacheFrameSize(state.maxCacheFrameSize);
    imp->setNewFrameCallback(state.frameCallback);
    imp->setFrameAllocator(state.allocatorFactory);
    imp->set(PropertyName::Width, state.requestedWidth);
    imp->set(PropertyName::Height, state.requestedHeight);
    imp->set(PropertyName::FrameRate, state.requestedFrameRate);
    imp->set(PropertyName::PixelFormatInternal, static_cast<double>(state.requestedInternalFormat));
    imp->set(PropertyName::PixelFormatOutput, static_cast<double>(state.requestedOutputFormat));
    if (state.hasFrameOrientationOverride) {
        imp->set(PropertyName::FrameOrientation, static_cast<double>(state.requestedFrameOrientation));
    }
}

bool Provider::tryOpenWithImplementation(ProviderImp* imp, std::string_view deviceName, bool autoStart) const {
    if (!imp) {
        return false;
    }

    applyCachedState(imp);
    if (!imp->open(deviceName)) {
        imp->close();
        return false;
    }

    if (autoStart && !imp->start()) {
        imp->close();
        return false;
    }

    return true;
}

std::vector<std::string> Provider::findDeviceNames() {
    if (!m_imp) {
        return std::vector<std::string>();
    }

#if defined(_MSC_VER) || defined(_WIN32)
    if (m_imp->isOpened()) {
        return m_imp->findDeviceNames();
    }

    WindowsBackendPreference preference = resolveWindowsBackendPreference(copyProviderState(m_imp).extraInfo);
    if (preference == WindowsBackendPreference::DirectShow) {
        return collectDeviceNamesFromBackend(WindowsBackendPreference::DirectShow);
    }

    if (preference == WindowsBackendPreference::MSMF) {
        return collectDeviceNamesFromBackend(WindowsBackendPreference::MSMF);
    }

    std::vector<std::string> preferred = collectDeviceNamesFromBackend(WindowsBackendPreference::DirectShow);
    std::vector<std::string> fallback = collectDeviceNamesFromBackend(WindowsBackendPreference::MSMF);
    return mergeDeviceNames(std::move(preferred), fallback);
#else
    return m_imp->findDeviceNames();
#endif
}

bool Provider::open(std::string_view deviceName, bool autoStart) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return false;
    }

#if defined(_MSC_VER) || defined(_WIN32)
    if (m_imp->isOpened()) {
        return m_imp->open(deviceName) && (!autoStart || m_imp->start());
    }

    auto tryBackend = [&](WindowsBackendPreference preference) {
        std::unique_ptr<ProviderImp> candidate(createWindowsProvider(preference));
        if (!candidate || !tryOpenWithImplementation(candidate.get(), deviceName, autoStart)) {
            return false;
        }

        transferProviderState(m_imp, candidate.get());
        delete m_imp;
        m_imp = candidate.release();
        return true;
    };

    if (looksLikeFilePath(deviceName)) {
        return tryBackend(WindowsBackendPreference::DirectShow);
    }

    WindowsBackendPreference preference = resolveWindowsBackendPreference(copyProviderState(m_imp).extraInfo);
    if (preference == WindowsBackendPreference::DirectShow) {
        return tryBackend(WindowsBackendPreference::DirectShow);
    }

    if (preference == WindowsBackendPreference::MSMF) {
        if (!isMediaFoundationCameraBackendAvailable()) {
            reportError(ErrorCode::InitializationFailed, "Media Foundation camera backend is unavailable on this system");
            return false;
        }
        return tryBackend(WindowsBackendPreference::MSMF);
    }

    for (WindowsBackendPreference candidate : getAutoBackendCandidates(deviceName)) {
        if (candidate == WindowsBackendPreference::MSMF && !isMediaFoundationCameraBackendAvailable()) {
            continue;
        }
        if (tryBackend(candidate)) {
            return true;
        }
    }

    return false;
#else
    return m_imp->open(deviceName) && (!autoStart || m_imp->start());
#endif
}

bool Provider::open(int deviceIndex, bool autoStart) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return false;
    }

    std::string deviceName;
    if (deviceIndex >= 0) {
        auto deviceNames = findDeviceNames();
        if (!deviceNames.empty()) {
            deviceIndex = std::min(deviceIndex, static_cast<int>(deviceNames.size()) - 1);
            deviceName = deviceNames[deviceIndex];

            CCAP_LOG_V("ccap: input device index %d, selected device name: %s\n", deviceIndex, deviceName.c_str());
        }
    }

    return open(deviceName, autoStart);
}

bool Provider::isOpened() const { return m_imp && m_imp->isOpened(); }

bool Provider::isFileMode() const { return m_imp && m_imp->isFileMode(); }

std::optional<DeviceInfo> Provider::getDeviceInfo() const { return m_imp ? m_imp->getDeviceInfo() : std::nullopt; }

void Provider::close() {
    if (m_imp) {
        m_imp->close();
    }
}

bool Provider::start() {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return false;
    }
    return m_imp->start();
}

void Provider::stop() {
    if (m_imp) m_imp->stop();
}

bool Provider::isStarted() const { return m_imp && m_imp->isStarted(); }

bool Provider::set(PropertyName prop, double value) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return false;
    }

    bool result = m_imp->set(prop, value);
    if (!result) {
        return false;
    }

    updateProviderState(m_imp, [&](ProviderCachedState& state) {
        switch (prop) {
        case PropertyName::Width:
            state.requestedWidth = static_cast<int>(value);
            break;
        case PropertyName::Height:
            state.requestedHeight = static_cast<int>(value);
            break;
        case PropertyName::FrameRate:
            state.requestedFrameRate = value;
            break;
        case PropertyName::PixelFormatInternal: {
            auto intValue = static_cast<int>(value);
#if defined(_MSC_VER) || defined(_WIN32)
            intValue &= ~kPixelFormatFullRangeBit;
#endif
            state.requestedInternalFormat = static_cast<PixelFormat>(intValue);
            break;
        }
        case PropertyName::PixelFormatOutput: {
            uint32_t formatValue = static_cast<uint32_t>(value);
#if defined(_MSC_VER) || defined(_WIN32)
            formatValue &= ~static_cast<uint32_t>(kPixelFormatFullRangeBit);
#endif
            state.requestedOutputFormat = static_cast<PixelFormat>(formatValue);
            break;
        }
        case PropertyName::FrameOrientation:
            state.requestedFrameOrientation = static_cast<FrameOrientation>(static_cast<int>(value));
            state.hasFrameOrientationOverride = true;
            break;
        default:
            break;
        }
    });

    return true;
}

double Provider::get(PropertyName prop) { return m_imp ? m_imp->get(prop) : NAN; }

std::shared_ptr<VideoFrame> Provider::grab(uint32_t timeoutInMs) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return nullptr;
    }
    return m_imp->grab(timeoutInMs);
}

void Provider::setNewFrameCallback(std::function<bool(const std::shared_ptr<VideoFrame>&)> callback) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return;
    }
    updateProviderState(m_imp, [&](ProviderCachedState& state) {
        state.frameCallback = callback;
    });
    m_imp->setNewFrameCallback(std::move(callback));
}

void Provider::setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return;
    }
    updateProviderState(m_imp, [&](ProviderCachedState& state) {
        state.allocatorFactory = allocatorFactory;
    });
    m_imp->setFrameAllocator(std::move(allocatorFactory));
}

void Provider::setMaxAvailableFrameSize(uint32_t size) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return;
    }
    updateProviderState(m_imp, [&](ProviderCachedState& state) {
        state.maxAvailableFrameSize = size;
    });
    m_imp->setMaxAvailableFrameSize(size);
}

void Provider::setMaxCacheFrameSize(uint32_t size) {
    if (!m_imp) {
        reportError(ErrorCode::InitializationFailed, ErrorMessages::PROVIDER_IMPLEMENTATION_NULL);
        return;
    }
    updateProviderState(m_imp, [&](ProviderCachedState& state) {
        state.maxCacheFrameSize = size;
    });
    m_imp->setMaxCacheFrameSize(size);
}

} // namespace ccap