/**
 * @file ccap_writer_windows.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Video writer implementation for Windows using Media Foundation Sink Writer.
 * @date 2025-05
 */

#include "ccap_utils.h"
#include "ccap_writer_imp.h"

#if defined(_WIN32) || defined(_MSC_VER)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <atomic>
#include <iomanip>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mutex>
#include <sstream>
#include <vector>
#include <windows.h>

#ifdef _MSC_VER
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

namespace ccap {

namespace {

std::string formatHRESULT(HRESULT hr) {
    std::ostringstream stream;
    stream << "0x"
           << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
           << static_cast<unsigned int>(hr);
    return stream.str();
}

} // namespace

class WriterWindows : public VideoWriter::Impl {
public:
    WriterWindows() :
        m_sinkWriter(nullptr), m_streamIndex(0), m_mfInitialized(false) {
        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
        m_mfInitialized = SUCCEEDED(hr);
        if (!m_mfInitialized) {
            CCAP_LOG_E("MFStartup failed: 0x%08lX\n", hr);
        }
    }

    ~WriterWindows() override {
        close();
        if (m_mfInitialized) {
            MFShutdown();
        }
    }

    bool open(std::string_view filePath, const WriterConfig& config) override {
        if (!m_mfInitialized) {
            reportError(ErrorCode::WriterOpenFailed, "Media Foundation not initialized");
            return false;
        }
        if (config.width == 0 || config.height == 0) {
            reportError(ErrorCode::WriterOpenFailed, "Invalid dimensions: " + std::to_string(config.width) + "x" + std::to_string(config.height));
            return false;
        }
        if (config.width % 2 != 0 || config.height % 2 != 0) {
            reportError(ErrorCode::WriterOpenFailed, "Video dimensions must be even for NV12 encoding: " + std::to_string(config.width) + "x" + std::to_string(config.height));
            return false;
        }
        m_config = config;

        // Convert path to wide string
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, filePath.data(), static_cast<int>(filePath.size()), nullptr, 0);
        std::wstring widePath(wideLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, filePath.data(), static_cast<int>(filePath.size()), widePath.data(), wideLen);

        // Try requested codec first, then fallback
        GUID codecs[2];
        VideoCodec cppCodecs[2];
        if (config.codec == VideoCodec::H264) {
            codecs[0] = MFVideoFormat_H264;
            cppCodecs[0] = VideoCodec::H264;
            codecs[1] = MFVideoFormat_HEVC;
            cppCodecs[1] = VideoCodec::HEVC;
        } else {
            codecs[0] = MFVideoFormat_HEVC;
            cppCodecs[0] = VideoCodec::HEVC;
            codecs[1] = MFVideoFormat_H264;
            cppCodecs[1] = VideoCodec::H264;
        }

        for (int i = 0; i < 2; i++) {
            if (tryCreateWriter(widePath, codecs[i], config)) {
                m_actualCodec = cppCodecs[i];
                m_frameCount = 0;
                m_isOpened = true;
                return true;
            }
        }

        reportError(ErrorCode::WriterOpenFailed, "Failed to create video writer with any supported codec");
        return false;
    }

    void close() override {
        if (!m_isOpened) return;
        m_isOpened = false;

        if (m_sinkWriter) {
            HRESULT hr = m_sinkWriter->Finalize();
            if (FAILED(hr)) {
                reportError(ErrorCode::WriterCloseFailed, "IMFSinkWriter::Finalize failed: " + formatHRESULT(hr));
            }
            m_sinkWriter->Release();
            m_sinkWriter = nullptr;
        }

        m_streamIndex = 0;
        m_frameCount = 0;
        std::memset(&m_config, 0, sizeof(m_config));
    }

    bool isOpened() const override {
        return m_isOpened;
    }

