/**
 * @file ccap_writer_windows.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Video writer implementation for Windows using Media Foundation Sink Writer.
 * @date 2025-05
 */

#include "ccap_writer_imp.h"
#include "ccap_utils.h"

#if defined(_WIN32) || defined(_MSC_VER)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <atomic>
#include <codecapi.h>
#include <mutex>
#include <vector>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace ccap {

class WriterWindows : public VideoWriter::Impl {
public:
    WriterWindows() : m_sinkWriter(nullptr), m_streamIndex(0), m_mfInitialized(false) {
        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
        m_mfInitialized = SUCCEEDED(hr);
    }

    ~WriterWindows() override {
        close();
        if (m_mfInitialized) {
            MFShutdown();
        }
    }

    bool open(std::string_view filePath, const WriterConfig& config) override {
        m_config = config;

        // Convert path to wide string
        std::wstring widePath(filePath.begin(), filePath.end());

        // Try HEVC first, fallback to H.264
        GUID videoCodec = MFVideoFormat_HEVC;
        m_actualCodec = VideoCodec::HEVC;
        if (!tryCreateWriter(widePath, videoCodec, config)) {
            videoCodec = MFVideoFormat_H264;
            m_actualCodec = VideoCodec::H264;
            if (!tryCreateWriter(widePath, videoCodec, config)) {
                CCAP_LOG_E("Failed to create video writer with H.264 or HEVC\n");
                return false;
            }
        }

        m_frameCount = 0;
        m_isOpened = true;
        return true;
    }

    void close() override {
        if (!m_isOpened) return;
        m_isOpened = false;

        if (m_sinkWriter) {
            m_sinkWriter->Finalize();
            m_sinkWriter->Release();
            m_sinkWriter = nullptr;
        }
    }

    bool isOpened() const override {
        return m_isOpened;
    }

