/**
 * @file ccap_writer_imp.h
 * @brief Internal header for VideoWriter platform implementations.
 */

#pragma once

#ifndef CCAP_WRITER_IMP_H
#define CCAP_WRITER_IMP_H

#include "ccap_writer.h"
#include "ccap_def.h"

#include <string_view>

namespace ccap {

struct VideoWriter::Impl {
    Impl() : m_actualCodec(VideoCodec::H264) {}
    virtual ~Impl() = default;

    virtual bool open(std::string_view filePath, const WriterConfig& config) = 0;
    virtual void close() = 0;
    virtual bool isOpened() const = 0;
    virtual bool writeFrame(const VideoFrame& frame, uint64_t timestampNs) = 0;

    VideoCodec m_actualCodec;
    WriterConfig m_config;
};

} // namespace ccap

#endif // CCAP_WRITER_IMP_H
