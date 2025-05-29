/**
 * @file ccap_imp_msmf.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Implementation for Provider class using Media Foundation.
 * @date 2025-05
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "ccap_imp_msmf.h"

#include "ccap_convert.h"
#include "ccap_convert_frame.h"
#include "ccap_utils.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <vector>

namespace
{
// Helper function to setup COM for Media Foundation
bool setupMF()
{
    static bool s_didSetup = false;
    if (!s_didSetup)
    {
        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        {
            CCAP_LOG_E("ccap: CoInitializeEx failed, hr=0x%08X\n", hr);
            return false;
        }

        // Initialize Media Foundation
        hr = MFStartup(MF_VERSION);
        if (FAILED(hr))
        {
            CCAP_LOG_E("ccap: MFStartup failed, hr=0x%08X\n", hr);
            return false;
        }

        s_didSetup = true;
    }
    return s_didSetup;
}

// Convert GUID to pixel format
ccap::PixelFormat convertGUIDToPixelFormat(const GUID& subtype)
{
    if (subtype == MFVideoFormat_NV12)
        return ccap::PixelFormat::NV12;
    else if (subtype == MFVideoFormat_I420)
        return ccap::PixelFormat::I420;
    else if (subtype == MFVideoFormat_RGB24)
        return ccap::PixelFormat::RGB24;
    else if (subtype == MFVideoFormat_RGB32)
        return ccap::PixelFormat::BGRA32;
    else if (subtype == MFVideoFormat_MJPG)
        return ccap::PixelFormat::BGR24; // MJPG will be decoded to BGR24
    else
        return ccap::PixelFormat::Unknown;
}

// Convert pixel format to GUID
GUID convertPixelFormatToGUID(ccap::PixelFormat format)
{
    switch (format)
    {
    case ccap::PixelFormat::NV12:
    case ccap::PixelFormat::NV12f:
        return MFVideoFormat_NV12;
    case ccap::PixelFormat::I420:
    case ccap::PixelFormat::I420f:
        return MFVideoFormat_I420;
    case ccap::PixelFormat::RGB24:
        return MFVideoFormat_RGB24;
    case ccap::PixelFormat::BGR24:
        return MFVideoFormat_RGB24; // Will convert BGR to RGB
    case ccap::PixelFormat::RGBA32:
    case ccap::PixelFormat::BGRA32:
        return MFVideoFormat_RGB32;
    default:
        return MFVideoFormat_RGB24; // Default fallback
    }
}

} // anonymous namespace

namespace ccap
{

ProviderMSMF::ProviderMSMF() :
    m_refCount(1)
{
    m_frameOrientation = FrameOrientation::TopToBottom; // Media Foundation typically uses top-to-bottom
    CCAP_LOG_V("ccap: ProviderMSMF constructor called\n");
}

ProviderMSMF::~ProviderMSMF()
{
    CCAP_LOG_V("ccap: ProviderMSMF destructor called\n");
    close();
}

bool ProviderMSMF::setup()
{
    if (!m_didSetup)
    {
        m_didSetup = setupMF();
    }
    return m_didSetup;
}

std::string ProviderMSMF::wstringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return {};

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring ProviderMSMF::stringToWstring(const std::string& str)
{
    if (str.empty())
        return {};

    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

void ProviderMSMF::enumerateDevices(std::function<bool(IMFActivate* activate, std::string_view)> callback)
{
    if (!setup())
        return;

    // Clean up previous enumeration
    if (m_devices)
    {
        for (UINT32 i = 0; i < m_deviceCount; i++)
        {
            if (m_devices[i])
                m_devices[i]->Release();
        }
        CoTaskMemFree(m_devices);
        m_devices = nullptr;
        m_deviceCount = 0;
    }

    // Create attributes for video capture devices
    IMFAttributes* attributes = nullptr;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: MFCreateAttributes failed, hr=0x%08X\n", hr);
        return;
    }

    hr = attributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: SetGUID failed, hr=0x%08X\n", hr);
        attributes->Release();
        return;
    }

    // Enumerate devices
    hr = MFEnumDeviceSources(attributes, &m_devices, &m_deviceCount);
    attributes->Release();

    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: MFEnumDeviceSources failed, hr=0x%08X\n", hr);
        return;
    }

    // Process each device
    for (UINT32 i = 0; i < m_deviceCount; i++)
    {
        WCHAR* friendlyName = nullptr;
        UINT32 nameLength = 0;

        hr = m_devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &friendlyName,
            &nameLength);

        if (SUCCEEDED(hr) && friendlyName)
        {
            std::string deviceName = wstringToString(std::wstring(friendlyName, nameLength));
            bool stop = callback && callback(m_devices[i], deviceName);
            CoTaskMemFree(friendlyName);

            if (stop)
                break;
        }
    }
}

std::vector<std::string> ProviderMSMF::findDeviceNames()
{
    if (!m_allDevices.empty())
    {
        std::vector<std::string> names;
        for (const auto& device : m_allDevices)
        {
            names.push_back(device.displayName);
        }
        return names;
    }

    m_allDevices.clear();

    enumerateDevices([&](IMFActivate* activate, std::string_view name) {
        DeviceInfo_Internal deviceInfo;
        deviceInfo.activate = activate;
        deviceInfo.displayName = std::string(name);

        // Get symbolic link if available
        WCHAR* symbolicLink = nullptr;
        UINT32 linkLength = 0;
        HRESULT hr = activate->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            &symbolicLink,
            &linkLength);

        if (SUCCEEDED(hr) && symbolicLink)
        {
            deviceInfo.symbolicLink = wstringToString(std::wstring(symbolicLink, linkLength));
            CoTaskMemFree(symbolicLink);
        }

        m_allDevices.push_back(deviceInfo);
        return false; // Continue enumeration
    });

    // Sort devices - real cameras first, virtual cameras last
    std::sort(m_allDevices.begin(), m_allDevices.end(), [](const DeviceInfo_Internal& a, const DeviceInfo_Internal& b) {
        std::string copyNameA = a.displayName;
        std::string copyNameB = b.displayName;
        std::transform(copyNameA.begin(), copyNameA.end(), copyNameA.begin(), ::tolower);
        std::transform(copyNameB.begin(), copyNameB.end(), copyNameB.begin(), ::tolower);

        // Virtual camera keywords
        std::string_view keywords[] = {
            "obs", "virtual", "fake", "camera2", "snap", "nvidia", "bandicam"
        };

        auto findKeyword = [&keywords](const std::string& name) {
            for (const auto& keyword : keywords)
            {
                if (name.find(keyword) != std::string::npos)
                    return true;
            }
            return false;
        };

        bool aIsVirtual = findKeyword(copyNameA);
        bool bIsVirtual = findKeyword(copyNameB);

        return aIsVirtual < bIsVirtual; // Real cameras first
    });

    std::vector<std::string> names;
    for (const auto& device : m_allDevices)
    {
        names.push_back(device.displayName);
    }

    return names;
}

bool ProviderMSMF::createSourceReader(IMFActivate* activate)
{
    // Create media source from the activate object
    IMFMediaSource* mediaSource = nullptr;
    HRESULT hr = activate->ActivateObject(IID_PPV_ARGS(&mediaSource));
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: ActivateObject failed, hr=0x%08X\n", hr);
        return false;
    }

    // Create source reader with callback
    IMFAttributes* attributes = nullptr;
    hr = MFCreateAttributes(&attributes, 2);
    if (SUCCEEDED(hr))
    {
        // Set callback
        hr = attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
        if (SUCCEEDED(hr))
        {
            // Disable hardware transforms for better compatibility
            hr = attributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = MFCreateSourceReaderFromMediaSource(mediaSource, attributes, &m_sourceReader);
    }

    if (attributes)
        attributes->Release();

    mediaSource->Release();

    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: MFCreateSourceReaderFromMediaSource failed, hr=0x%08X\n", hr);
        return false;
    }

    return true;
}

bool ProviderMSMF::configureSourceReader()
{
    if (!m_sourceReader)
        return false;

    // Get the first available media type
    IMFMediaType* mediaType = nullptr;
    HRESULT hr = m_sourceReader->GetNativeMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &mediaType);

    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: GetNativeMediaType failed, hr=0x%08X\n", hr);
        return false;
    }

    // Get video format information
    GUID subtype = GUID_NULL;
    hr = mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (SUCCEEDED(hr))
    {
        m_frameProp.cameraPixelFormat = convertGUIDToPixelFormat(subtype);
    }

    // Get frame size
    UINT64 frameSize = 0;
    hr = mediaType->GetUINT64(MF_MT_FRAME_SIZE, &frameSize);
    if (SUCCEEDED(hr))
    {
        m_frameProp.width = static_cast<uint32_t>(frameSize >> 32);
        m_frameProp.height = static_cast<uint32_t>(frameSize & 0xFFFFFFFF);
    }

    // Get frame rate
    UINT64 frameRate = 0;
    hr = mediaType->GetUINT64(MF_MT_FRAME_RATE, &frameRate);
    if (SUCCEEDED(hr))
    {
        UINT32 numerator = static_cast<UINT32>(frameRate >> 32);
        UINT32 denominator = static_cast<UINT32>(frameRate & 0xFFFFFFFF);
        if (denominator > 0)
        {
            m_frameProp.fps = static_cast<double>(numerator) / denominator;
        }
    }

    // Set output pixel format if different from camera format
    if (m_frameProp.outputPixelFormat == PixelFormat::Unknown)
    {
        m_frameProp.outputPixelFormat = m_frameProp.cameraPixelFormat;
    }

    // Try to set preferred output media type
    IMFMediaType* outputType = nullptr;
    hr = MFCreateMediaType(&outputType);
    if (SUCCEEDED(hr))
    {
        hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(hr))
        {
            GUID outputSubtype = convertPixelFormatToGUID(m_frameProp.outputPixelFormat);
            hr = outputType->SetGUID(MF_MT_SUBTYPE, outputSubtype);
        }

        if (SUCCEEDED(hr))
        {
            hr = outputType->SetUINT64(MF_MT_FRAME_SIZE, frameSize);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_sourceReader->SetCurrentMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                nullptr,
                outputType);
        }

        outputType->Release();
    }

    mediaType->Release();

    CCAP_LOG_V("ccap: Configured camera: %dx%d, fps: %g, format: %s\n",
               m_frameProp.width, m_frameProp.height, m_frameProp.fps,
               pixelFormatToString(m_frameProp.cameraPixelFormat).data());

    return true;
}

bool ProviderMSMF::open(std::string_view deviceName)
{
    if (m_isOpened)
    {
        CCAP_LOG_E("ccap: Camera already opened, please close it first.\n");
        return false;
    }

    // Find devices if not already done
    if (m_allDevices.empty())
    {
        findDeviceNames();
    }

    // Find the specified device
    IMFActivate* selectedActivate = nullptr;
    for (const auto& device : m_allDevices)
    {
        if (deviceName.empty() || deviceName == device.displayName)
        {
            selectedActivate = device.activate;
            m_deviceName = device.displayName;
            break;
        }
    }

    if (!selectedActivate)
    {
        CCAP_LOG_E("ccap: Device not found: %s\n", deviceName.empty() ? "default" : deviceName.data());
        return false;
    }

    // Create source reader
    if (!createSourceReader(selectedActivate))
    {
        return false;
    }

    // Configure source reader
    if (!configureSourceReader())
    {
        close();
        return false;
    }

    m_isOpened = true;
    m_frameIndex = 0;
    CCAP_LOG_I("ccap: Opened Media Foundation device: %s\n", m_deviceName.c_str());

    return true;
}

bool ProviderMSMF::isOpened() const
{
    return m_isOpened;
}

std::optional<DeviceInfo> ProviderMSMF::getDeviceInfo() const
{
    if (!m_isOpened)
        return std::nullopt;

    DeviceInfo info;
    info.deviceName = m_deviceName;

    // Add current resolution
    info.supportedResolutions.push_back({ (uint32_t)m_frameProp.width, (uint32_t)m_frameProp.height });

    // Add supported pixel formats
    info.supportedPixelFormats.push_back(m_frameProp.cameraPixelFormat);
    info.supportedPixelFormats.push_back(PixelFormat::BGR24);
    info.supportedPixelFormats.push_back(PixelFormat::BGRA32);
    info.supportedPixelFormats.push_back(PixelFormat::RGB24);
    info.supportedPixelFormats.push_back(PixelFormat::RGBA32);
    info.supportedPixelFormats.push_back(PixelFormat::NV12);

    return info;
}

void ProviderMSMF::close()
{
    CCAP_LOG_V("ccap: ProviderMSMF close called\n");

    if (m_isRunning)
    {
        stop();
    }

    if (m_sourceReader)
    {
        m_sourceReader->Release();
        m_sourceReader = nullptr;
    }

    // Clean up device enumeration
    if (m_devices)
    {
        for (UINT32 i = 0; i < m_deviceCount; i++)
        {
            if (m_devices[i])
                m_devices[i]->Release();
        }
        CoTaskMemFree(m_devices);
        m_devices = nullptr;
        m_deviceCount = 0;
    }

    m_allDevices.clear();
    m_isOpened = false;
    m_isRunning = false;
    m_firstFrameArrived = false;

    CCAP_LOG_V("ccap: MSMF Camera closed.\n");
}

bool ProviderMSMF::start()
{
    if (!m_isOpened || !m_sourceReader)
        return false;

    if (m_isRunning)
        return true;

    // Start reading samples asynchronously
    HRESULT hr = m_sourceReader->ReadSample(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: ReadSample failed, hr=0x%08X\n", hr);
        return false;
    }

    m_isRunning = true;
    CCAP_LOG_V("ccap: MSMF source reader started.\n");

    return true;
}

void ProviderMSMF::stop()
{
    CCAP_LOG_V("ccap: ProviderMSMF stop called\n");

    if (m_grabFrameWaiting)
    {
        CCAP_LOG_V("ccap: VideoFrame waiting stopped\n");
        m_grabFrameWaiting = false;
        m_frameCondition.notify_all();
    }

    m_isRunning = false;
    CCAP_LOG_V("ccap: MSMF source reader stopped.\n");
}

bool ProviderMSMF::isStarted() const
{
    return m_isRunning;
}

// IMFSourceReaderCallback implementation
HRESULT STDMETHODCALLTYPE ProviderMSMF::OnReadSample(
    HRESULT hrStatus,
    DWORD dwStreamIndex,
    DWORD dwStreamFlags,
    LONGLONG llTimestamp,
    IMFSample* pSample)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    if (!m_isRunning)
        return S_OK;

    // Continue reading next sample
    if (m_sourceReader)
    {
        m_sourceReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            nullptr,
            nullptr,
            nullptr,
            nullptr);
    }

    if (FAILED(hrStatus))
    {
        CCAP_LOG_E("ccap: OnReadSample failed, hr=0x%08X\n", hrStatus);
        return hrStatus;
    }

    if (!pSample)
        return S_OK;

    auto newFrame = getFreeFrame();
    if (!newFrame)
    {
        CCAP_LOG_W("ccap: VideoFrame pool is full, a new frame skipped...\n");
        return S_OK;
    }

    // Set frame timing
    if (!m_firstFrameArrived)
    {
        m_firstFrameArrived = true;
        m_startTime = std::chrono::steady_clock::now();
    }

    if (llTimestamp > 0)
    {
        newFrame->timestamp = static_cast<uint64_t>(llTimestamp * 100); // Convert to nanoseconds
    }
    else
    {
        newFrame->timestamp = (std::chrono::steady_clock::now() - m_startTime).count();
    }

    // Get the media buffer
    IMFMediaBuffer* buffer = nullptr;
    HRESULT hr = pSample->GetBufferByIndex(0, &buffer);
    if (FAILED(hr))
    {
        CCAP_LOG_E("ccap: GetBufferByIndex failed, hr=0x%08X\n", hr);
        return hr;
    }

    // Lock the buffer
    BYTE* bufferData = nullptr;
    DWORD bufferLength = 0;
    hr = buffer->Lock(&bufferData, nullptr, &bufferLength);

    if (SUCCEEDED(hr) && bufferData)
    {
        // Set frame properties
        newFrame->pixelFormat = m_frameProp.cameraPixelFormat;
        newFrame->width = m_frameProp.width;
        newFrame->height = m_frameProp.height;
        newFrame->orientation = m_frameOrientation;
        newFrame->frameIndex = m_frameIndex++;

        // Handle different pixel formats
        bool isInputYUV = (m_frameProp.cameraPixelFormat & kPixelFormatYUVColorBit);
        bool isOutputYUV = (m_frameProp.outputPixelFormat & kPixelFormatYUVColorBit);
        bool shouldConvert = m_frameProp.cameraPixelFormat != m_frameProp.outputPixelFormat;
        bool zeroCopy = !shouldConvert;

        if (isInputYUV && zeroCopy)
        {
            // Handle YUV formats with zero-copy
            newFrame->data[0] = bufferData;
            newFrame->stride[0] = m_frameProp.width;

            if (pixelFormatInclude(m_frameProp.cameraPixelFormat, PixelFormat::NV12))
            {
                // NV12: Y plane + interleaved UV plane
                newFrame->data[1] = bufferData + m_frameProp.width * m_frameProp.height;
                newFrame->stride[1] = m_frameProp.width;
                newFrame->stride[2] = 0;
                newFrame->data[2] = nullptr;
            }
            else if (pixelFormatInclude(m_frameProp.cameraPixelFormat, PixelFormat::I420))
            {
                // I420: Y plane + U plane + V plane
                newFrame->data[1] = bufferData + m_frameProp.width * m_frameProp.height;
                newFrame->data[2] = bufferData + m_frameProp.width * m_frameProp.height * 5 / 4;
                newFrame->stride[1] = m_frameProp.width / 2;
                newFrame->stride[2] = m_frameProp.width / 2;
            }

            newFrame->sizeInBytes = bufferLength;
        }
        else
        {
            // Handle RGB formats or conversion needed
            if (!newFrame->allocator)
            {
                newFrame->allocator = m_allocatorFactory ? m_allocatorFactory() : std::make_shared<DefaultAllocator>();
            }

            uint32_t outputSize = m_frameProp.width * m_frameProp.height *
                ((m_frameProp.outputPixelFormat & kPixelFormatAlphaColorBit) ? 4 : 3);

            newFrame->allocator->resize(outputSize);
            newFrame->data[0] = newFrame->allocator->data();
            newFrame->stride[0] = m_frameProp.width *
                ((m_frameProp.outputPixelFormat & kPixelFormatAlphaColorBit) ? 4 : 3);
            newFrame->stride[1] = 0;
            newFrame->stride[2] = 0;
            newFrame->data[1] = nullptr;
            newFrame->data[2] = nullptr;
            newFrame->sizeInBytes = outputSize;

            if (shouldConvert)
            {
                // Perform pixel format conversion
                // For now, just copy the data - you would implement actual conversion here
                size_t copySize = std::min((uint32_t)bufferLength, outputSize);
                memcpy(newFrame->data[0], bufferData, copySize);

                // TODO: Implement proper pixel format conversion using ccap_convert functions
                // inplaceConvertFrame(newFrame.get(), m_frameProp.outputPixelFormat, false);
            }
            else
            {
                // Direct copy for RGB formats
                size_t copySize = std::min(static_cast<uint32_t>(bufferLength), outputSize);
                memcpy(newFrame->data[0], bufferData, copySize);
            }
        }

        buffer->Unlock();

        if (ccap::verboseLogEnabled())
        {
            CCAP_LOG_V("ccap: New MSMF frame available: %lux%lu, bytes %lu, Data address: %p\n",
                       newFrame->width, newFrame->height, newFrame->sizeInBytes, newFrame->data[0]);
        }

        newFrameAvailable(std::move(newFrame));
    }
    else
    {
        CCAP_LOG_E("ccap: Buffer lock failed, hr=0x%08X\n", hr);
    }

    buffer->Release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderMSMF::OnFlush(DWORD dwStreamIndex)
{
    CCAP_LOG_V("ccap: OnFlush called for stream %lu\n", dwStreamIndex);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ProviderMSMF::OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent)
{
    CCAP_LOG_V("ccap: OnEvent called for stream %lu\n", dwStreamIndex);
    return S_OK;
}

// IUnknown implementation
HRESULT STDMETHODCALLTYPE ProviderMSMF::QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject)
{
    if (riid == IID_IUnknown)
    {
        *ppvObject = static_cast<IUnknown*>(static_cast<IMFSourceReaderCallback*>(this));
    }
    else if (riid == IID_IMFSourceReaderCallback)
    {
        *ppvObject = static_cast<IMFSourceReaderCallback*>(this);
    }
    else
    {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE ProviderMSMF::AddRef(void)
{
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE ProviderMSMF::Release(void)
{
    ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0)
    {
        // Don't delete here since this object is managed by ccap
    }
    return count;
}

ProviderImp* createProviderMSMF()
{
    return new ProviderMSMF();
}

} // namespace ccap

#endif // defined(_WIN32) || defined(_MSC_VER)
