/**
 * @file ccap_writer_imp.h
 * @brief Internal header for VideoWriter platform implementations.
 */

#pragma once

#ifndef CCAP_WRITER_IMP_H
#define CCAP_WRITER_IMP_H

#include "ccap_def.h"
#include "ccap_writer.h"

#include <cstring>
#include <string_view>
#include <vector>

namespace ccap {

void reportError(ErrorCode errorCode, std::string_view description);

struct VideoWriter::Impl {
    Impl() :
        m_actualCodec(VideoCodec::H264) {}
    virtual ~Impl() = default;

    virtual bool open(std::string_view filePath, const WriterConfig& config) = 0;
    virtual void close() = 0;
    virtual bool isOpened() const = 0;
    virtual bool writeFrame(const VideoFrame& frame, uint64_t timestampNs) = 0;

    VideoCodec m_actualCodec;
    WriterConfig m_config;
};

/// Factory function implemented per platform (Apple / Windows).
/// Returns nullptr on unsupported platforms.
VideoWriter::Impl* createVideoWriterImpl();

/// Compute auto bit rate based on resolution and frame rate.
///
/// Reference: YouTube official recommended bitrates for H.264 encoding.
/// https://support.google.com/youtube/answer/2853702
///
/// YouTube H.264 reference points (Mbps):
///   720p  @30fps → 7.5    720p  @60fps → 9.0
///   1080p @30fps → 10     1080p @60fps → 12
///   1440p @30fps → 15     1440p @60fps → 24
///   2160p @30fps → 30     2160p @60fps → 35
///
/// For resolutions below 720p, the 720p rate is used as floor.
/// For resolutions between reference points, bit rate is linearly interpolated by pixel count.
/// For resolutions above 4K, bit rate is extrapolated by pixel count.
/// For HEVC, bit rate is scaled down to ~60% of H.264 (HEVC achieves similar quality at lower bit rate).
inline uint64_t computeAutoBitRate(uint32_t width, uint32_t height, double frameRate, VideoCodec codec) {
    const double kMbps = 1'000'000.0;
    const double fps = (frameRate > 0.0) ? frameRate : 30.0;
    const bool is60fps = fps > 45.0;

    // YouTube H.264 reference data points: (pixelCount, bitrateInMbps)
    struct RefPoint {
        double pixels;
        double bitrateMbps;
    };
    static const RefPoint refs30[] = {
        { 1280 * 720, 7.5 },
        { 1920 * 1080, 10.0 },
        { 2560 * 1440, 15.0 },
        { 3840 * 2160, 30.0 },
    };
    static const RefPoint refs60[] = {
        { 1280 * 720, 9.0 },
        { 1920 * 1080, 12.0 },
        { 2560 * 1440, 24.0 },
        { 3840 * 2160, 35.0 },
    };

    const RefPoint* refs = is60fps ? refs60 : refs30;
    const int refCount = 4;
    const double pixels = static_cast<double>(width) * height;
    double bitrateMbps;

    if (pixels <= refs[0].pixels) {
        // Below 720p: use 720p floor
        bitrateMbps = refs[0].bitrateMbps;
    } else if (pixels >= refs[refCount - 1].pixels) {
        // Above 4K: extrapolate using slope of the last two reference points
        const auto& a = refs[refCount - 2];
        const auto& b = refs[refCount - 1];
        double slope = (b.bitrateMbps - a.bitrateMbps) / (b.pixels - a.pixels);
        bitrateMbps = b.bitrateMbps + slope * (pixels - b.pixels);
    } else {
        // Between reference points: linear interpolation by pixel count
        int i = 0;
        while (i < refCount - 1 && pixels > refs[i + 1].pixels)
            i++;
        const auto& lo = refs[i];
        const auto& hi = refs[i + 1];
        double t = (pixels - lo.pixels) / (hi.pixels - lo.pixels);
        bitrateMbps = lo.bitrateMbps + t * (hi.bitrateMbps - lo.bitrateMbps);
    }

    // Scale for non-standard frame rates between 30 and 60
    if (!is60fps && fps > 30.0) {
        const auto& r30 = refs30;
        const auto& r60 = refs60;
        // Average ratio across reference points
        double ratio = 0;
        for (int i = 0; i < refCount; i++)
            ratio += r60[i].bitrateMbps / r30[i].bitrateMbps;
        ratio /= refCount; // ~1.27
        double t = (fps - 30.0) / 30.0;
        bitrateMbps *= (1.0 + t * (ratio - 1.0));
    }

    // HEVC achieves similar visual quality at ~60% of H.264 bit rate
    if (codec == VideoCodec::HEVC) {
        bitrateMbps *= 0.6;
    }

    return static_cast<uint64_t>(bitrateMbps * kMbps);
}

/// Resolve effective bit rate: use user value if set, otherwise compute auto.
inline uint64_t effectiveBitRate(const WriterConfig& config) {
    if (config.bitRate > 0) return config.bitRate;
    return computeAutoBitRate(config.width, config.height, config.frameRate, config.codec);
}

// ---- Shared NV12 conversion helpers (used by both platform implementations) ----

inline int orientedRowIndex(FrameOrientation orientation, int row, int height) {
    return orientation == FrameOrientation::BottomToTop ? (height - 1 - row) : row;
}

inline void bgrToNv12(const uint8_t* src, int srcStride,
                      uint8_t* dstY, int dstYStride,
                      uint8_t* dstUV, int dstUVStride,
                      int width, int height, int bytesPerPixel,
                      FrameOrientation orientation) {
    // bytesPerPixel: 3 for BGR24, 4 for BGRA32
    const int w2 = width / 2;
    for (int y = 0; y < height; y += 2) {
        const uint8_t* line0 = src + orientedRowIndex(orientation, y, height) * srcStride;
        const uint8_t* line1 = (y + 1 < height) ? src + orientedRowIndex(orientation, y + 1, height) * srcStride : line0;
        for (int x = 0; x < w2; x++) {
            const int off = x * 2 * bytesPerPixel;
            int b0 = line0[off], g0 = line0[off + 1], r0 = line0[off + 2];
            int b1 = line0[off + bytesPerPixel], g1 = line0[off + bytesPerPixel + 1], r1 = line0[off + bytesPerPixel + 2];
            int b2 = line1[off], g2 = line1[off + 1], r2 = line1[off + 2];
            int b3 = line1[off + bytesPerPixel], g3 = line1[off + bytesPerPixel + 1], r3 = line1[off + bytesPerPixel + 2];
            dstY[y * dstYStride + x * 2] = static_cast<uint8_t>(((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16);
            dstY[y * dstYStride + x * 2 + 1] = static_cast<uint8_t>(((66 * r1 + 129 * g1 + 25 * b1 + 128) >> 8) + 16);
            dstY[(y + 1) * dstYStride + x * 2] = static_cast<uint8_t>(((66 * r2 + 129 * g2 + 25 * b2 + 128) >> 8) + 16);
            dstY[(y + 1) * dstYStride + x * 2 + 1] = static_cast<uint8_t>(((66 * r3 + 129 * g3 + 25 * b3 + 128) >> 8) + 16);
            int bAvg = (b0 + b1 + b2 + b3) / 4, rAvg = (r0 + r1 + r2 + r3) / 4, gAvg = (g0 + g1 + g2 + g3) / 4;
            dstUV[(y / 2) * dstUVStride + x * 2] = static_cast<uint8_t>(((-38 * rAvg - 74 * gAvg + 112 * bAvg + 128) >> 8) + 128);
            dstUV[(y / 2) * dstUVStride + x * 2 + 1] = static_cast<uint8_t>(((112 * rAvg - 94 * gAvg - 18 * bAvg + 128) >> 8) + 128);
        }
    }
}

/// Convert any supported pixel format to NV12 Y and UV planes.
/// Returns false on unsupported format. Requires even width/height.
inline bool convertFrameToNv12(const VideoFrame& frame,
                               std::vector<uint8_t>& yBuf, std::vector<uint8_t>& uvBuf,
                               uint32_t& yStride, uint32_t& uvStride) {
    const int w = static_cast<int>(frame.width);
    const int h = static_cast<int>(frame.height);
    if (w <= 0 || h <= 0 || (w % 2) != 0 || (h % 2) != 0) {
        return false;
    }
    const int w2 = w / 2;
    const int h2 = h / 2;
    const FrameOrientation orientation = frame.orientation;

    yStride = static_cast<uint32_t>(w);
    uvStride = static_cast<uint32_t>(w2 * 2);
    yBuf.resize(static_cast<size_t>(yStride) * h);
    uvBuf.resize(static_cast<size_t>(uvStride) * h2);

    switch (frame.pixelFormat) {
    case PixelFormat::NV12:
    case PixelFormat::NV12f:
        for (int y = 0; y < h; y++)
            std::memcpy(yBuf.data() + y * yStride,
                        frame.data[0] + orientedRowIndex(orientation, y, h) * frame.stride[0],
                        static_cast<size_t>(w));
        for (int y = 0; y < h2; y++)
            std::memcpy(uvBuf.data() + y * uvStride,
                        frame.data[1] + orientedRowIndex(orientation, y, h2) * frame.stride[1],
                        static_cast<size_t>(w2) * 2);
        return true;

    case PixelFormat::I420:
    case PixelFormat::I420f:
        for (int y = 0; y < h; y++)
            std::memcpy(yBuf.data() + y * yStride,
                        frame.data[0] + orientedRowIndex(orientation, y, h) * frame.stride[0],
                        static_cast<size_t>(w));
        for (int y = 0; y < h2; y++) {
            const int srcRow = orientedRowIndex(orientation, y, h2);
            for (int x = 0; x < w2; x++) {
                uvBuf[y * uvStride + x * 2] = frame.data[1][srcRow * frame.stride[1] + x];
                uvBuf[y * uvStride + x * 2 + 1] = frame.data[2][srcRow * frame.stride[2] + x];
            }
        }
        return true;

    case PixelFormat::BGR24:
        bgrToNv12(frame.data[0], static_cast<int>(frame.stride[0]),
                  yBuf.data(), static_cast<int>(yStride),
                  uvBuf.data(), static_cast<int>(uvStride), w, h, 3, orientation);
        return true;

    case PixelFormat::BGRA32:
        bgrToNv12(frame.data[0], static_cast<int>(frame.stride[0]),
                  yBuf.data(), static_cast<int>(yStride),
                  uvBuf.data(), static_cast<int>(uvStride), w, h, 4, orientation);
        return true;

    default:
        return false;
    }
}

} // namespace ccap

#endif // CCAP_WRITER_IMP_H
