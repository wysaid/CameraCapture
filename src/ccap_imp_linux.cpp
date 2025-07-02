/**
 * @file ccap_imp_linux.h
 * @author wysaid (this@wysaid.org)
 * @brief Linux implementation of ccap::Provider class.
 * @date 2025-04
 *
 */

/**
 * @file ccap_imp_linux.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Linux implementation of ccap::Provider class using V4L2.
 * @date 2025-04
 *
 */

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "ccap_imp_linux.h"
#include "ccap_convert_frame.h"
#include "ccap_utils.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

namespace ccap {

// Supported V4L2 pixel formats mapping
const std::vector<ProviderV4L2::V4L2Format> ProviderV4L2::s_supportedV4L2Formats = {
    { V4L2_PIX_FMT_YUYV, PixelFormat::Unknown, "YUYV" },
    { V4L2_PIX_FMT_UYVY, PixelFormat::Unknown, "UYVY" }, 
    { V4L2_PIX_FMT_NV12, PixelFormat::NV12, "NV12" },
    { V4L2_PIX_FMT_YUV420, PixelFormat::I420, "YUV420" },
    { V4L2_PIX_FMT_RGB24, PixelFormat::RGB24, "RGB24" },
    { V4L2_PIX_FMT_BGR24, PixelFormat::BGR24, "BGR24" },
    { V4L2_PIX_FMT_RGB32, PixelFormat::RGBA32, "RGB32" },
    { V4L2_PIX_FMT_BGR32, PixelFormat::BGRA32, "BGR32" },
    { V4L2_PIX_FMT_MJPEG, PixelFormat::Unknown, "MJPEG" },
};

ProviderV4L2::ProviderV4L2() {
    CCAP_LOG_V("ccap: ProviderV4L2 created\n");
}

ProviderV4L2::~ProviderV4L2() {
    close();
    CCAP_LOG_V("ccap: ProviderV4L2 destroyed\n");
}

std::vector<std::string> ProviderV4L2::findDeviceNames() {
    std::vector<std::string> deviceNames;
    
    // Scan /dev/video* devices
    for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
        const std::string filename = entry.path().filename().string();
        if (filename.find("video") == 0) {
            std::string devicePath = entry.path().string();
            if (isVideoDevice(devicePath)) {
                std::string description = getDeviceDescription(devicePath);
                if (!description.empty()) {
                    deviceNames.push_back(std::move(description));
                } else {
                    deviceNames.push_back(devicePath);
                }
                CCAP_LOG_I("ccap: Found video device: %s -> %s\n", devicePath.c_str(), deviceNames.back().c_str());
            }
        }
    }
    
    return deviceNames;
}

