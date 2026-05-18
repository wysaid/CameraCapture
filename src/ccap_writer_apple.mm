/**
 * @file ccap_writer_apple.mm
 * @author wysaid (this@wysaid.org)
 * @brief Video writer implementation for macOS using AVAssetWriter.
 * @date 2025-05
 */

#include "ccap_writer_imp.h"
#include "ccap_utils.h"

#if __APPLE__

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace ccap {

class WriterApple : public VideoWriter::Impl {
public:
    WriterApple() : m_assetWriter(nullptr), m_writerInput(nullptr),
                    m_pixelBufferAdaptor(nullptr), m_sessionStarted(false) {}

    ~WriterApple() override {
        close();
    }

    bool open(std::string_view filePath, const WriterConfig& config) override {
        if (config.width == 0 || config.height == 0) {
            reportError(ErrorCode::WriterOpenFailed, "Invalid dimensions: " + std::to_string(config.width) + "x" + std::to_string(config.height));
            return false;
        }
        if (config.width % 2 != 0 || config.height % 2 != 0) {
            reportError(ErrorCode::WriterOpenFailed, "Video dimensions must be even for NV12 encoding: " + std::to_string(config.width) + "x" + std::to_string(config.height));
            return false;
        }
        m_config = config;

        NSString* pathStr = [NSString stringWithUTF8String: std::string(filePath).c_str()];

        AVFileType fileType = AVFileTypeMPEG4;
        if (config.container == VideoFormat::MOV) {
            fileType = AVFileTypeQuickTimeMovie;
        }

        // Try requested codec first, then fallback
        AVVideoCodecType codecs[2];
        VideoCodec cppCodecs[2];
        if (config.codec == VideoCodec::H264) {
            codecs[0] = AVVideoCodecTypeH264;  cppCodecs[0] = VideoCodec::H264;
            codecs[1] = AVVideoCodecTypeHEVC;   cppCodecs[1] = VideoCodec::HEVC;
        } else {
            codecs[0] = AVVideoCodecTypeHEVC;   cppCodecs[0] = VideoCodec::HEVC;
            codecs[1] = AVVideoCodecTypeH264;   cppCodecs[1] = VideoCodec::H264;
        }

        for (int i = 0; i < 2; i++) {
            if (tryOpen(fileType, pathStr, codecs[i])) {
                m_actualCodec = cppCodecs[i];
                return true;
            }
        }

        reportError(ErrorCode::WriterOpenFailed, "Failed to create video writer with any supported codec");
        return false;
    }

private:
    bool tryOpen(AVFileType fileType, NSString* pathStr, AVVideoCodecType codec) {
        NSURL* url = [NSURL fileURLWithPath: pathStr];
        NSError* error = nil;
        int64_t bitRate = static_cast<int64_t>(effectiveBitRate(m_config));
        int frameRateInt = (m_config.frameRate > 0) ? static_cast<int>(m_config.frameRate) : 30;
        int maxKeyFrameInterval = frameRateInt * 2;

        NSDictionary* videoSettings = @{
            AVVideoCodecKey: codec,
            AVVideoWidthKey: @(m_config.width),
            AVVideoHeightKey: @(m_config.height),
            AVVideoCompressionPropertiesKey: @{
                AVVideoAverageBitRateKey: @(bitRate),
                AVVideoExpectedSourceFrameRateKey: @(frameRateInt),
                AVVideoMaxKeyFrameIntervalKey: @(maxKeyFrameInterval),
            },
        };

        // Delete existing file
        [[NSFileManager defaultManager] removeItemAtPath: pathStr error: nil];

        @try {
            m_assetWriter = [[AVAssetWriter alloc] initWithURL: url fileType: fileType error: &error];
            if (error) {
                CCAP_LOG_E("AVAssetWriter creation failed: %s\n", error.localizedDescription.UTF8String);
                m_assetWriter = nil;
                return false;
            }

            m_writerInput = [AVAssetWriterInput assetWriterInputWithMediaType: AVMediaTypeVideo
                                                               outputSettings: videoSettings];
            if (!m_writerInput) {
                CCAP_LOG_E("AVAssetWriterInput creation failed\n");
                [m_assetWriter cancelWriting];
                m_assetWriter = nil;
                return false;
            }

            m_writerInput.expectsMediaDataInRealTime = NO;
            [m_assetWriter addInput: m_writerInput];

            NSDictionary* pixelBufferAttrs = @{
                (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
                (id)kCVPixelBufferWidthKey: @(m_config.width),
                (id)kCVPixelBufferHeightKey: @(m_config.height),
            };

            m_pixelBufferAdaptor = [AVAssetWriterInputPixelBufferAdaptor
                assetWriterInputPixelBufferAdaptorWithAssetWriterInput: m_writerInput
                sourcePixelBufferAttributes: pixelBufferAttrs];
            if (!m_pixelBufferAdaptor) {
                CCAP_LOG_E("Pixel buffer adaptor creation failed\n");
                [m_assetWriter cancelWriting];
                m_assetWriter = nil;
                m_writerInput = nil;
                return false;
            }

            if (![m_assetWriter startWriting]) {
                CCAP_LOG_E("startWriting failed: %s\n", m_assetWriter.error.localizedDescription.UTF8String);
                [m_assetWriter cancelWriting];
                m_assetWriter = nil;
                m_writerInput = nil;
                m_pixelBufferAdaptor = nil;
                return false;
            }

            [m_assetWriter startSessionAtSourceTime: CMTimeMake(0, 1)];
            m_sessionStarted = YES;
            m_frameCount = 0;
            m_isOpened = true;
            return true;
        }
        @catch (NSException* e) {
            CCAP_LOG_E("Exception during writer setup: %s\n", e.reason.UTF8String);
            m_assetWriter = nil;
            m_writerInput = nil;
            m_pixelBufferAdaptor = nil;
            return false;
        }
    }

public:

    void close() override {
        if (!m_isOpened) return;
        m_isOpened = false;

        AVAssetWriter* assetWriter = m_assetWriter;
        AVAssetWriterInput* writerInput = m_writerInput;

        @try {
            if (writerInput) {
                [writerInput markAsFinished];
            }
            if (assetWriter) {
                dispatch_semaphore_t sem = dispatch_semaphore_create(0);
                [assetWriter finishWritingWithCompletionHandler:^{
                    dispatch_semaphore_signal(sem);
                }];

                const long waitResult = dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
                if (waitResult != 0) {
                    reportError(ErrorCode::WriterCloseFailed, "finishWriting timed out after 10 seconds");
                } else if (assetWriter.error) {
                    reportError(ErrorCode::WriterCloseFailed, "finishWriting failed: " + std::string(assetWriter.error.localizedDescription.UTF8String));
                }
            }
        }
        @catch (NSException* e) {
            reportError(ErrorCode::WriterCloseFailed, "Exception during writer close: " + std::string(e.reason.UTF8String));
        }

        m_pixelBufferAdaptor = nil;
        m_writerInput = nil;
        m_assetWriter = nil;
        m_sessionStarted = NO;
        m_frameCount = 0;
        std::memset(&m_config, 0, sizeof(m_config));
    }

    bool isOpened() const override {
        return m_isOpened;
    }

    bool writeFrame(const VideoFrame& frame, uint64_t timestampNs) override {
        if (!m_isOpened || !m_writerInput || !m_assetWriter || !m_pixelBufferAdaptor) return false;

        if (frame.width != m_config.width || frame.height != m_config.height) {
            reportError(ErrorCode::WriterWriteFailed, "Frame dimensions " + std::to_string(frame.width) + "x" + std::to_string(frame.height) +
                " do not match configured " + std::to_string(m_config.width) + "x" + std::to_string(m_config.height));
            return false;
        }

        @try {
            // Wait for writer input to be ready (with 2 second timeout)
            int waitMs = 0;
            while (![m_writerInput isReadyForMoreMediaData]) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (++waitMs > 2000) {
                    reportError(ErrorCode::WriterWriteFailed, "Writer input not ready after 2s, dropping frame");
                    return false;
                }
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

            // Create CVPixelBuffer
            CVPixelBufferRef pixelBuffer = nullptr;
            CVReturn ret = CVPixelBufferCreate(kCFAllocatorDefault, w, h,
                                               kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                                               nullptr, &pixelBuffer);
            if (ret != kCVReturnSuccess) {
                reportError(ErrorCode::WriterWriteFailed, "CVPixelBufferCreate failed: " + std::to_string(ret));
                return false;
            }

            // Fill pixel buffer with converted data
            CVPixelBufferLockBaseAddress(pixelBuffer, 0);
            uint8_t* dstY = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
            size_t dstYStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
            uint8_t* dstUV = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
            size_t dstUVStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

            for (int y = 0; y < h; y++) {
                memcpy(dstY + y * dstYStride, yBuf.data() + y * yStride, static_cast<size_t>(w));
            }
            for (int y = 0; y < h2; y++) {
                memcpy(dstUV + y * dstUVStride, uvBuf.data() + y * uvStride, static_cast<size_t>(w2) * 2);
            }

            CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

            // Calculate timestamp using a high timescale for precision
            static constexpr int32_t kTimeScale = 600 * 1000; // 600000 supports common frame rates accurately
            CMTime presentationTime;
            if (timestampNs > 0) {
                presentationTime = CMTimeMake(static_cast<int64_t>(timestampNs / 1000000.0 * kTimeScale / 1000.0), kTimeScale);
            } else {
                double fps = m_config.frameRate > 0 ? m_config.frameRate : 30.0;
                int64_t timeValue = static_cast<int64_t>(m_frameCount * (static_cast<double>(kTimeScale) / fps));
                presentationTime = CMTimeMake(timeValue, kTimeScale);
            }

            // Append pixel buffer via adaptor
            BOOL success = [m_pixelBufferAdaptor appendPixelBuffer: pixelBuffer
                                              withPresentationTime: presentationTime];
            CVPixelBufferRelease(pixelBuffer);

            if (!success) {
                reportError(ErrorCode::WriterWriteFailed, "appendPixelBuffer failed: " +
                    std::string(m_assetWriter.error ? m_assetWriter.error.localizedDescription.UTF8String : "unknown"));
                return false;
            }

            m_frameCount++;
            return true;
        }
        @catch (NSException* e) {
            reportError(ErrorCode::WriterWriteFailed, "Exception during writeFrame: " + std::string(e.reason.UTF8String));
            return false;
        }
    }

private:
    AVAssetWriter* m_assetWriter;
    AVAssetWriterInput* m_writerInput;
    AVAssetWriterInputPixelBufferAdaptor* m_pixelBufferAdaptor;
    BOOL m_sessionStarted;
    std::atomic<bool> m_isOpened{false};
    std::atomic<int> m_frameCount{0};
};

VideoWriter::Impl* createVideoWriterImpl() {
    return new WriterApple();
}

} // namespace ccap

#endif // __APPLE__