    bool writeFrame(const VideoFrame& frame, uint64_t timestampNs) override {
        if (!m_isOpened || !m_sinkWriter) return false;

        int w = static_cast<int>(frame.width);
        int h = static_cast<int>(frame.height);
        int w2 = (w + 1) / 2;
        int h2 = (h + 1) / 2;

        // Convert frame to NV12
        std::vector<uint8_t> yBuf, uvBuf;
        uint32_t yStride, uvStride;
        if (!convertToNv12(frame, yBuf, uvBuf, yStride, uvStride)) {
            return false;
        }

        // Create total buffer size: Y + UV
        int totalSize = static_cast<int>(yStride) * h + static_cast<int>(uvStride) * h2;

        // Create sample
        IMFSample* pSample = nullptr;
        HRESULT hr = MFCreateSample(&pSample);
        if (FAILED(hr)) {
            CCAP_LOG_E("MFCreateSample failed: 0x%08X\n", hr);
            return false;
        }

        IMFSinkWriter* pWriter = m_sinkWriter;

        // Add buffer
        IMFMediaBuffer* pBuffer = nullptr;
        hr = MFCreateMemoryBuffer(static_cast<DWORD>(totalSize), &pBuffer);
        if (FAILED(hr)) {
            CCAP_LOG_E("MFCreateMemoryBuffer failed: 0x%08X\n", hr);
            pSample->Release();
            return false;
        }

        BYTE* pData = nullptr;
        hr = pBuffer->Lock(&pData, nullptr, nullptr);
        if (FAILED(hr)) {
            CCAP_LOG_E("Buffer Lock failed: 0x%08X\n", hr);
            pBuffer->Release();
            pSample->Release();
            return false;
        }

        // Copy Y plane
        for (int y = 0; y < h; y++) {
            memcpy(pData + y * yStride, yBuf.data() + y * yStride, static_cast<size_t>(w));
        }
        // Copy UV plane
        uint8_t* uvStart = pData + static_cast<int>(yStride) * h;
        for (int y = 0; y < h2; y++) {
            memcpy(uvStart + y * uvStride, uvBuf.data() + y * uvStride, static_cast<size_t>(w2) * 2);
        }

        pBuffer->Unlock();
        pBuffer->SetCurrentLength(static_cast<DWORD>(totalSize));
        pSample->AddBuffer(pBuffer);
        pBuffer->Release();

        // Set timestamp
        LONGLONG hnsTimestamp;
        if (timestampNs > 0) {
            hnsTimestamp = static_cast<LONGLONG>(timestampNs / 100); // ns to 100ns
        } else {
            double fps = m_config.frameRate > 0 ? m_config.frameRate : 30.0;
            hnsTimestamp = static_cast<LONGLONG>(m_frameCount * 10000000.0 / fps);
        }
        pSample->SetSampleTime(hnsTimestamp);

        // Write sample
        hr = pWriter->WriteSample(m_streamIndex, pSample);
        pSample->Release();

        if (FAILED(hr)) {
            CCAP_LOG_E("WriteSample failed: 0x%08X\n", hr);
            return false;
        }

        m_frameCount++;
        return true;
    }

private:
    bool tryCreateWriter(const std::wstring& filePath, GUID videoCodec, const WriterConfig& config) {
        IMFAttributes* pAttributes = nullptr;
        HRESULT hr = MFCreateAttributes(&pAttributes, 1);
        if (FAILED(hr)) return false;

        hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        if (FAILED(hr)) {
            pAttributes->Release();
            return false;
        }

        IMFAttributes* pEncodingAttributes = nullptr;
        if (videoCodec == MFVideoFormat_H264) {
            hr = MFCreateAttributes(&pEncodingAttributes, 2);
            if (SUCCEEDED(hr)) {
                hr = pEncodingAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
            }
        }

        IMFMediaSink* pSink = nullptr;
        hr = MFCreateMediaSinkForURL(filePath.c_str(), pAttributes, &pSink);
        if (FAILED(hr)) {
            CCAP_LOG_E("MFCreateMediaSinkForURL failed: 0x%08X\n", hr);
            pAttributes->Release();
            if (pEncodingAttributes) pEncodingAttributes->Release();
            return false;
        }

        IMFMediaType* pOutputType = nullptr;
        hr = MFCreateMediaType(&pOutputType);
        if (FAILED(hr)) {
            pSink->Release();
            pAttributes->Release();
            if (pEncodingAttributes) pEncodingAttributes->Release();
            return false;
        }

        pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pOutputType->SetGUID(MF_MT_SUBTYPE, videoCodec);
        pOutputType->SetUINT32(MF_MT_AVG_BITRATE, static_cast<UINT32>(config.bitRate > 0 ? config.bitRate : config.width * config.height * 4));
        pOutputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

        // Set frame rate
        UINT32 fpsNum = static_cast<UINT32>(config.frameRate * 1000);
        UINT32 fpsDen = 1000;
        if (config.frameRate <= 0) { fpsNum = 30000; fpsDen = 1000; }
        MFSetAttributeRatio(pOutputType, MF_MT_FRAME_RATE, fpsNum, fpsDen);

        MFSetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, config.width, config.height);

        hr = pSink->AddStream(pOutputType, &m_streamIndex);
        pOutputType->Release();
        if (FAILED(hr)) {
            CCAP_LOG_E("AddStream failed: 0x%08X\n", hr);
            pSink->Release();
            pAttributes->Release();
            if (pEncodingAttributes) pEncodingAttributes->Release();
            return false;
        }

        hr = MFCreateSinkWriterFromMediaSink(pSink, pEncodingAttributes, &m_sinkWriter);
        pSink->Release();
        pAttributes->Release();
        if (pEncodingAttributes) pEncodingAttributes->Release();

        if (FAILED(hr)) {
            CCAP_LOG_E("MFCreateSinkWriterFromMediaSink failed: 0x%08X\n", hr);
            return false;
        }

        // Set input type (NV12)
        IMFMediaType* pInputType = nullptr;
        hr = MFCreateMediaType(&pInputType);
        if (FAILED(hr)) {
            m_sinkWriter->Release();
            m_sinkWriter = nullptr;
            return false;
        }

        pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        pInputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(pInputType, MF_MT_FRAME_SIZE, config.width, config.height);
        MFSetAttributeRatio(pInputType, MF_MT_FRAME_RATE, fpsNum, fpsDen);

        hr = m_sinkWriter->SetInputMediaType(m_streamIndex, pInputType, nullptr);
        pInputType->Release();