bool ProviderV4L2::open(std::string_view deviceName) {
    if (m_isOpened) {
        CCAP_LOG_E("ccap: Device already opened\n");
        return false;
    }
    
    // Find device path
    if (deviceName.empty()) {
        // Use first available device
        auto devices = findDeviceNames();
        if (devices.empty()) {
            CCAP_LOG_E("ccap: No video devices found\n");
            return false;
        }
        m_deviceName = devices[0];
        m_devicePath = "/dev/video0"; // Default to first device
    } else {
        m_deviceName = deviceName;
        // Try to find device path by name
        bool found = false;
        for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
            const std::string filename = entry.path().filename().string();
            if (filename.find("video") == 0) {
                std::string devicePath = entry.path().string();
                std::string description = getDeviceDescription(devicePath);
                if (description == deviceName || devicePath == deviceName) {
                    m_devicePath = devicePath;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            CCAP_LOG_E("ccap: Device not found: %s\n", deviceName.data());
            return false;
        }
    }
    
    // Open device
    m_fd = ::open(m_devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0) {
        CCAP_LOG_E("ccap: Failed to open device %s: %s\n", m_devicePath.c_str(), strerror(errno));
        return false;
    }
    
    if (!setupDevice()) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    
    m_isOpened = true;
    CCAP_LOG_I("ccap: Successfully opened device: %s\n", m_deviceName.c_str());
    return true;
}

bool ProviderV4L2::isOpened() const {
    return m_isOpened && m_fd >= 0;
}

std::optional<DeviceInfo> ProviderV4L2::getDeviceInfo() const {
    if (!isOpened()) {
        return std::nullopt;
    }
    
    DeviceInfo info;
    info.deviceName = m_deviceName;
    
    // Get supported pixel formats
    for (const auto& format : m_supportedFormats) {
        if (format.ccapFormat != PixelFormat::Unknown) {
            info.supportedPixelFormats.push_back(format.ccapFormat);
        }
    }
    
    info.supportedResolutions = m_supportedResolutions;
    
    return info;
}

void ProviderV4L2::close() {
    if (isStarted()) {
        stop();
    }
    
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    
    m_isOpened = false;
    m_isStreaming = false;
    
    CCAP_LOG_V("ccap: Device closed\n");
}

bool ProviderV4L2::start() {
    if (!isOpened()) {
        CCAP_LOG_E("ccap: Device not opened\n");
        return false;
    }
    
    if (m_isStreaming) {
        CCAP_LOG_W("ccap: Already streaming\n");
        return true;
    }
    
    if (!negotiateFormat() || !allocateBuffers() || !startStreaming()) {
        return false;
    }
    
    m_shouldStop = false;
    m_startTime = std::chrono::steady_clock::now();
    m_frameIndex = 0;
    
    // Start capture thread
    m_captureThread = std::make_unique<std::thread>(&ProviderV4L2::captureThread, this);
    
    m_isStreaming = true;
    CCAP_LOG_I("ccap: Streaming started\n");
    return true;
}

void ProviderV4L2::stop() {
    if (!m_isStreaming) {
        return;
    }
    
    m_shouldStop = true;
    
    // Wait for capture thread to finish
    if (m_captureThread && m_captureThread->joinable()) {
        m_captureThread->join();
        m_captureThread.reset();
    }
    
    stopStreaming();
    releaseBuffers();
    
    m_isStreaming = false;
    CCAP_LOG_I("ccap: Streaming stopped\n");
}

bool ProviderV4L2::isStarted() const {
    return m_isStreaming && !m_shouldStop;
}

// Private implementation methods

bool ProviderV4L2::setupDevice() {
    if (!queryCapabilities()) {
        return false;
    }
    
    if (!enumerateFormats()) {
        return false;
    }
    
    return true;
}

bool ProviderV4L2::queryCapabilities() {
    if (ioctl(m_fd, VIDIOC_QUERYCAP, &m_caps) < 0) {
        CCAP_LOG_E("ccap: VIDIOC_QUERYCAP failed: %s\n", strerror(errno));
        return false;
    }
    
    if (!(m_caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        CCAP_LOG_E("ccap: Device does not support video capture\n");
        return false;
    }
    
    if (!(m_caps.capabilities & V4L2_CAP_STREAMING)) {
        CCAP_LOG_E("ccap: Device does not support streaming\n");
        return false;
    }
    
    CCAP_LOG_V("ccap: Device capabilities: %s\n", m_caps.card);
    return true;
}

bool ProviderV4L2::enumerateFormats() {
    m_supportedFormats.clear();
    
    struct v4l2_fmtdesc fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    for (fmt.index = 0; ioctl(m_fd, VIDIOC_ENUM_FMT, &fmt) == 0; fmt.index++) {
        // Find matching format
        for (const auto& supportedFormat : s_supportedV4L2Formats) {
            if (supportedFormat.pixelformat == fmt.pixelformat) {
                m_supportedFormats.push_back(supportedFormat);
                CCAP_LOG_V("ccap: Supported format: %s (%c%c%c%c)\n", 
                          supportedFormat.name,
                          fmt.pixelformat & 0xFF,
                          (fmt.pixelformat >> 8) & 0xFF,
                          (fmt.pixelformat >> 16) & 0xFF,
                          (fmt.pixelformat >> 24) & 0xFF);
                
                // Get supported resolutions for this format
                auto resolutions = getSupportedResolutions(fmt.pixelformat);
                m_supportedResolutions.insert(m_supportedResolutions.end(), 
                                            resolutions.begin(), resolutions.end());
                break;
            }
        }
    }
    
    return !m_supportedFormats.empty();
}

std::vector<DeviceInfo::Resolution> ProviderV4L2::getSupportedResolutions(uint32_t pixelformat) {
    std::vector<DeviceInfo::Resolution> resolutions;
    
    struct v4l2_frmsizeenum framesize = {};
    framesize.pixel_format = pixelformat;
    
    for (framesize.index = 0; ioctl(m_fd, VIDIOC_ENUM_FRAMESIZES, &framesize) == 0; framesize.index++) {
        if (framesize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            resolutions.push_back({framesize.discrete.width, framesize.discrete.height});
        } else if (framesize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            // Add some common resolutions within the range
            uint32_t commonWidths[] = {320, 640, 800, 1024, 1280, 1920, 2560, 3840};
            uint32_t commonHeights[] = {240, 480, 600, 768, 720, 1080, 1440, 2160};
            
            for (size_t i = 0; i < sizeof(commonWidths)/sizeof(commonWidths[0]); i++) {
                if (commonWidths[i] >= framesize.stepwise.min_width && 
                    commonWidths[i] <= framesize.stepwise.max_width &&
                    commonHeights[i] >= framesize.stepwise.min_height &&
                    commonHeights[i] <= framesize.stepwise.max_height) {
                    resolutions.push_back({commonWidths[i], commonHeights[i]});
                }
            }
        }
    }
    
    return resolutions;
}

bool ProviderV4L2::negotiateFormat() {
    // Get current format
    m_currentFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_G_FMT, &m_currentFormat) < 0) {
        CCAP_LOG_E("ccap: VIDIOC_G_FMT failed: %s\n", strerror(errno));
        return false;
    }
    
    // Set desired format if specified
    bool formatChanged = false;
    auto& pix = m_currentFormat.fmt.pix;
    
    if (m_frameProp.width > 0 && m_frameProp.height > 0) {
        if (pix.width != m_frameProp.width || pix.height != m_frameProp.height) {
            pix.width = m_frameProp.width;
            pix.height = m_frameProp.height;
            formatChanged = true;
        }
    }
    
    // Try to set a supported format
    if (m_frameProp.cameraPixelFormat != PixelFormat::Unknown) {
        uint32_t v4l2Format = ccapFormatToV4l2Format(m_frameProp.cameraPixelFormat);
        if (v4l2Format != 0 && pix.pixelformat != v4l2Format) {
            pix.pixelformat = v4l2Format;
            formatChanged = true;
        }
    }
    
    // Apply format if changed
    if (formatChanged) {
        if (ioctl(m_fd, VIDIOC_S_FMT, &m_currentFormat) < 0) {
            CCAP_LOG_W("ccap: VIDIOC_S_FMT failed, using current format: %s\n", strerror(errno));
        }
        
        // Get the actual format set by the driver
        if (ioctl(m_fd, VIDIOC_G_FMT, &m_currentFormat) < 0) {
            CCAP_LOG_E("ccap: VIDIOC_G_FMT failed after set: %s\n", strerror(errno));
            return false;
        }
    }
    
    // Update frame properties
    m_frameProp.width = pix.width;
    m_frameProp.height = pix.height;
    m_frameProp.cameraPixelFormat = v4l2FormatToCcapFormat(pix.pixelformat);
    
    CCAP_LOG_I("ccap: Format negotiated: %dx%d, format=%s\n", 
               pix.width, pix.height, getFormatName(pix.pixelformat));
    
    return true;
}

