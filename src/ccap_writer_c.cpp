/**
 * @file ccap_writer_c.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Pure C interface implementation for ccap video writer.
 * @date 2025-05
 */

#include "ccap_writer_c.h"

#ifdef CCAP_ENABLE_VIDEO_WRITER

#include "ccap_writer.h"

extern "C" {

CcapVideoWriter* ccap_video_writer_create(void) {
    try {
        return reinterpret_cast<CcapVideoWriter*>(new ccap::VideoWriter());
    } catch (...) {
        return nullptr;
    }
}

void ccap_video_writer_destroy(CcapVideoWriter* writer) {
    if (!writer) return;
    try {
        delete reinterpret_cast<ccap::VideoWriter*>(writer);
    } catch (...) {
        // Never throw across C ABI boundary.
    }
}

bool ccap_video_writer_open(CcapVideoWriter* writer, const char* filePath,
                            const CcapWriterConfig* config) {
    if (!writer || !filePath || !config) return false;

    try {
        auto* cppWriter = reinterpret_cast<ccap::VideoWriter*>(writer);

        ccap::WriterConfig cppConfig;
        cppConfig.codec = (config->codec == CCAP_VIDEO_CODEC_HEVC) ? ccap::VideoCodec::HEVC : ccap::VideoCodec::H264;
        cppConfig.container = (config->container == CCAP_VIDEO_FORMAT_MOV) ? ccap::VideoFormat::MOV : ccap::VideoFormat::MP4;
        cppConfig.width = config->width;
        cppConfig.height = config->height;
        cppConfig.frameRate = config->frameRate;
        cppConfig.bitRate = config->bitRate;

        return cppWriter->open(filePath, cppConfig);
    } catch (...) {
        return false;
    }
}

void ccap_video_writer_close(CcapVideoWriter* writer) {
    if (!writer) return;
    try {
        reinterpret_cast<ccap::VideoWriter*>(writer)->close();
    } catch (...) {
        // Never throw across C ABI boundary.
    }
}

bool ccap_video_writer_is_opened(const CcapVideoWriter* writer) {
    if (!writer) return false;
    try {
        return reinterpret_cast<const ccap::VideoWriter*>(writer)->isOpened();
    } catch (...) {
        return false;
    }
}

bool ccap_video_writer_write_frame(CcapVideoWriter* writer,
                                   const CcapVideoFrameInfo* frameInfo,
                                   uint64_t timestampNs) {
    if (!writer || !frameInfo) return false;

    try {
        auto* cppWriter = reinterpret_cast<ccap::VideoWriter*>(writer);

        // Build a temporary VideoFrame wrapper
        // Note: CcapVideoFrame is actually a shared_ptr<ccap::VideoFrame>
        // But for write_frame we construct a minimal VideoFrame on the stack
        ccap::VideoFrame frame;
        for (int i = 0; i < 3; i++) {
            frame.data[i] = frameInfo->data[i];
            frame.stride[i] = frameInfo->stride[i];
        }
        const uint64_t resolvedTimestamp = timestampNs > 0 ? timestampNs : frameInfo->timestamp;
        frame.pixelFormat = static_cast<ccap::PixelFormat>(static_cast<uint32_t>(frameInfo->pixelFormat));
        frame.width = frameInfo->width;
        frame.height = frameInfo->height;
        frame.sizeInBytes = frameInfo->sizeInBytes;
        frame.timestamp = resolvedTimestamp;
        frame.frameIndex = frameInfo->frameIndex;
        frame.orientation = static_cast<ccap::FrameOrientation>(static_cast<uint32_t>(frameInfo->orientation));

        return cppWriter->writeFrame(frame, resolvedTimestamp);
    } catch (...) {
        return false;
    }
}

CcapVideoCodec ccap_video_writer_actual_codec(const CcapVideoWriter* writer) {
    if (!writer) return CCAP_VIDEO_CODEC_H264;
    try {
        auto* cppWriter = reinterpret_cast<const ccap::VideoWriter*>(writer);
        return (cppWriter->actualCodec() == ccap::VideoCodec::HEVC) ? CCAP_VIDEO_CODEC_HEVC : CCAP_VIDEO_CODEC_H264;
    } catch (...) {
        return CCAP_VIDEO_CODEC_H264;
    }
}

} // extern "C"

#else // CCAP_ENABLE_VIDEO_WRITER not defined

extern "C" {

CcapVideoWriter* ccap_video_writer_create(void) { return nullptr; }
void ccap_video_writer_destroy(CcapVideoWriter*) {}
bool ccap_video_writer_open(CcapVideoWriter*, const char*, const CcapWriterConfig*) { return false; }
void ccap_video_writer_close(CcapVideoWriter*) {}
bool ccap_video_writer_is_opened(const CcapVideoWriter*) { return false; }
bool ccap_video_writer_write_frame(CcapVideoWriter*, const CcapVideoFrameInfo*, uint64_t) { return false; }
CcapVideoCodec ccap_video_writer_actual_codec(const CcapVideoWriter*) { return CCAP_VIDEO_CODEC_H264; }

} // extern "C"

#endif // CCAP_ENABLE_VIDEO_WRITER
