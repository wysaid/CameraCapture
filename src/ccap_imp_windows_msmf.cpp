/**
 * @file ccap_imp_windows_msmf.cpp
 * @brief Implementation for Provider class using Media Foundation.
 * @date 2026-03
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "ccap_imp_windows_msmf.h"

#include "ccap_convert_frame.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <vector>

namespace {

template <typename T>
void releaseComPtr(T*& ptr) {
    if (ptr != nullptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

std::string wideToUtf8(const wchar_t* text) {
    if (text == nullptr || *text == L'\0') {
        return {};
    }

    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }

    std::string value(static_cast<size_t>(length), '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), length, nullptr, nullptr);
    if (written <= 0) {
        return {};
    }

    value.resize(static_cast<size_t>(written - 1));
    return value;
}

int virtualCameraRank(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (char ch : name) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    constexpr std::string_view keywords[] = {
        "obs",
        "virtual",
        "fake",
    };

    for (size_t index = 0; index < sizeof(keywords) / sizeof(keywords[0]); ++index) {
        if (normalized.find(keywords[index]) != std::string::npos) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

struct PixelFormatInfo {
    GUID subtype;
    const char* name;
    ccap::PixelFormat pixelFormat;
    bool isCompressed;
};

const PixelFormatInfo s_pixelFormatInfos[] = {
    { MFVideoFormat_MJPG, "MJPG", ccap::PixelFormat::Unknown, true },
    { MFVideoFormat_RGB24, "BGR24", ccap::PixelFormat::BGR24, false },
    { MFVideoFormat_RGB32, "BGRA32", ccap::PixelFormat::BGRA32, false },
    { MFVideoFormat_NV12, "NV12", ccap::PixelFormat::NV12, false },
#ifdef MFVideoFormat_I420
    { MFVideoFormat_I420, "I420", ccap::PixelFormat::I420, false },
#endif
    { MFVideoFormat_IYUV, "I420", ccap::PixelFormat::I420, false },
    { MFVideoFormat_YUY2, "YUY2", ccap::PixelFormat::YUYV, false },
    { MFVideoFormat_UYVY, "UYVY", ccap::PixelFormat::UYVY, false },
};

PixelFormatInfo findPixelFormatInfo(const GUID& subtype) {
    for (const auto& info : s_pixelFormatInfos) {
        if (info.subtype == subtype) {
            return info;
        }
    }

    return { GUID_NULL, "Unknown", ccap::PixelFormat::Unknown, false };
}

GUID preferredSubtypeForPixelFormat(ccap::PixelFormat format) {
    switch (static_cast<uint32_t>(format) & ~static_cast<uint32_t>(ccap::kPixelFormatFullRangeBit)) {
    case static_cast<uint32_t>(ccap::PixelFormat::NV12):
        return MFVideoFormat_NV12;
    case static_cast<uint32_t>(ccap::PixelFormat::I420):
#ifdef MFVideoFormat_I420
        return MFVideoFormat_I420;
#else
        return MFVideoFormat_IYUV;
#endif
    case static_cast<uint32_t>(ccap::PixelFormat::YUYV):
        return MFVideoFormat_YUY2;
    case static_cast<uint32_t>(ccap::PixelFormat::UYVY):
        return MFVideoFormat_UYVY;
    case static_cast<uint32_t>(ccap::PixelFormat::BGR24):
    case static_cast<uint32_t>(ccap::PixelFormat::RGB24):
        return MFVideoFormat_RGB24;
    case static_cast<uint32_t>(ccap::PixelFormat::BGRA32):
    case static_cast<uint32_t>(ccap::PixelFormat::RGBA32):
        return MFVideoFormat_RGB32;
    default:
        break;
    }

    return GUID_NULL;
}

void appendUniqueSubtype(std::vector<GUID>& subtypes, const GUID& subtype) {
    if (subtype == GUID_NULL) {
        return;
    }

    auto duplicate = std::find_if(subtypes.begin(), subtypes.end(), [&](const GUID& existing) { return existing == subtype; });
    if (duplicate == subtypes.end()) {
        subtypes.push_back(subtype);
    }
}

} // namespace

namespace ccap {

ProviderMSMF::ProviderMSMF() {
    m_frameOrientation = FrameOrientation::TopToBottom;
}

ProviderMSMF::~ProviderMSMF() {
    close();
    uninitMediaFoundation();
    if (m_didInitializeCom) {
        CoUninitialize();
        m_didInitializeCom = false;
    }
}

bool ProviderMSMF::setup() {
    if (m_mfInitialized) {
        return true;
    }

    if (!m_didSetup) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_didInitializeCom = SUCCEEDED(hr);
        m_didSetup = m_didInitializeCom || hr == RPC_E_CHANGED_MODE;
        if (!m_didSetup) {
            reportError(ErrorCode::InitializationFailed, "COM initialization failed for Media Foundation backend");
            return false;
        }
    }

    return initMediaFoundation();
}

bool ProviderMSMF::initMediaFoundation() {
    if (m_mfInitialized) {
        return true;
    }

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        reportError(ErrorCode::InitializationFailed, "Failed to initialize Media Foundation");
        return false;
    }

    m_mfInitialized = true;
    return true;
}

void ProviderMSMF::uninitMediaFoundation() {
    if (m_mfInitialized) {
        MFShutdown();
        m_mfInitialized = false;
    }
}

bool ProviderMSMF::enumerateDevices(std::vector<DeviceEntry>& devices) {
    devices.clear();

    if (!setup()) {
        return false;
    }

    IMFAttributes* attributes = nullptr;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) {
        reportError(ErrorCode::NoDeviceFound, "Failed to create Media Foundation attributes for device enumeration");
        return false;
    }

    hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        releaseComPtr(attributes);
        reportError(ErrorCode::NoDeviceFound, "Failed to configure Media Foundation video device enumeration");
        return false;
    }

    IMFActivate** deviceArray = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(attributes, &deviceArray, &count);
    releaseComPtr(attributes);

    if (FAILED(hr)) {
        reportError(ErrorCode::NoDeviceFound, "Media Foundation device enumeration failed");
        return false;
    }

    devices.reserve(count);
    for (UINT32 index = 0; index < count; ++index) {
        wchar_t* friendlyName = nullptr;
        UINT32 friendlyLength = 0;
        wchar_t* symbolicLink = nullptr;
        UINT32 symbolicLength = 0;

        hr = deviceArray[index]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendlyName, &friendlyLength);
        if (SUCCEEDED(hr) && friendlyName != nullptr) {
            DeviceEntry entry;
            entry.friendlyName = wideToUtf8(friendlyName);

            if (SUCCEEDED(deviceArray[index]->GetAllocatedString(
                    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symbolicLink, &symbolicLength)) &&
                symbolicLink != nullptr) {
                entry.symbolicLink.assign(symbolicLink, symbolicLength);
            }

            devices.emplace_back(std::move(entry));
        }

        if (friendlyName != nullptr) {
            CoTaskMemFree(friendlyName);
        }
        if (symbolicLink != nullptr) {
            CoTaskMemFree(symbolicLink);
        }

        releaseComPtr(deviceArray[index]);
    }

    CoTaskMemFree(deviceArray);

    std::stable_sort(devices.begin(), devices.end(), [](const DeviceEntry& lhs, const DeviceEntry& rhs) {
        return virtualCameraRank(lhs.friendlyName) < virtualCameraRank(rhs.friendlyName);
    });

    return true;
}

bool ProviderMSMF::ensureDeviceCache() {
    if (!m_devices.empty()) {
        return true;
    }
    return enumerateDevices(m_devices);
}

std::vector<std::string> ProviderMSMF::findDeviceNames() {
    if (!ensureDeviceCache()) {
        return {};
    }

    std::vector<std::string> deviceNames;
    deviceNames.reserve(m_devices.size());
    for (const DeviceEntry& entry : m_devices) {
        deviceNames.push_back(entry.friendlyName);
    }
    return deviceNames;
}

bool ProviderMSMF::createMediaSource(const std::wstring& symbolicLink) {
    IMFAttributes* attributes = nullptr;
    HRESULT hr = MFCreateAttributes(&attributes, 2);
    if (FAILED(hr)) {
        reportError(ErrorCode::DeviceOpenFailed, "Failed to create Media Foundation attributes for device open");
        return false;
    }

    hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr)) {
        hr = attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, symbolicLink.c_str());
    }

    if (SUCCEEDED(hr)) {
        hr = MFCreateDeviceSource(attributes, &m_mediaSource);
    }

    releaseComPtr(attributes);

    if (FAILED(hr) || m_mediaSource == nullptr) {
        reportError(ErrorCode::DeviceOpenFailed, "Failed to create Media Foundation device source");
        return false;
    }

    return true;
}

bool ProviderMSMF::createSourceReader() {
    IMFAttributes* attributes = nullptr;
    HRESULT hr = MFCreateAttributes(&attributes, 2);
    if (FAILED(hr)) {
        reportError(ErrorCode::DeviceOpenFailed, "Failed to create Media Foundation source reader attributes");
        return false;
    }

    hr = attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    if (SUCCEEDED(hr)) {
#ifdef MF_LOW_LATENCY
        attributes->SetUINT32(MF_LOW_LATENCY, TRUE);
#endif
        hr = MFCreateSourceReaderFromMediaSource(m_mediaSource, attributes, &m_sourceReader);
    }

    releaseComPtr(attributes);

    if (FAILED(hr) || m_sourceReader == nullptr) {
        reportError(ErrorCode::DeviceOpenFailed, "Failed to create Media Foundation source reader");
        return false;
    }

    m_sourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    hr = m_sourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    if (FAILED(hr)) {
        reportError(ErrorCode::DeviceOpenFailed, "Failed to select Media Foundation video stream");
        return false;
    }

    return true;
}

std::vector<ProviderMSMF::MediaTypeInfo> ProviderMSMF::enumerateMediaTypes() const {
    std::vector<MediaTypeInfo> mediaTypes;
    if (m_sourceReader == nullptr) {
        return mediaTypes;
    }

    for (DWORD index = 0;; ++index) {
        IMFMediaType* mediaType = nullptr;
        HRESULT hr = m_sourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, &mediaType);
        if (hr == MF_E_NO_MORE_TYPES) {
            break;
        }
        if (FAILED(hr) || mediaType == nullptr) {
            continue;
        }

        GUID subtype = GUID_NULL;
        mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        PixelFormatInfo pixelFormatInfo = findPixelFormatInfo(subtype);

        UINT32 width = 0;
        UINT32 height = 0;
        MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);

        UINT32 fpsNumerator = 0;
        UINT32 fpsDenominator = 0;
        MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);

        MediaTypeInfo info;
        info.mediaType = mediaType;
        info.subtype = subtype;
        info.pixelFormat = pixelFormatInfo.pixelFormat;
        info.width = width;
        info.height = height;
        info.frameRateNumerator = fpsNumerator;
        info.frameRateDenominator = fpsDenominator == 0 ? 1 : fpsDenominator;
        info.fps = fpsNumerator != 0 ? static_cast<double>(fpsNumerator) / static_cast<double>(info.frameRateDenominator) : 0.0;
        info.isCompressed = pixelFormatInfo.isCompressed;
        mediaTypes.push_back(info);
    }

    return mediaTypes;
}

void ProviderMSMF::releaseMediaTypes(std::vector<MediaTypeInfo>& mediaTypes) const {
    for (MediaTypeInfo& info : mediaTypes) {
        releaseComPtr(info.mediaType);
    }
}

bool ProviderMSMF::trySetCurrentMediaType(const MediaTypeInfo& selected, const GUID& subtype) {
    IMFMediaType* outputType = nullptr;
    HRESULT hr = MFCreateMediaType(&outputType);
    if (FAILED(hr)) {
        return false;
    }

    hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) {
        hr = outputType->SetGUID(MF_MT_SUBTYPE, subtype);
    }
    if (SUCCEEDED(hr)) {
        hr = MFSetAttributeSize(outputType, MF_MT_FRAME_SIZE, selected.width, selected.height);
    }
    if (SUCCEEDED(hr) && selected.frameRateNumerator != 0) {
        hr = MFSetAttributeRatio(outputType, MF_MT_FRAME_RATE, selected.frameRateNumerator, selected.frameRateDenominator);
    }
    if (SUCCEEDED(hr)) {
        hr = m_sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outputType);
    }

    releaseComPtr(outputType);
    return SUCCEEDED(hr);
}

bool ProviderMSMF::updateCurrentMediaType() {
    IMFMediaType* currentType = nullptr;
    HRESULT hr = m_sourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &currentType);
    if (FAILED(hr) || currentType == nullptr) {
        reportError(ErrorCode::UnsupportedPixelFormat, "Failed to query Media Foundation output media type");
        return false;
    }

    GUID subtype = GUID_NULL;
    currentType->GetGUID(MF_MT_SUBTYPE, &subtype);
    PixelFormatInfo info = findPixelFormatInfo(subtype);

    UINT32 width = 0;
    UINT32 height = 0;
    MFGetAttributeSize(currentType, MF_MT_FRAME_SIZE, &width, &height);

    UINT32 fpsNumerator = 0;
    UINT32 fpsDenominator = 0;
    MFGetAttributeRatio(currentType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);

    LONG stride = 0;
    UINT32 strideValue = 0;
    if (FAILED(currentType->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideValue))) {
        switch (info.pixelFormat) {
        case PixelFormat::BGRA32:
            stride = static_cast<LONG>(width * 4);
            break;
        case PixelFormat::BGR24:
            stride = static_cast<LONG>(width * 3);
            break;
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            stride = static_cast<LONG>(width * 2);
            break;
        default:
            stride = static_cast<LONG>(width);
            break;
        }
    } else {
        stride = static_cast<LONG>(strideValue);
    }

    releaseComPtr(currentType);

    if (info.pixelFormat == PixelFormat::Unknown) {
        reportError(ErrorCode::UnsupportedPixelFormat, "Media Foundation selected an unsupported output pixel format");
        return false;
    }

    m_activePixelFormat = info.pixelFormat;
    m_activeWidth = width;
    m_activeHeight = height;
    m_activeFps = fpsNumerator != 0 ? static_cast<double>(fpsNumerator) / static_cast<double>(fpsDenominator == 0 ? 1 : fpsDenominator) : 0.0;
    m_activeStride0 = stride;

    m_frameProp.cameraPixelFormat = m_activePixelFormat;
    m_frameProp.width = static_cast<int>(m_activeWidth);
    m_frameProp.height = static_cast<int>(m_activeHeight);
    m_frameProp.fps = m_activeFps;
    return true;
}

bool ProviderMSMF::configureMediaType() {
    std::vector<MediaTypeInfo> mediaTypes = enumerateMediaTypes();
    if (mediaTypes.empty()) {
        reportError(ErrorCode::UnsupportedPixelFormat, "No Media Foundation video formats were reported by the device");
        return false;
    }

    const int desiredWidth = m_frameProp.width;
    const int desiredHeight = m_frameProp.height;
    std::vector<size_t> matchedIndexes;
    matchedIndexes.reserve(mediaTypes.size());

    for (size_t index = 0; index < mediaTypes.size(); ++index) {
        const MediaTypeInfo& info = mediaTypes[index];
        if (desiredWidth <= static_cast<int>(info.width) && desiredHeight <= static_cast<int>(info.height)) {
            matchedIndexes.push_back(index);
        }
    }

    if (matchedIndexes.empty()) {
        matchedIndexes.resize(mediaTypes.size());
        for (size_t index = 0; index < mediaTypes.size(); ++index) {
            matchedIndexes[index] = index;
        }
    }

    double closestDistance = 1.e9;
    std::vector<size_t> bestIndexes;
    for (size_t index : matchedIndexes) {
        const MediaTypeInfo& info = mediaTypes[index];
        double distance = std::abs(static_cast<double>(info.width) - desiredWidth) + std::abs(static_cast<double>(info.height) - desiredHeight);
        if (distance < closestDistance) {
            closestDistance = distance;
            bestIndexes = { index };
        } else if (std::abs(distance - closestDistance) < 1e-5) {
            bestIndexes.push_back(index);
        }
    }

    PixelFormat preferredPixelFormat = m_frameProp.cameraPixelFormat != PixelFormat::Unknown ? m_frameProp.cameraPixelFormat :
                                                                                        m_frameProp.outputPixelFormat;

    const MediaTypeInfo* selected = nullptr;
    for (size_t index : bestIndexes) {
        const MediaTypeInfo& info = mediaTypes[index];
        if (info.pixelFormat == preferredPixelFormat || (!(preferredPixelFormat & kPixelFormatYUVColorBit) && info.isCompressed)) {
            selected = &info;
            break;
        }
    }

    if (selected == nullptr && !bestIndexes.empty()) {
        selected = &mediaTypes[bestIndexes.front()];
    }

    if (selected == nullptr) {
        releaseMediaTypes(mediaTypes);
        reportError(ErrorCode::UnsupportedPixelFormat, "Failed to choose a Media Foundation camera format");
        return false;
    }

    std::vector<GUID> subtypesToTry;
    appendUniqueSubtype(subtypesToTry, preferredSubtypeForPixelFormat(preferredPixelFormat));
    appendUniqueSubtype(subtypesToTry, preferredSubtypeForPixelFormat(selected->pixelFormat));
    if (preferredPixelFormat & kPixelFormatYUVColorBit) {
        appendUniqueSubtype(subtypesToTry, MFVideoFormat_NV12);
    } else {
        appendUniqueSubtype(subtypesToTry, MFVideoFormat_RGB32);
        appendUniqueSubtype(subtypesToTry, MFVideoFormat_RGB24);
    }

    bool configured = false;
    for (const GUID& subtype : subtypesToTry) {
        if (trySetCurrentMediaType(*selected, subtype)) {
            configured = true;
            break;
        }
    }

    releaseMediaTypes(mediaTypes);

    if (!configured || !updateCurrentMediaType()) {
        reportError(ErrorCode::UnsupportedPixelFormat, "Failed to configure Media Foundation output media type");
        return false;
    }

    return true;
}

bool ProviderMSMF::open(std::string_view deviceName) {
    if (looksLikeFilePath(deviceName)) {
        reportError(ErrorCode::DeviceOpenFailed, "The Media Foundation camera backend does not handle file playback");
        return false;
    }

    if (m_isOpened || m_sourceReader != nullptr || m_mediaSource != nullptr) {
        reportError(ErrorCode::DeviceOpenFailed, "Camera already opened, please close it first");
        return false;
    }

    if (!ensureDeviceCache()) {
        return false;
    }

    const DeviceEntry* selectedDevice = nullptr;
    for (const DeviceEntry& entry : m_devices) {
        if (deviceName.empty() || entry.friendlyName == deviceName) {
            selectedDevice = &entry;
            break;
        }
    }

    if (selectedDevice == nullptr) {
        reportError(ErrorCode::InvalidDevice, "No Media Foundation video capture device: " + std::string(deviceName));
        return false;
    }

    m_deviceName = selectedDevice->friendlyName;
    m_deviceSymbolicLink = selectedDevice->symbolicLink;
    m_isFileMode = false;
    m_frameIndex = 0;

    if (!createMediaSource(m_deviceSymbolicLink) || !createSourceReader() || !configureMediaType()) {
        close();
        return false;
    }

    m_isOpened = true;
    return true;
}

bool ProviderMSMF::isOpened() const {
    return m_isOpened;
}

std::optional<DeviceInfo> ProviderMSMF::getDeviceInfo() const {
    if (!m_isOpened || m_sourceReader == nullptr) {
        return std::nullopt;
    }

    std::vector<MediaTypeInfo> mediaTypes = enumerateMediaTypes();
    if (mediaTypes.empty()) {
        return std::nullopt;
    }

    std::optional<DeviceInfo> info;
    info.emplace();
    info->deviceName = m_deviceName;

    bool hasMJPG = false;
    for (const MediaTypeInfo& mediaType : mediaTypes) {
        if (mediaType.pixelFormat != PixelFormat::Unknown) {
            info->supportedPixelFormats.push_back(mediaType.pixelFormat);
        } else if (mediaType.isCompressed) {
            hasMJPG = true;
        }

        if (mediaType.width != 0 && mediaType.height != 0) {
            info->supportedResolutions.push_back({ mediaType.width, mediaType.height });
        }
    }

    releaseMediaTypes(mediaTypes);

    if (hasMJPG) {
        info->supportedPixelFormats.push_back(PixelFormat::BGR24);
        info->supportedPixelFormats.push_back(PixelFormat::BGRA32);
        info->supportedPixelFormats.push_back(PixelFormat::RGB24);
        info->supportedPixelFormats.push_back(PixelFormat::RGBA32);
    }

    auto& resolutions = info->supportedResolutions;
    std::sort(resolutions.begin(), resolutions.end(), [](const DeviceInfo::Resolution& lhs, const DeviceInfo::Resolution& rhs) {
        return lhs.width * lhs.height < rhs.width * rhs.height;
    });
    resolutions.erase(std::unique(resolutions.begin(), resolutions.end(), [](const DeviceInfo::Resolution& lhs, const DeviceInfo::Resolution& rhs) {
                          return lhs.width == rhs.width && lhs.height == rhs.height;
                      }),
                      resolutions.end());

    auto& formats = info->supportedPixelFormats;
    std::sort(formats.begin(), formats.end());
    formats.erase(std::unique(formats.begin(), formats.end()), formats.end());
    return info;
}

void ProviderMSMF::readLoop() {
    HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool didInitCom = SUCCEEDED(comResult);

    while (!m_shouldStop && m_sourceReader != nullptr) {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* sample = nullptr;

        HRESULT hr = m_sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &sample);
        if (FAILED(hr)) {
            if (!m_shouldStop) {
                reportError(ErrorCode::FrameCaptureFailed, "Media Foundation frame read failed");
            }
            releaseComPtr(sample);
            break;
        }

        if (flags & (MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED | MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
            if (!updateCurrentMediaType()) {
                releaseComPtr(sample);
                break;
            }
        }

        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) || sample == nullptr) {
            releaseComPtr(sample);
            if (m_shouldStop) {
                break;
            }
            continue;
        }

        IMFMediaBuffer* buffer = nullptr;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr) || buffer == nullptr) {
            releaseComPtr(sample);
            continue;
        }

        BYTE* data = nullptr;
        DWORD maxLength = 0;
        DWORD currentLength = 0;
        hr = buffer->Lock(&data, &maxLength, &currentLength);
        if (FAILED(hr) || data == nullptr) {
            releaseComPtr(buffer);
            releaseComPtr(sample);
            continue;
        }

        auto newFrame = getFreeFrame();
        if (!newFrame) {
            buffer->Unlock();
            releaseComPtr(buffer);
            releaseComPtr(sample);
            continue;
        }

        newFrame->timestamp = static_cast<uint64_t>(timestamp * 100);
        newFrame->pixelFormat = m_activePixelFormat;
        newFrame->width = m_activeWidth;
        newFrame->height = m_activeHeight;
        newFrame->nativeHandle = nullptr;

        bool isOutputYUV = (m_frameProp.outputPixelFormat & kPixelFormatYUVColorBit) != 0;
        FrameOrientation targetOrientation = isOutputYUV ? FrameOrientation::TopToBottom : m_frameOrientation;
        newFrame->orientation = targetOrientation;

        switch (m_activePixelFormat) {
        case PixelFormat::NV12:
            newFrame->data[0] = data;
            newFrame->data[1] = data + static_cast<size_t>(m_activeWidth) * static_cast<size_t>(m_activeHeight);
            newFrame->data[2] = nullptr;
            newFrame->stride[0] = m_activeWidth;
            newFrame->stride[1] = m_activeWidth;
            newFrame->stride[2] = 0;
            break;
        case PixelFormat::I420:
            newFrame->data[0] = data;
            newFrame->data[1] = data + static_cast<size_t>(m_activeWidth) * static_cast<size_t>(m_activeHeight);
            newFrame->data[2] = newFrame->data[1] + static_cast<size_t>(m_activeWidth / 2) * static_cast<size_t>(m_activeHeight / 2);
            newFrame->stride[0] = m_activeWidth;
            newFrame->stride[1] = m_activeWidth / 2;
            newFrame->stride[2] = m_activeWidth / 2;
            break;
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            newFrame->data[0] = data;
            newFrame->data[1] = nullptr;
            newFrame->data[2] = nullptr;
            newFrame->stride[0] = m_activeWidth * 2;
            newFrame->stride[1] = 0;
            newFrame->stride[2] = 0;
            break;
        case PixelFormat::BGR24:
            newFrame->data[0] = data;
            newFrame->data[1] = nullptr;
            newFrame->data[2] = nullptr;
            newFrame->stride[0] = static_cast<uint32_t>(m_activeStride0 > 0 ? m_activeStride0 : static_cast<int32_t>(m_activeWidth * 3));
            newFrame->stride[1] = 0;
            newFrame->stride[2] = 0;
            break;
        case PixelFormat::BGRA32:
            newFrame->data[0] = data;
            newFrame->data[1] = nullptr;
            newFrame->data[2] = nullptr;
            newFrame->stride[0] = static_cast<uint32_t>(m_activeStride0 > 0 ? m_activeStride0 : static_cast<int32_t>(m_activeWidth * 4));
            newFrame->stride[1] = 0;
            newFrame->stride[2] = 0;
            break;
        default:
            buffer->Unlock();
            releaseComPtr(buffer);
            releaseComPtr(sample);
            continue;
        }

        bool shouldFlip = !isOutputYUV && targetOrientation != FrameOrientation::TopToBottom;
        bool shouldConvert = newFrame->pixelFormat != m_frameProp.outputPixelFormat;
        bool zeroCopy = !shouldConvert && !shouldFlip;

        if (!zeroCopy) {
            if (!newFrame->allocator) {
                newFrame->allocator = m_allocatorFactory ? m_allocatorFactory() : std::make_shared<DefaultAllocator>();
            }

            zeroCopy = !inplaceConvertFrame(newFrame.get(), m_frameProp.outputPixelFormat, shouldFlip);
            newFrame->sizeInBytes = newFrame->stride[0] * newFrame->height +
                (newFrame->stride[1] + newFrame->stride[2]) * newFrame->height / 2;
        }

        if (zeroCopy) {
            newFrame->nativeHandle = sample;
            newFrame->sizeInBytes = currentLength;
            sample->AddRef();
            buffer->AddRef();
            auto manager = std::make_shared<FakeFrame>([newFrame, buffer, sample]() mutable {
                newFrame = nullptr;
                buffer->Unlock();
                buffer->Release();
                sample->Release();
            });
            newFrame = std::shared_ptr<VideoFrame>(manager, newFrame.get());
        } else {
            buffer->Unlock();
            releaseComPtr(buffer);
        }

        newFrame->frameIndex = m_frameIndex++;
        newFrameAvailable(std::move(newFrame));

        releaseComPtr(buffer);
        releaseComPtr(sample);
    }

    m_isRunning = false;
    notifyGrabWaiters();

    if (didInitCom) {
        CoUninitialize();
    }
}

bool ProviderMSMF::start() {
    if (!m_isOpened) {
        return false;
    }

    if (m_isRunning) {
        return true;
    }

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    m_shouldStop = false;
    m_isRunning = true;
    m_readThread = std::thread([this]() {
        readLoop();
    });
    return true;
}

void ProviderMSMF::stop() {
    if (m_grabFrameWaiting) {
        m_grabFrameWaiting = false;
        notifyGrabWaiters();
    }

    if (!m_isRunning) {
        return;
    }

    m_shouldStop = true;
    if (m_sourceReader != nullptr) {
        m_sourceReader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    }

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    m_isRunning = false;
}

bool ProviderMSMF::isStarted() const {
    return m_isRunning && m_isOpened;
}

void ProviderMSMF::close() {
    stop();

    releaseComPtr(m_sourceReader);
    if (m_mediaSource != nullptr) {
        m_mediaSource->Shutdown();
    }
    releaseComPtr(m_mediaSource);

    {
        std::lock_guard<std::mutex> lock(m_availableFrameMutex);
        m_availableFrames = {};
    }

    m_isOpened = false;
    m_shouldStop = false;
    m_isFileMode = false;
    m_activePixelFormat = PixelFormat::Unknown;
    m_activeWidth = 0;
    m_activeHeight = 0;
    m_activeFps = 0.0;
    m_activeStride0 = 0;
}

ProviderImp* createProviderMSMF() {
    return new ProviderMSMF();
}

} // namespace ccap

#endif