bool ProviderV4L2::allocateBuffers() {
    struct v4l2_requestbuffers req = {};
    req.count = kBufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
        CCAP_LOG_E("ccap: VIDIOC_REQBUFS failed: %s\n", strerror(errno));
        return false;
    }
    
    if (req.count < 2) {
        CCAP_LOG_E("ccap: Insufficient buffer memory\n");
        return false;
    }
    
    m_buffers.resize(req.count);
    
    for (size_t i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            CCAP_LOG_E("ccap: VIDIOC_QUERYBUF failed: %s\n", strerror(errno));
            return false;
        }
        
        m_buffers[i].length = buf.length;
        m_buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        m_buffers[i].index = i;
        
        if (m_buffers[i].start == MAP_FAILED) {
            CCAP_LOG_E("ccap: mmap failed: %s\n", strerror(errno));
            return false;
        }
    }
    
    CCAP_LOG_V("ccap: Allocated %zu buffers\n", m_buffers.size());
    return true;
}

void ProviderV4L2::releaseBuffers() {
    for (auto& buffer : m_buffers) {
        if (buffer.start != nullptr && buffer.start != MAP_FAILED) {
            munmap(buffer.start, buffer.length);
        }
    }
    m_buffers.clear();
}

bool ProviderV4L2::startStreaming() {
    // Queue all buffers
    for (size_t i = 0; i < m_buffers.size(); i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            CCAP_LOG_E("ccap: VIDIOC_QBUF failed: %s\n", strerror(errno));
            return false;
        }
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        CCAP_LOG_E("ccap: VIDIOC_STREAMON failed: %s\n", strerror(errno));
        return false;
    }
    
    return true;
}