    bool writeFrame(const VideoFrame& frame, uint64_t timestampNs) override {
        if (!m_isOpened || !m_sinkWriter) return false;

        if (frame.width != m_config.width || frame.height != m_config.height) {
            reportError(ErrorCode::WriterWriteFailed, "Frame dimensions " + std::to_string(frame.width) + "x" + std::to_string(frame.height) + " do not match configured " + std::to_string(m_config.width) + "x" + std::to_string(m_config.height));
            return false;
        }

        const int w = static_cast<int>(frame.width);
        const int h = static_cast<int>(frame.height);
        const int w2 = w / 2;
        const int h2 = h / 2;

        // Convert frame to NV12
        std::vector<uint8_t> yBuf, uvBuf;
        uint32_t yStride, uvStride;
        if (!convertFrameToNv12(frame, yBuf, uvBuf, yStride, uvStride)) {
            reportError(ErrorCode::WriterWriteFailed, "Unsupported pixel format: " + std::to_string(static_cast<int>(frame.pixelFormat)));
            return false;
        }

        // Total NV12 buffer size
        DWORD totalSize = static_cast<DWORD>(yStride) * h + static_cast<DWORD>(uvStride) * h2;

        // Create sample
        IMFSample* pSample = nullptr;
        HRESULT hr = MFCreateSample(&pSample);
        if (FAILED(hr)) {
            reportError(ErrorCode::WriterWriteFailed, "MFCreateSample failed");
            return false;
        }

        IMFMediaBuffer* pBuffer = nullptr;
        hr = MFCreateMemoryBuffer(totalSize, &pBuffer);
        if (FAILED(hr)) {
            reportError(ErrorCode::WriterWriteFailed, "MFCreateMemoryBuffer failed");
            pSample->Release();
            return false;
        }

        BYTE* pData = nullptr;
        hr = pBuffer->Lock(&pData, nullptr, nullptr);
        if (FAILED(hr)) {
            pBuffer->Release();
            pSample->Release();
            return false;
        }

        // Copy Y plane
        for (int y = 0; y < h; y++) {
            memcpy(pData + y * yStride, yBuf.data() + y * yStride, static_cast<size_t>(w));
        }
        // Copy UV plane
        uint8_t* uvStart = pData + static_cast<DWORD>(yStride) * h;
        for (int y = 0; y < h2; y++) {
            memcpy(uvStart + y * uvStride, uvBuf.data() + y * uvStride, static_cast<size_t>(w2) * 2);
        }

        pBuffer->Unlock();
        pBuffer->SetCurrentLength(totalSize);
        pSample->AddBuffer(pBuffer);
        pBuffer->Release();

        // Set timestamp (100ns units)
        LONGLONG hnsTimestamp;
        if (timestampNs > 0) {
            hnsTimestamp = static_cast<LONGLONG>(timestampNs / 100);
        } else {
            double fps = m_config.frameRate > 0 ? m_config.frameRate : 30.0;
            hnsTimestamp = static_cast<LONGLONG>(m_frameCount * 10000000.0 / fps);
        }
        pSample->SetSampleTime(hnsTimestamp);

        // Set sample duration
        {
            double fps = m_config.frameRate > 0 ? m_config.frameRate : 30.0;
            LONGLONG duration = static_cast<LONGLONG>(10000000.0 / fps);
            pSample->SetSampleDuration(duration);
        }

        hr = m_sinkWriter->WriteSample(m_streamIndex, pSample);
        pSample->Release();

        if (FAILED(hr)) {
            reportError(ErrorCode::WriterWriteFailed, "WriteSample failed");
            return false;
        }

        m_frameCount++;
        return true;
    }

private:
    bool tryCreateWriter(const std::wstring& filePath, const GUID& videoCodec, const WriterConfig& config) {
        // Use MFCreateSinkWriterFromURL - the standard approach for file writing
        IMFSinkWriter* pWriter = nullptr;

        IMFAttributes* pAttributes = nullptr;
        HRESULT hr = MFCreateAttributes(&pAttributes, 2);
        if (FAILED(hr)) return false;

        pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        pAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

        hr = MFCreateSinkWriterFromURL(filePath.c_str(), nullptr, pAttributes, &pWriter);
        pAttributes->Release();

        if (FAILED(hr)) {
            CCAP_LOG_E("MFCreateSinkWriterFromURL failed: 0x%08lX\n", hr);
            return false;
        }

        // Configure output media type (encoded format)
        IMFMediaType* pOutputType = nullptr;
        hr = MFCreateMediaType(&pOutputType);
        if (FAILED(hr)) {
            pWriter->Release();
            return false;
        }

        pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pOutputType->SetGUID(MF_MT_SUBTYPE, videoCodec);
        pOutputType->SetUINT32(MF_MT_AVG_BITRATE, static_cast<UINT32>(effectiveBitRate(config)));
        pOutputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, config.width, config.height);

        UINT32 fpsNum = static_cast<UINT32>(config.frameRate * 1000);
        UINT32 fpsDen = 1000;
        if (config.frameRate <= 0) {
            fpsNum = 30000;
            fpsDen = 1000;
        }
        MFSetAttributeRatio(pOutputType, MF_MT_FRAME_RATE, fpsNum, fpsDen);

        DWORD streamIndex = 0;
        hr = pWriter->AddStream(pOutputType, &streamIndex);
        pOutputType->Release();
        if (FAILED(hr)) {
            CCAP_LOG_E("AddStream failed: 0x%08lX\n", hr);
            pWriter->Release();
            return false;
        }

        // Configure input media type (raw NV12)
        IMFMediaType* pInputType = nullptr;
        hr = MFCreateMediaType(&pInputType);
        if (FAILED(hr)) {
            pWriter->Release();
            return false;
        }

        pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        pInputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(pInputType, MF_MT_FRAME_SIZE, config.width, config.height);
        MFSetAttributeRatio(pInputType, MF_MT_FRAME_RATE, fpsNum, fpsDen);

        hr = pWriter->SetInputMediaType(streamIndex, pInputType, nullptr);
        pInputType->Release();

        if (FAILED(hr)) {
            CCAP_LOG_E("SetInputMediaType failed: 0x%08lX\n", hr);
            pWriter->Release();
            return false;
        }

        hr = pWriter->BeginWriting();
        if (FAILED(hr)) {
            CCAP_LOG_E("BeginWriting failed: 0x%08lX\n", hr);
            pWriter->Release();
            return false;
        }

        m_sinkWriter = pWriter;
        m_streamIndex = streamIndex;
        return true;
    }

    IMFSinkWriter* m_sinkWriter;
    DWORD m_streamIndex;
    bool m_mfInitialized;
    std::atomic<bool> m_isOpened{ false };
    std::atomic<int> m_frameCount{ 0 };
};

VideoWriter::Impl* createVideoWriterImpl() {
    return new WriterWindows();
}

} // namespace ccap

#endif // _WIN32