        if (FAILED(hr)) {
            CCAP_LOG_E("SetInputMediaType failed: 0x%08X\n", hr);
            m_sinkWriter->Release();
            m_sinkWriter = nullptr;
            return false;
        }

        // Start writing
        hr = m_sinkWriter->BeginWriting();
        if (FAILED(hr)) {
            CCAP_LOG_E("BeginWriting failed: 0x%08X\n", hr);
            m_sinkWriter->Release();
            m_sinkWriter = nullptr;
            return false;
        }

        return true;
    }

    bool convertToNv12(const VideoFrame& frame,
                       std::vector<uint8_t>& yBuf, std::vector<uint8_t>& uvBuf,
                       uint32_t& yStride, uint32_t& uvStride) {
        int w = static_cast<int>(frame.width);
        int h = static_cast<int>(frame.height);
        int w2 = (w + 1) / 2;
        int h2 = (h + 1) / 2;

        yStride = static_cast<uint32_t>(w);
        uvStride = static_cast<uint32_t>(w2 * 2);

        yBuf.resize(static_cast<size_t>(yStride) * h);
        uvBuf.resize(static_cast<size_t>(uvStride) * h2);

        if (frame.pixelFormat == PixelFormat::BGR24) {
            const uint8_t* src = frame.data[0];
            int srcStride = static_cast<int>(frame.stride[0]);
            uint8_t* dstY = yBuf.data();
            uint8_t* dstUV = uvBuf.data();
            for (int y = 0; y < h; y += 2) {
                const uint8_t* l0 = src + y * srcStride;
                const uint8_t* l1 = src + (y + 1 < h ? (y + 1) * srcStride : y * srcStride);
                for (int x = 0; x < w2; x++) {
                    int b0 = l0[x*6+0], g0 = l0[x*6+1], r0 = l0[x*6+2];
                    int b1 = l0[x*6+3], g1 = l0[x*6+4], r1 = l0[x*6+5];
                    int b2 = l1[x*6+0], g2 = l1[x*6+1], r2 = l1[x*6+2];
                    int b3 = l1[x*6+3], g3 = l1[x*6+4], r3 = l1[x*6+5];
                    dstY[y*yStride + x*2]     = static_cast<uint8_t>((66*r0+129*g0+25*b0+128)>>8)+16;
                    dstY[y*yStride + x*2+1]   = static_cast<uint8_t>((66*r1+129*g1+25*b1+128)>>8)+16;
                    dstY[(y+1)*yStride + x*2]     = static_cast<uint8_t>((66*r2+129*g2+25*b2+128)>>8)+16;
                    dstY[(y+1)*yStride + x*2+1]   = static_cast<uint8_t>((66*r3+129*g3+25*b3+128)>>8)+16;
                    int bA=(b0+b1+b2+b3)/4, rA=(r0+r1+r2+r3)/4, gA=(g0+g1+g2+g3)/4;
                    dstUV[(y/2)*uvStride + x*2]     = static_cast<uint8_t>((-38*rA-74*gA+112*bA+128)>>8)+128;
                    dstUV[(y/2)*uvStride + x*2+1]   = static_cast<uint8_t>((112*rA-94*gA-18*bA+128)>>8)+128;
                }
            }
        } else if (frame.pixelFormat == PixelFormat::NV12 || frame.pixelFormat == PixelFormat::NV12f) {
            for (int y = 0; y < h; y++) {
                memcpy(yBuf.data() + y * yStride, frame.data[0] + y * frame.stride[0], static_cast<size_t>(w));
            }
            for (int y = 0; y < h2; y++) {
                memcpy(uvBuf.data() + y * uvStride, frame.data[1] + y * frame.stride[1], static_cast<size_t>(w2) * 2);
            }
        } else {
            CCAP_LOG_E("Unsupported pixel format for writer on Windows: %d\n",
                       static_cast<int>(frame.pixelFormat));
            return false;
        }
        return true;
    }

    IMFSinkWriter* m_sinkWriter;
    DWORD m_streamIndex;
    bool m_mfInitialized;
    std::atomic<bool> m_isOpened{false};
    std::atomic<int> m_frameCount{0};
};

} // namespace ccap

#endif // _WIN32