void ProviderV4L2::stopStreaming() {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_STREAMOFF, &type) < 0) {
        CCAP_LOG_E("ccap: VIDIOC_STREAMOFF failed: %s\n", strerror(errno));
    }
}

void ProviderV4L2::captureThread() {
    CCAP_LOG_V("ccap: Capture thread started\n");
    
    while (!m_shouldStop) {
        if (!readFrame()) {
            // Error or timeout, continue
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    CCAP_LOG_V("ccap: Capture thread finished\n");
}

bool ProviderV4L2::readFrame() {
    // Use poll to wait for data
    struct pollfd fds[1];
    fds[0].fd = m_fd;
    fds[0].events = POLLIN;
    
    int ret = poll(fds, 1, 100); // 100ms timeout
    if (ret < 0) {
        if (errno != EINTR) {
            CCAP_LOG_E("ccap: poll failed: %s\n", strerror(errno));
        }
        return false;
    } else if (ret == 0) {
        // Timeout
        return false;
    }
    
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno != EAGAIN) {
            CCAP_LOG_E("ccap: VIDIOC_DQBUF failed: %s\n", strerror(errno));
        }
        return false;
    }
    
    // Process frame
    if (tooManyNewFrames()) {
        bool hasCallback = (m_callback && *m_callback);
        if (!hasCallback) {
            CCAP_LOG_I("ccap: Frame dropped to avoid memory leak\n");
        }
    } else {
        auto frame = getFreeFrame();
        if (frame) {
            // Fill frame data
            frame->width = m_frameProp.width;
            frame->height = m_frameProp.height;
            frame->pixelFormat = m_frameProp.cameraPixelFormat;
            frame->timestamp = (std::chrono::steady_clock::now() - m_startTime).count();
            frame->frameIndex = m_frameIndex++;
            frame->orientation = FrameOrientation::TopToBottom;
            
            // Copy frame data
            if (!frame->allocator) {
                frame->allocator = m_allocatorFactory ? m_allocatorFactory() : std::make_shared<DefaultAllocator>();
            }
            frame->allocator->resize(buf.bytesused);
            
            frame->data[0] = frame->allocator->data();
            frame->stride[0] = m_currentFormat.fmt.pix.bytesperline;
            frame->sizeInBytes = buf.bytesused;
            
            std::memcpy(frame->data[0], m_buffers[buf.index].start, buf.bytesused);
            
            // Handle pixel format conversion if needed
            if (m_frameProp.outputPixelFormat != PixelFormat::Unknown && 
                m_frameProp.outputPixelFormat != frame->pixelFormat) {
                inplaceConvertFrame(frame.get(), m_frameProp.outputPixelFormat, false);
            }
            
            newFrameAvailable(std::move(frame));
        }
    }
    
    // Requeue buffer
    if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
        CCAP_LOG_E("ccap: VIDIOC_QBUF failed: %s\n", strerror(errno));
        return false;
    }
    
    return true;
}

// Utility methods

PixelFormat ProviderV4L2::v4l2FormatToCcapFormat(uint32_t v4l2Format) {
    for (const auto& format : s_supportedV4L2Formats) {
        if (format.pixelformat == v4l2Format) {
            return format.ccapFormat;
        }
    }
    return PixelFormat::Unknown;
}

uint32_t ProviderV4L2::ccapFormatToV4l2Format(PixelFormat ccapFormat) {
    for (const auto& format : s_supportedV4L2Formats) {
        if (format.ccapFormat == ccapFormat) {
            return format.pixelformat;
        }
    }
    return 0;
}

const char* ProviderV4L2::getFormatName(uint32_t pixelformat) {
    for (const auto& format : s_supportedV4L2Formats) {
        if (format.pixelformat == pixelformat) {
            return format.name;
        }
    }
    return "Unknown";
}

bool ProviderV4L2::isVideoDevice(const std::string& devicePath) {
    int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }
    
    struct v4l2_capability cap;
    bool isVideo = (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) && 
                   (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE);
    
    ::close(fd);
    return isVideo;
}

std::string ProviderV4L2::getDeviceDescription(const std::string& devicePath) {
    int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return "";
    }
    
    struct v4l2_capability cap;
    std::string description;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        description = reinterpret_cast<const char*>(cap.card);
    }
    
    ::close(fd);
    return description;
}

// Factory function
ProviderImp* createProviderV4L2() {
    return new ProviderV4L2();
}

} // namespace ccap

#endif // Linux check