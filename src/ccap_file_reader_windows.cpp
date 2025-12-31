/**
 * @file ccap_file_reader_windows.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Video file reader implementation for Windows using Media Foundation
 * @date 2025-12
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "ccap_file_reader_windows.h"

#include "ccap_convert_frame.h"
#include "ccap_imp_windows.h"

// MinGW compatibility: Ensure SHStrDupW is declared before propvarutil.h needs it
#include <shlwapi.h>
#ifdef __MINGW32__
extern "C" {
HRESULT WINAPI SHStrDupW(LPCWSTR psz, LPWSTR* ppwsz);
}
#endif

#include <algorithm>
#include <chrono>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <thread>

// MinGW compatibility: PropVariantToInt64 may not be available
#ifdef __MINGW32__
#ifndef PropVariantToInt64
inline HRESULT PropVariantToInt64(REFPROPVARIANT propvar, LONGLONG* pllVal) {
    if (!pllVal) return E_POINTER;
    switch (propvar.vt) {
    case VT_I8:
        *pllVal = propvar.hVal.QuadPart;
        return S_OK;
    case VT_UI8:
        *pllVal = static_cast<LONGLONG>(propvar.uhVal.QuadPart);
        return S_OK;
    case VT_I4:
        *pllVal = propvar.lVal;
        return S_OK;
    case VT_UI4:
        *pllVal = propvar.ulVal;
        return S_OK;
    case VT_I2:
        *pllVal = propvar.iVal;
        return S_OK;
    case VT_UI2:
        *pllVal = propvar.uiVal;
        return S_OK;
    case VT_I1:
        *pllVal = propvar.cVal;
        return S_OK;
    case VT_UI1:
        *pllVal = propvar.bVal;
        return S_OK;
    default:
        return DISP_E_TYPEMISMATCH;
    }
}
#endif
#endif

// MSVC-only: Automatically link required Media Foundation libraries
// CMake already handles this via target_link_libraries, but this provides
// a fallback for direct MSVC compilation without CMake
#ifdef _MSC_VER
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "propsys.lib")
#endif // _MSC_VER

namespace {
// Conversion factor: 1 second = 10,000,000 units (100-nanosecond units used by Media Foundation)
constexpr double kMFTimeUnitsPerSecond = 10000000.0;
} // namespace

namespace ccap {

FileReaderWindows::FileReaderWindows(ProviderDirectShow* provider) :
    m_provider(provider) {
}

FileReaderWindows::~FileReaderWindows() {
    close();
}

bool FileReaderWindows::initMediaFoundation() {
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

void FileReaderWindows::uninitMediaFoundation() {
    if (m_mfInitialized) {
        MFShutdown();
        m_mfInitialized = false;
    }
}

bool FileReaderWindows::open(std::string_view filePath) {
    if (m_isOpened) {
        close();
    }

    if (!initMediaFoundation()) {
        return false;
    }

    // Convert to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, filePath.data(), static_cast<int>(filePath.size()), nullptr, 0);
    std::wstring widePath(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, filePath.data(), static_cast<int>(filePath.size()), widePath.data(), wideLen);

    // Check if file exists
    if (!PathFileExistsW(widePath.c_str())) {
        reportError(ErrorCode::FileOpenFailed, "File does not exist");
        uninitMediaFoundation();
        return false;
    }

    if (!createSourceReader(widePath)) {
        uninitMediaFoundation();
        return false;
    }

    if (!configureOutput()) {
        close();
        return false;
    }

    m_isOpened = true;
    m_currentFrameIndex = 0;
    m_currentTime = 0.0;

    CCAP_LOG_I("ccap: Opened video file: %dx%d, %.2f fps, %.2f seconds, %lld frames\n",
               m_width, m_height, m_frameRate, m_duration, (long long)m_totalFrameCount);

    return true;
}

bool FileReaderWindows::createSourceReader(const std::wstring& filePath) {
    IMFAttributes* attributes = nullptr;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) {
        reportError(ErrorCode::FileOpenFailed, "Failed to create MF attributes");
        return false;
    }

    // Enable video processing for format conversion
    hr = attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    if (FAILED(hr)) {
        attributes->Release();
        reportError(ErrorCode::FileOpenFailed, "Failed to enable video processing");
        return false;
    }

    hr = MFCreateSourceReaderFromURL(filePath.c_str(), attributes, &m_sourceReader);
    attributes->Release();

    if (FAILED(hr)) {
        reportError(ErrorCode::FileOpenFailed, "Failed to create source reader from file");
        return false;
    }

    // Get duration
    PROPVARIANT var;
    PropVariantInit(&var);
    hr = m_sourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
    if (SUCCEEDED(hr)) {
        LONGLONG duration100ns = 0;
        // Use PropVariantToInt64 for proper PROPVARIANT handling
        if (SUCCEEDED(PropVariantToInt64(var, &duration100ns))) {
            m_duration = static_cast<double>(duration100ns) / kMFTimeUnitsPerSecond;
        }
    }
    PropVariantClear(&var);

    // Get native media type to determine video properties
    IMFMediaType* nativeType = nullptr;
    hr = m_sourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nativeType);
    if (FAILED(hr)) {
        reportError(ErrorCode::UnsupportedVideoFormat, "No video stream found");
        return false;
    }

    // Get frame size
    UINT32 width = 0, height = 0;
    MFGetAttributeSize(nativeType, MF_MT_FRAME_SIZE, &width, &height);
    m_width = static_cast<int>(width);
    m_height = static_cast<int>(height);

    // Get frame rate
    UINT32 numerator = 0, denominator = 1;
    MFGetAttributeRatio(nativeType, MF_MT_FRAME_RATE, &numerator, &denominator);
    if (numerator > 0 && denominator > 0) {
        m_frameRate = static_cast<double>(numerator) / static_cast<double>(denominator);
    } else {
        m_frameRate = 30.0;
    }

    nativeType->Release();

    // Calculate total frame count
    m_totalFrameCount = static_cast<int64_t>(m_duration * m_frameRate);

    // Update provider frame properties
    if (m_provider) {
        auto& prop = m_provider->getFrameProperty();
        prop.width = m_width;
        prop.height = m_height;
        prop.fps = m_frameRate;
    }

    return true;
}

bool FileReaderWindows::configureOutput() {
    if (!m_sourceReader) {
        return false;
    }

    // Deselect all streams first
    m_sourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);

    // Select only the first video stream
    HRESULT hr = m_sourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    if (FAILED(hr)) {
        reportError(ErrorCode::UnsupportedVideoFormat, "Failed to select video stream");
        return false;
    }

    // Create output media type - request NV12 or RGB32 based on provider settings
    IMFMediaType* outputType = nullptr;
    hr = MFCreateMediaType(&outputType);
    if (FAILED(hr)) {
        return false;
    }

    hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) {
        outputType->Release();
        return false;
    }

    // Prefer NV12 for YUV output, RGB32 for RGB output
    GUID outputFormat = MFVideoFormat_RGB32;
    if (m_provider) {
        auto& prop = m_provider->getFrameProperty();
        if (prop.outputPixelFormat & kPixelFormatYUVColorBit) {
            outputFormat = MFVideoFormat_NV12;
        }
    }

    hr = outputType->SetGUID(MF_MT_SUBTYPE, outputFormat);
    if (FAILED(hr)) {
        outputType->Release();
        return false;
    }

    hr = m_sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outputType);
    outputType->Release();

    if (FAILED(hr)) {
        // Try fallback to RGB32
        hr = MFCreateMediaType(&outputType);
        if (SUCCEEDED(hr)) {
            outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
            hr = m_sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outputType);
            outputType->Release();
        }

        if (FAILED(hr)) {
            reportError(ErrorCode::UnsupportedVideoFormat, "Failed to set output format");
            return false;
        }
    }

    return true;
}

void FileReaderWindows::close() {
    stop();

    if (m_sourceReader) {
        m_sourceReader->Release();
        m_sourceReader = nullptr;
    }

    m_isOpened = false;
    m_currentFrameIndex = 0;
    m_currentTime = 0.0;

    uninitMediaFoundation();
}

bool FileReaderWindows::isOpened() const {
    return m_isOpened;
}

bool FileReaderWindows::start() {
    if (!m_isOpened || m_isStarted) {
        return m_isStarted;
    }

    m_shouldStop = false;
    m_isStarted = true;

    // Start read thread
    std::thread readThread([this]() {
        readLoop();
    });
    readThread.detach();

    return true;
}

void FileReaderWindows::stop() {
    m_shouldStop = true;
    m_isStarted = false;

    // Wait for reading to finish
    int waitCount = 0;
    while (m_isReading && waitCount++ < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool FileReaderWindows::isStarted() const {
    return m_isStarted;
}

void FileReaderWindows::readLoop() {
    m_isReading = true;

    auto lastFrameTime = std::chrono::steady_clock::now();
    double playbackSpeed = m_playbackSpeed.load();
    double targetFrameInterval = (playbackSpeed > 0.0) ? (1.0 / (m_frameRate * playbackSpeed)) : 0.0;

    while (!m_shouldStop && m_sourceReader) {
        // Backpressure: if queue is full, wait for consumer to catch up
        // This prevents dropping frames in file mode
        if (!m_provider->shouldReadMoreFrames()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* sample = nullptr;

        HRESULT hr = m_sourceReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &sample);

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (sample) sample->Release();
            CCAP_LOG_I("ccap: Video playback completed\n");
            break;
        }

        if (!sample) {
            continue;
        }

        // Frame rate control (only when playback speed > 0)
        if (targetFrameInterval > 0.0) {
            auto now = std::chrono::steady_clock::now();
            double elapsedSeconds = std::chrono::duration<double>(now - lastFrameTime).count();
            double sleepTime = targetFrameInterval - elapsedSeconds;

            if (sleepTime > 0.001) {
                std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
            }

            lastFrameTime = std::chrono::steady_clock::now();
        }

        // Process the frame
        IMFMediaBuffer* buffer = nullptr;
        hr = sample->ConvertToContiguousBuffer(&buffer);

        if (SUCCEEDED(hr) && buffer) {
            BYTE* data = nullptr;
            DWORD maxLen = 0, currentLen = 0;

            hr = buffer->Lock(&data, &maxLen, &currentLen);
            if (SUCCEEDED(hr) && data && m_provider) {
                auto newFrame = m_provider->getFreeFrame();

                newFrame->timestamp = static_cast<uint64_t>(timestamp * 100); // 100ns to ns
                newFrame->width = static_cast<uint32_t>(m_width);
                newFrame->height = static_cast<uint32_t>(m_height);
                newFrame->sizeInBytes = currentLen;

                // Determine pixel format from output type
                IMFMediaType* currentType = nullptr;
                GUID subtype = {};
                if (SUCCEEDED(m_sourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &currentType))) {
                    currentType->GetGUID(MF_MT_SUBTYPE, &subtype);
                    currentType->Release();
                }

                // Note: Media Foundation (Source Reader) returns video frames in TopToBottom order
                // for all formats, unlike DirectShow cameras which may return BottomToTop for RGB.
                constexpr FrameOrientation inputOrientation = FrameOrientation::TopToBottom;
                if (subtype == MFVideoFormat_NV12) {
                    newFrame->pixelFormat = PixelFormat::NV12;
                    newFrame->data[0] = data;
                    newFrame->data[1] = data + m_width * m_height;
                    newFrame->data[2] = nullptr;
                    newFrame->stride[0] = m_width;
                    newFrame->stride[1] = m_width;
                    newFrame->stride[2] = 0;
                } else {
                    // RGB32 / BGRA32
                    newFrame->pixelFormat = PixelFormat::BGRA32;
                    newFrame->data[0] = data;
                    newFrame->data[1] = nullptr;
                    newFrame->data[2] = nullptr;
                    newFrame->stride[0] = m_width * 4;
                    newFrame->stride[1] = 0;
                    newFrame->stride[2] = 0;
                }

                // Check if conversion or flip is needed
                auto& prop = m_provider->getFrameProperty();
                bool isOutputYUV = (prop.outputPixelFormat & kPixelFormatYUVColorBit) != 0;
                FrameOrientation targetOrientation = isOutputYUV ? FrameOrientation::TopToBottom : m_provider->frameOrientation();
                bool shouldFlip = !isOutputYUV && (inputOrientation != targetOrientation);
                bool shouldConvert = newFrame->pixelFormat != prop.outputPixelFormat;

                newFrame->orientation = targetOrientation;

                bool zeroCopy = !shouldConvert && !shouldFlip;

                if (!zeroCopy) {
                    if (!newFrame->allocator) {
                        auto&& f = m_provider->getAllocatorFactory();
                        newFrame->allocator = f ? f() : std::make_shared<DefaultAllocator>();
                    }
                    inplaceConvertFrame(newFrame.get(), prop.outputPixelFormat, shouldFlip);
                }

                newFrame->frameIndex = m_currentFrameIndex;

                if (zeroCopy) {
                    // Zero-copy path: buffer must stay locked until frame is released
                    // Use FakeFrame to manage buffer lifetime (similar to camera implementation)
                    buffer->AddRef(); // Ensure buffer lifecycle
                    auto manager = std::make_shared<FakeFrame>([newFrame, buffer]() mutable {
                        newFrame = nullptr;
                        buffer->Unlock();
                        buffer->Release();
                    });
                    newFrame = std::shared_ptr<VideoFrame>(manager, newFrame.get());
                    m_provider->newFrameAvailable(std::move(newFrame));
                    // Don't unlock/release buffer here - FakeFrame will do it when frame is destroyed
                } else {
                    // Conversion path: data was copied, safe to unlock immediately
                    m_provider->newFrameAvailable(std::move(newFrame));
                    buffer->Unlock();
                    buffer->Release();
                }
            } else {
                // Failed to lock buffer, release it
                buffer->Release();
            }
        }

        sample->Release();

        m_currentFrameIndex++;
        m_currentTime = static_cast<double>(timestamp) / kMFTimeUnitsPerSecond;

        // Update target interval in case playback speed changed
        playbackSpeed = m_playbackSpeed.load();
        targetFrameInterval = (playbackSpeed > 0.0) ? (1.0 / (m_frameRate * playbackSpeed)) : 0.0;
    }

    m_isReading = false;
    m_isStarted = false;
    // Notify waiting grab() calls that playback has finished
    if (m_provider) {
        m_provider->notifyGrabWaiters();
    }
}

double FileReaderWindows::getDuration() const {
    return m_duration;
}

double FileReaderWindows::getFrameCount() const {
    return static_cast<double>(m_totalFrameCount);
}

double FileReaderWindows::getCurrentTime() const {
    return m_currentTime;
}

bool FileReaderWindows::seekToTime(double timeInSeconds) {
    if (!m_isOpened || !m_sourceReader) {
        return false;
    }

    // Stop reading to avoid race condition with IMFSourceReader
    bool wasStarted = m_isStarted;
    if (wasStarted) {
        stop();
    }

    timeInSeconds = (std::max)(0.0, (std::min)(timeInSeconds, m_duration));

    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = static_cast<LONGLONG>(timeInSeconds * kMFTimeUnitsPerSecond);

    HRESULT hr = m_sourceReader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);

    if (FAILED(hr)) {
        reportError(ErrorCode::SeekFailed, "Failed to seek to specified time");
        return false;
    }

    m_currentTime = timeInSeconds;
    m_currentFrameIndex = static_cast<int64_t>(timeInSeconds * m_frameRate);

    // Resume playback if it was started before seeking
    if (wasStarted) {
        start();
    }

    return true;
}

double FileReaderWindows::getCurrentFrameIndex() const {
    return static_cast<double>(m_currentFrameIndex.load());
}

bool FileReaderWindows::seekToFrame(int64_t frameIndex) {
    if (!m_isOpened) {
        return false;
    }

    frameIndex = (std::max)(static_cast<int64_t>(0), (std::min)(frameIndex, m_totalFrameCount));
    double timeInSeconds = static_cast<double>(frameIndex) / m_frameRate;

    return seekToTime(timeInSeconds);
}

double FileReaderWindows::getPlaybackSpeed() const {
    return m_playbackSpeed;
}

bool FileReaderWindows::setPlaybackSpeed(double speed) {
    if (speed < 0) {
        return false;
    }
    m_playbackSpeed = speed;
    return true;
}

double FileReaderWindows::getFrameRate() const {
    return m_frameRate;
}

int FileReaderWindows::getWidth() const {
    return m_width;
}

int FileReaderWindows::getHeight() const {
    return m_height;
}

} // namespace ccap

#endif // _WIN32 || _MSC_VER
