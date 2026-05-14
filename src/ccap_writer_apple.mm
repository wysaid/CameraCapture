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
#include <mutex>
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
            CCAP_LOG_E("Invalid dimensions: %ux%u\n", config.width, config.height);
            return false;
        }
        m_config = config;

        NSString* pathStr = [NSString stringWithUTF8String: std::string(filePath).c_str()];

        // Determine output file type
        AVFileType fileType = AVFileTypeMPEG4; // MP4
        if (config.container == VideoFormat::MOV) {
            fileType = AVFileTypeQuickTimeMovie;
        }

        // Try HEVC first, fallback to H.264
        AVVideoCodecType codecs[] = { AVVideoCodecTypeHEVC, AVVideoCodecTypeH264 };
        VideoCodec cppCodecs[] = { VideoCodec::HEVC, VideoCodec::H264 };

        for (int i = 0; i < 2; i++) {
            if (tryOpen(filePath, fileType, pathStr, codecs[i])) {
                m_actualCodec = cppCodecs[i];
                return true;
            }
        }

        CCAP_LOG_E("Failed to create video writer with HEVC or H.264\n");
        return false;
    }

private:
    bool tryOpen(std::string_view, AVFileType fileType, NSString* pathStr, AVVideoCodecType codec) {
        NSURL* url = [NSURL fileURLWithPath: pathStr];
        NSError* error = nil;
        int64_t bitRate = (m_config.bitRate > 0) ? static_cast<int64_t>(m_config.bitRate) : static_cast<int64_t>(m_config.width) * m_config.height * 4;
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

            // CRITICAL: Pixel buffer adaptor MUST be created before startWriting
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

            // Start session at time 0 so we can append samples immediately
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

        @try {
            if (m_writerInput) {
                [m_writerInput markAsFinished];
            }
            if (m_assetWriter) {
                // Use a background queue to avoid blocking the calling thread
                // which allows the completion handler to execute
                dispatch_queue_t queue = dispatch_queue_create("com.ccap.writer.close", DISPATCH_QUEUE_SERIAL);
                dispatch_semaphore_t sem = dispatch_semaphore_create(0);
                dispatch_async(queue, ^{
                    [m_assetWriter finishWritingWithCompletionHandler:^{
                        dispatch_semaphore_signal(sem);
                    }];
                });
                // Wait for completion
                dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));

                if (m_assetWriter.error) {
                    CCAP_LOG_E("finishWriting failed: %s\n",
                            m_assetWriter.error.localizedDescription.UTF8String);
                }
            }
        }
        @catch (NSException* e) {
            CCAP_LOG_E("Exception during writer close: %s\n", e.reason.UTF8String);
        }

        m_pixelBufferAdaptor = nil;
        m_writerInput = nil;
        m_assetWriter = nil;
        std::memset(&m_config, 0, sizeof(m_config));
    }

    bool isOpened() const override {
        return m_isOpened;
    }

    bool writeFrame(const VideoFrame& frame, uint64_t timestampNs) override {
        if (!m_isOpened || !m_writerInput || !m_assetWriter || !m_pixelBufferAdaptor) return false;

        @try {
            // Wait for writer input to be ready (with 2 second timeout)
            int waitMs = 0;
            while (![m_writerInput isReadyForMoreMediaData]) {
                usleep(1000); // 1ms
                if (++waitMs > 2000) {
                    CCAP_LOG_W("Writer input not ready after 2s, dropping frame\n");
                    return false;
                }
            }

            int w = static_cast<int>(frame.width);
            int h = static_cast<int>(frame.height);
            int w2 = (w + 1) / 2;
            int h2 = (h + 1) / 2;

            // Convert frame to NV12
            std::vector<uint8_t> yBuf, uvBuf;
            uint32_t yStride, uvStride;
            {
                yStride = static_cast<uint32_t>(w);
                uvStride = static_cast<uint32_t>(w2 * 2);
                yBuf.resize(static_cast<size_t>(yStride) * h);
                uvBuf.resize(static_cast<size_t>(uvStride) * h2);

                uint8_t* dstYTmp = yBuf.data();
                uint8_t* dstUVTmp = uvBuf.data();

                if (frame.pixelFormat == PixelFormat::NV12 || frame.pixelFormat == PixelFormat::NV12f) {
                    for (int y = 0; y < h; y++) {
                        memcpy(dstYTmp + y * yStride, frame.data[0] + y * frame.stride[0], static_cast<size_t>(w));
                    }
                    for (int y = 0; y < h2; y++) {
                        memcpy(dstUVTmp + y * uvStride, frame.data[1] + y * frame.stride[1], static_cast<size_t>(w2) * 2);
                    }
                } else if (frame.pixelFormat == PixelFormat::I420 || frame.pixelFormat == PixelFormat::I420f) {
                    for (int y = 0; y < h; y++) {
                        memcpy(dstYTmp + y * yStride, frame.data[0] + y * frame.stride[0], static_cast<size_t>(w));
                    }
                    for (int y = 0; y < h2; y++) {
                        for (int x = 0; x < w2; x++) {
                            dstUVTmp[y * uvStride + x * 2] = frame.data[1][y * frame.stride[1] + x];
                            dstUVTmp[y * uvStride + x * 2 + 1] = frame.data[2][y * frame.stride[2] + x];
                        }
                    }
                } else if (frame.pixelFormat == PixelFormat::BGR24) {
                    bgr24ToNv12(frame.data[0], static_cast<int>(frame.stride[0]),
                               dstYTmp, static_cast<int>(yStride),
                               dstUVTmp, static_cast<int>(uvStride), w, h);
                } else if (frame.pixelFormat == PixelFormat::BGRA32) {
                    bgra32ToNv12(frame.data[0], static_cast<int>(frame.stride[0]),
                                dstYTmp, static_cast<int>(yStride),
                                dstUVTmp, static_cast<int>(uvStride), w, h);
                } else {
                    CCAP_LOG_E("Unsupported pixel format for writer on macOS: %d\n",
                               static_cast<int>(frame.pixelFormat));
                    return false;
                }
            }

            // Create CVPixelBuffer
            CVPixelBufferRef pixelBuffer = nullptr;
            CVReturn ret = CVPixelBufferCreate(kCFAllocatorDefault, w, h,
                                               kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                                               nullptr, &pixelBuffer);
            if (ret != kCVReturnSuccess) {
                CCAP_LOG_E("CVPixelBufferCreate failed: %d\n", ret);
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

            // Calculate timestamp
            CMTime presentationTime;
            if (timestampNs > 0) {
                presentationTime = CMTimeMake(static_cast<int64_t>(timestampNs), 1000000000);
            } else {
                double fps = m_config.frameRate > 0 ? m_config.frameRate : 30.0;
                presentationTime = CMTimeMake(static_cast<int64_t>(m_frameCount), static_cast<int32_t>(fps));
            }

            // Append pixel buffer via adaptor
            BOOL success = [m_pixelBufferAdaptor appendPixelBuffer: pixelBuffer
                                              withPresentationTime: presentationTime];
            CVPixelBufferRelease(pixelBuffer);

            if (!success) {
                CCAP_LOG_E("appendPixelBuffer failed: %s\n",
                           m_assetWriter.error ? m_assetWriter.error.localizedDescription.UTF8String : "unknown");
                return false;
            }

            m_frameCount++;
            return true;
        }
        @catch (NSException* e) {
            CCAP_LOG_E("Exception during writeFrame: %s\n", e.reason.UTF8String);
            return false;
        }
    }

private:
    // Inline conversion helpers
    void bgr24ToNv12(const uint8_t* src, int srcStride,
                     uint8_t* dstY, int dstYStride,
                     uint8_t* dstUV, int dstUVStride,
                     int width, int height) {
        int w2 = width / 2;
        const uint8_t* line = src;
        for (int y = 0; y < height; y += 2) {
            const uint8_t* line0 = line;
            const uint8_t* line1 = (y + 1 < height) ? line + srcStride : line;
            for (int x = 0; x < w2; x++) {
                int b0 = line0[x*6+0], g0 = line0[x*6+1], r0 = line0[x*6+2];
                int b1 = line0[x*6+3], g1 = line0[x*6+4], r1 = line0[x*6+5];
                int b2 = line1[x*6+0], g2 = line1[x*6+1], r2 = line1[x*6+2];
                int b3 = line1[x*6+3], g3 = line1[x*6+4], r3 = line1[x*6+5];
                dstY[y * dstYStride + x*2]     = static_cast<uint8_t>((66*r0+129*g0+25*b0+128)>>8)+16;
                dstY[y * dstYStride + x*2+1]   = static_cast<uint8_t>((66*r1+129*g1+25*b1+128)>>8)+16;
                dstY[(y+1) * dstYStride + x*2]     = static_cast<uint8_t>((66*r2+129*g2+25*b2+128)>>8)+16;
                dstY[(y+1) * dstYStride + x*2+1]   = static_cast<uint8_t>((66*r3+129*g3+25*b3+128)>>8)+16;
                int bAvg = (b0+b1+b2+b3)/4, rAvg = (r0+r1+r2+r3)/4, gAvg = (g0+g1+g2+g3)/4;
                dstUV[(y/2) * dstUVStride + x*2]     = static_cast<uint8_t>((-38*rAvg-74*gAvg+112*bAvg+128)>>8)+128;
                dstUV[(y/2) * dstUVStride + x*2+1]   = static_cast<uint8_t>((112*rAvg-94*gAvg-18*bAvg+128)>>8)+128;
            }
            line += srcStride * 2;
        }
    }

    void bgra32ToNv12(const uint8_t* src, int srcStride,
                      uint8_t* dstY, int dstYStride,
                      uint8_t* dstUV, int dstUVStride,
                      int width, int height) {
        int w2 = width / 2;
        const uint8_t* line = src;
        for (int y = 0; y < height; y += 2) {
            const uint8_t* line0 = line;
            const uint8_t* line1 = (y + 1 < height) ? line + srcStride : line;
            for (int x = 0; x < w2; x++) {
                int b0 = line0[x*8+0], g0 = line0[x*8+1], r0 = line0[x*8+2];
                int b1 = line0[x*8+4], g1 = line0[x*8+5], r1 = line0[x*8+6];
                int b2 = line1[x*8+0], g2 = line1[x*8+1], r2 = line1[x*8+2];
                int b3 = line1[x*8+4], g3 = line1[x*8+5], r3 = line1[x*8+6];
                dstY[y * dstYStride + x*2]     = static_cast<uint8_t>((66*r0+129*g0+25*b0+128)>>8)+16;
                dstY[y * dstYStride + x*2+1]   = static_cast<uint8_t>((66*r1+129*g1+25*b1+128)>>8)+16;
                dstY[(y+1) * dstYStride + x*2]     = static_cast<uint8_t>((66*r2+129*g2+25*b2+128)>>8)+16;
                dstY[(y+1) * dstYStride + x*2+1]   = static_cast<uint8_t>((66*r3+129*g3+25*b3+128)>>8)+16;
                int bAvg = (b0+b1+b2+b3)/4, rAvg = (r0+r1+r2+r3)/4, gAvg = (g0+g1+g2+g3)/4;
                dstUV[(y/2) * dstUVStride + x*2]     = static_cast<uint8_t>((-38*rAvg-74*gAvg+112*bAvg+128)>>8)+128;
                dstUV[(y/2) * dstUVStride + x*2+1]   = static_cast<uint8_t>((112*rAvg-94*gAvg-18*bAvg+128)>>8)+128;
            }
            line += srcStride * 2;
        }
    }

    AVAssetWriter* m_assetWriter;
    AVAssetWriterInput* m_writerInput;
    AVAssetWriterInputPixelBufferAdaptor* m_pixelBufferAdaptor;
    BOOL m_sessionStarted;
    std::atomic<bool> m_isOpened{false};
    std::atomic<int> m_frameCount{0};
};

} // namespace ccap

#endif // __APPLE__
