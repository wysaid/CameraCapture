/**
 * @file ccap_writer.mm
 * @author wysaid (this@wysaid.org)
 * @brief Video writer platform dispatch layer.
 * @date 2025-05
 */

#include "ccap_writer.h"
#include "ccap_writer_imp.h"
#include "ccap_imp.h"

#include "ccap_convert.h"
#include "ccap_utils.h"

#include <cstring>
#include <memory>
#include <vector>

#ifdef CCAP_ENABLE_VIDEO_WRITER

#if __APPLE__
#include "ccap_writer_apple.mm"
#elif defined(_WIN32) || defined(_MSC_VER)
#include "ccap_writer_windows.cpp"
#endif

namespace ccap {

// ---- Platform dispatch ----

static VideoWriter::Impl* impl(void* p) { return reinterpret_cast<VideoWriter::Impl*>(p); }
static const VideoWriter::Impl* impl(const void* p) { return reinterpret_cast<const VideoWriter::Impl*>(p); }

VideoWriter::VideoWriter() : m_impl(nullptr) {
#if __APPLE__
    m_impl = new WriterApple();
#elif defined(_WIN32) || defined(_MSC_VER)
    m_impl = new WriterWindows();
#endif
}

VideoWriter::~VideoWriter() {
    delete impl(m_impl);
}

VideoWriter::VideoWriter(VideoWriter&& other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

VideoWriter& VideoWriter::operator=(VideoWriter&& other) noexcept {
    if (this != &other) {
        delete impl(m_impl);
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

bool VideoWriter::open(std::string_view filePath, const WriterConfig& config) {
    if (!m_impl) {
        reportError(ErrorCode::WriterNotOpened, "VideoWriter not available on this platform");
        return false;
    }
    return impl(m_impl)->open(filePath, config);
}

void VideoWriter::close() {
    if (m_impl) impl(m_impl)->close();
}

bool VideoWriter::isOpened() const {
    return m_impl && impl(m_impl)->isOpened();
}

bool VideoWriter::writeFrame(const VideoFrame& frame, uint64_t timestampNs) {
    if (!m_impl) return false;
    return impl(m_impl)->writeFrame(frame, timestampNs);
}

VideoCodec VideoWriter::actualCodec() const {
    return m_impl ? impl(m_impl)->m_actualCodec : VideoCodec::H264;
}

uint32_t VideoWriter::width() const {
    return m_impl ? impl(m_impl)->m_config.width : 0;
}

uint32_t VideoWriter::height() const {
    return m_impl ? impl(m_impl)->m_config.height : 0;
}

double VideoWriter::frameRate() const {
    return m_impl ? impl(m_impl)->m_config.frameRate : 0.0;
}

} // namespace ccap

#else // CCAP_ENABLE_VIDEO_WRITER not defined

namespace ccap {

VideoWriter::VideoWriter() : m_impl(nullptr) {}
VideoWriter::~VideoWriter() = default;
VideoWriter::VideoWriter(VideoWriter&&) noexcept = default;
VideoWriter& VideoWriter::operator=(VideoWriter&&) noexcept = default;

bool VideoWriter::open(std::string_view, const WriterConfig&) {
    reportError(ErrorCode::WriterNotOpened, "Video writer not enabled in this build");
    return false;
}
void VideoWriter::close() {}
bool VideoWriter::isOpened() const { return false; }
bool VideoWriter::writeFrame(const VideoFrame&, uint64_t) { return false; }
VideoCodec VideoWriter::actualCodec() const { return VideoCodec::H264; }
uint32_t VideoWriter::width() const { return 0; }
uint32_t VideoWriter::height() const { return 0; }
double VideoWriter::frameRate() const { return 0.0; }

} // namespace ccap

#endif // CCAP_ENABLE_VIDEO_WRITER
