/**
 * @file test_video_writer.cpp
 * @brief Tests for video writer functionality
 *
 * Tests verify basic writer lifecycle, frame writing, and output validation.
 * Output files are written to a temporary directory and cleaned up after.
 */

#include <ccap.h>
#include <ccap_convert.h>
#include <ccap_writer.h>
#include <ccap_writer_c.h>
#include "ccap_writer_imp.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;

namespace {

struct MeanBgr {
    double b = 0.0;
    double g = 0.0;
    double r = 0.0;
};

std::vector<uint8_t> createQuadrantBgrFrame(int w, int h, int stride) {
    std::vector<uint8_t> data(static_cast<size_t>(stride) * h, 0);
    for (int y = 0; y < h; ++y) {
        uint8_t* row = data.data() + static_cast<size_t>(y) * stride;
        for (int x = 0; x < w; ++x) {
            uint8_t* pixel = row + x * 3;
            const bool isTop = y < h / 2;
            const bool isLeft = x < w / 2;
            if (isTop && isLeft) {
                pixel[0] = 240; // B
                pixel[1] = 32;  // G
                pixel[2] = 32;  // R
            } else if (isTop) {
                pixel[0] = 32;
                pixel[1] = 240;
                pixel[2] = 32;
            } else if (isLeft) {
                pixel[0] = 32;
                pixel[1] = 32;
                pixel[2] = 240;
            } else {
                pixel[0] = 230;
                pixel[1] = 230;
                pixel[2] = 230;
            }
        }
    }
    return data;
}

std::vector<uint8_t> flipRows(const std::vector<uint8_t>& src, int stride, int h) {
    std::vector<uint8_t> dst(src.size(), 0);
    for (int y = 0; y < h; ++y) {
        std::memcpy(dst.data() + static_cast<size_t>(y) * stride,
                    src.data() + static_cast<size_t>(h - 1 - y) * stride,
                    static_cast<size_t>(stride));
    }
    return dst;
}

MeanBgr calculateLogicalRegionMean(const ccap::VideoFrame& frame, int x0, int y0, int regionWidth, int regionHeight) {
    const bool hasAlpha = ccap::pixelFormatInclude(frame.pixelFormat, ccap::kPixelFormatAlphaColorBit);
    const bool isBgrOrder = ccap::pixelFormatInclude(frame.pixelFormat, ccap::kPixelFormatBGRBit);
    const int channels = hasAlpha ? 4 : 3;
    MeanBgr mean{};
    const double samples = static_cast<double>(regionWidth * regionHeight);

    for (int y = y0; y < y0 + regionHeight; ++y) {
        const int logicalRow = frame.orientation == ccap::FrameOrientation::TopToBottom ? y : static_cast<int>(frame.height) - 1 - y;
        const uint8_t* row = frame.data[0] + static_cast<size_t>(logicalRow) * frame.stride[0];
        for (int x = x0; x < x0 + regionWidth; ++x) {
            const uint8_t* pixel = row + x * channels;
            if (isBgrOrder) {
                mean.b += pixel[0];
                mean.g += pixel[1];
                mean.r += pixel[2];
            } else {
                mean.r += pixel[0];
                mean.g += pixel[1];
                mean.b += pixel[2];
            }
        }
    }

    mean.b /= samples;
    mean.g /= samples;
    mean.r /= samples;
    return mean;
}

std::string meanToString(const MeanBgr& mean) {
    std::ostringstream stream;
    stream << "(B=" << mean.b << ", G=" << mean.g << ", R=" << mean.r << ")";
    return stream.str();
}

void expectUprightQuadrantPattern(const ccap::VideoFrame& frame) {
    ASSERT_TRUE(ccap::pixelFormatInclude(frame.pixelFormat, ccap::kPixelFormatRGBColorBit))
        << "Expected RGB output, got pixel format=" << static_cast<uint32_t>(frame.pixelFormat);

    const int width = static_cast<int>(frame.width);
    const int height = static_cast<int>(frame.height);
    const int sampleWidth = std::max(8, width / 4);
    const int sampleHeight = std::max(8, height / 4);

    const MeanBgr topLeft = calculateLogicalRegionMean(frame, width / 8, height / 8, sampleWidth, sampleHeight);
    const MeanBgr topRight = calculateLogicalRegionMean(frame, width / 2 + width / 8, height / 8, sampleWidth, sampleHeight);
    const MeanBgr bottomLeft = calculateLogicalRegionMean(frame, width / 8, height / 2 + height / 8, sampleWidth, sampleHeight);
    const MeanBgr bottomRight = calculateLogicalRegionMean(frame, width / 2 + width / 8, height / 2 + height / 8, sampleWidth, sampleHeight);

    EXPECT_GT(topLeft.b, topLeft.g + 40.0) << meanToString(topLeft);
    EXPECT_GT(topLeft.b, topLeft.r + 40.0) << meanToString(topLeft);

    EXPECT_GT(topRight.g, topRight.b + 40.0) << meanToString(topRight);
    EXPECT_GT(topRight.g, topRight.r + 40.0) << meanToString(topRight);

    EXPECT_GT(bottomLeft.r, bottomLeft.b + 40.0) << meanToString(bottomLeft);
    EXPECT_GT(bottomLeft.r, bottomLeft.g + 40.0) << meanToString(bottomLeft);

    const double whiteMin = std::min({ bottomRight.b, bottomRight.g, bottomRight.r });
    const double whiteMax = std::max({ bottomRight.b, bottomRight.g, bottomRight.r });
    EXPECT_GT(whiteMin, 170.0) << meanToString(bottomRight);
    EXPECT_LT(whiteMax - whiteMin, 50.0) << meanToString(bottomRight);
}

void initializeBgrFrame(ccap::VideoFrame& frame, uint8_t* data, int w, int h, int stride, ccap::FrameOrientation orientation) {
    frame.data[0] = data;
    frame.data[1] = nullptr;
    frame.data[2] = nullptr;
    frame.stride[0] = static_cast<uint32_t>(stride);
    frame.stride[1] = 0;
    frame.stride[2] = 0;
    frame.pixelFormat = ccap::PixelFormat::BGR24;
    frame.width = static_cast<uint32_t>(w);
    frame.height = static_cast<uint32_t>(h);
    frame.sizeInBytes = static_cast<uint32_t>(stride * h);
    frame.timestamp = 0;
    frame.frameIndex = 0;
    frame.orientation = orientation;
}

} // namespace

// Helper to check if video writer is supported on this platform
bool isVideoWriterSupported() {
#if (defined(__APPLE__) || defined(_WIN32)) && defined(CCAP_ENABLE_VIDEO_WRITER)
    return true;
#else
    return false;
#endif
}

// Create a synthetic BGR24 frame with random noise
std::vector<uint8_t> createBgrFrame(int w, int h, int stride) {
    std::vector<uint8_t> data(static_cast<size_t>(stride) * h);
    std::mt19937 gen(42); // fixed seed for reproducibility
    std::uniform_int_distribution<> dist(0, 255);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = static_cast<uint8_t>(dist(gen));
    }
    return data;
}

class VideoWriterTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        if (!isVideoWriterSupported()) {
            GTEST_SKIP() << "Video writer not supported on this platform/build";
        }
    }

    void TearDown() override {
        std::error_code ec;
        for (const auto& path : m_outputPaths) {
            fs::remove(path, ec);
            ec.clear();
        }
    }

    fs::path makeTestOutputPath(std::string_view name, std::string_view extension = ".mp4") {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();

        std::string fileName = "ccap_writer_test_";
        if (info) {
            fileName += info->test_suite_name();
            fileName += "_";
            fileName += info->name();
            fileName += "_";
        }
        fileName += std::string(name);
        fileName += "_";
        fileName += std::to_string(m_outputPaths.size());
        fileName += std::string(extension);

        fs::path outputPath = fs::temp_directory_path() / fileName;
        m_outputPaths.push_back(outputPath);
        return outputPath;
    }

private:
    std::vector<fs::path> m_outputPaths;
};

// Test fixture for video writer tests
class VideoWriterTest : public VideoWriterTestBase {};

// Test fixture for C API tests
class VideoWriterCTest : public VideoWriterTestBase {};

// ---- C++ API Tests ----

TEST_F(VideoWriterTest, ConstructAndDestroy) {
    ccap::VideoWriter writer;
    EXPECT_FALSE(writer.isOpened());
}

TEST_F(VideoWriterTest, MoveConstructor) {
    ccap::VideoWriter writer1;
    ccap::VideoWriter writer2(std::move(writer1));
    EXPECT_FALSE(writer2.isOpened());
}

TEST_F(VideoWriterTest, MoveAssignment) {
    ccap::VideoWriter writer1;
    ccap::VideoWriter writer2;
    writer2 = std::move(writer1);
    EXPECT_FALSE(writer2.isOpened());
}

TEST_F(VideoWriterTest, OpenInvalidPath) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 640;
    config.height = 480;
    config.frameRate = 30.0;
    config.bitRate = 5000000;

    // Invalid path should fail
    bool result = writer.open("/nonexistent/deeply/nested/path/output.mp4", config);
    EXPECT_FALSE(result);
    EXPECT_FALSE(writer.isOpened());
}

TEST_F(VideoWriterTest, OpenZeroDimensions) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 0;
    config.height = 0;
    config.frameRate = 30.0;
    config.bitRate = 5000000;

    bool result = writer.open(makeTestOutputPath("zero_dim").string(), config);
    EXPECT_FALSE(result);
}

TEST_F(VideoWriterTest, OpenAndClose) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 640;
    config.height = 480;
    config.frameRate = 30.0;
    config.bitRate = 5000000;

    fs::path outputPath = makeTestOutputPath("open_close");
    bool result = writer.open(outputPath.string(), config);
    EXPECT_TRUE(result);
    EXPECT_TRUE(writer.isOpened());

    writer.close();
    EXPECT_FALSE(writer.isOpened());

    // Verify output file was created (may be empty since no frames were written)
    EXPECT_TRUE(fs::exists(outputPath));
}

TEST_F(VideoWriterTest, WriteFramesAndValidateFile) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;

    fs::path outputPath = makeTestOutputPath("write_frames");
    ASSERT_TRUE(writer.open(outputPath.string(), config));

    // Create and write 30 frames (1 second at 30fps)
    int w = 320, h = 240;
    int stride = w * 3; // BGR24
    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);

    ccap::VideoFrame frame{};
    frame.data[0] = frameData.data();
    frame.stride[0] = static_cast<uint32_t>(stride);
    frame.data[1] = nullptr;
    frame.stride[1] = 0;
    frame.data[2] = nullptr;
    frame.stride[2] = 0;
    frame.pixelFormat = ccap::PixelFormat::BGR24;
    frame.width = static_cast<uint32_t>(w);
    frame.height = static_cast<uint32_t>(h);
    frame.sizeInBytes = static_cast<uint32_t>(stride * h);
    frame.timestamp = 0;
    frame.frameIndex = 0;
    frame.orientation = ccap::FrameOrientation::Default;

    for (int i = 0; i < 30; i++) {
        frame.timestamp = static_cast<uint64_t>(i) * 33333333; // ~30fps in ns
        frame.frameIndex = static_cast<uint32_t>(i);
        bool writeResult = writer.writeFrame(frame, frame.timestamp);
        EXPECT_TRUE(writeResult);
    }

    writer.close();

    // Verify file exists and has reasonable size
    EXPECT_TRUE(fs::exists(outputPath));
    uint64_t fileSize = fs::file_size(outputPath);
    // 30 frames at 320x240 with 2Mbps bitrate should produce at least a few KB
    EXPECT_GT(fileSize, 1000);
    EXPECT_LT(fileSize, 50 * 1024 * 1024); // less than 50MB

    // Verify file can be opened for playback
#ifdef CCAP_ENABLE_FILE_PLAYBACK
    ccap::Provider provider;
    EXPECT_TRUE(provider.open(outputPath.string()));
    auto framePtr = provider.grab(5000);
    EXPECT_NE(framePtr, nullptr);
    if (framePtr) {
        EXPECT_EQ(framePtr->width, 320);
        EXPECT_EQ(framePtr->height, 240);
    }
    provider.close();
#endif
}

TEST_F(VideoWriterTest, SharedNv12ConversionRespectsBottomToTopOrientation) {
    constexpr int w = 128;
    constexpr int h = 96;
    constexpr int stride = w * 3;

    std::vector<uint8_t> topDown = createQuadrantBgrFrame(w, h, stride);
    std::vector<uint8_t> bottomUp = flipRows(topDown, stride, h);

    ccap::VideoFrame topFrame;
    initializeBgrFrame(topFrame, topDown.data(), w, h, stride, ccap::FrameOrientation::TopToBottom);

    ccap::VideoFrame bottomFrame;
    initializeBgrFrame(bottomFrame, bottomUp.data(), w, h, stride, ccap::FrameOrientation::BottomToTop);

    std::vector<uint8_t> topY;
    std::vector<uint8_t> topUv;
    uint32_t topYStride = 0;
    uint32_t topUvStride = 0;
    ASSERT_TRUE(ccap::convertFrameToNv12(topFrame, topY, topUv, topYStride, topUvStride));

    std::vector<uint8_t> bottomY;
    std::vector<uint8_t> bottomUv;
    uint32_t bottomYStride = 0;
    uint32_t bottomUvStride = 0;
    ASSERT_TRUE(ccap::convertFrameToNv12(bottomFrame, bottomY, bottomUv, bottomYStride, bottomUvStride));

    EXPECT_EQ(bottomYStride, topYStride);
    EXPECT_EQ(bottomUvStride, topUvStride);
    EXPECT_EQ(bottomY, topY);
    EXPECT_EQ(bottomUv, topUv);
}

TEST_F(VideoWriterTest, SharedNv12ConversionRejectsOddDimensions) {
    constexpr int w = 127;
    constexpr int h = 95;
    constexpr int stride = w * 3;

    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);
    ccap::VideoFrame frame{};
    initializeBgrFrame(frame, frameData.data(), w, h, stride, ccap::FrameOrientation::TopToBottom);

    std::vector<uint8_t> yBuf;
    std::vector<uint8_t> uvBuf;
    uint32_t yStride = 0;
    uint32_t uvStride = 0;
    EXPECT_FALSE(ccap::convertFrameToNv12(frame, yBuf, uvBuf, yStride, uvStride));
}

TEST_F(VideoWriterTest, BottomToTopFramesRoundTripUpright) {
#ifdef CCAP_ENABLE_FILE_PLAYBACK
    constexpr int w = 128;
    constexpr int h = 96;
    constexpr int stride = w * 3;
    std::vector<uint8_t> topDown = createQuadrantBgrFrame(w, h, stride);
    std::vector<uint8_t> bottomUp = flipRows(topDown, stride, h);

    ccap::WriterConfig config;
    config.width = w;
    config.height = h;
    config.frameRate = 30.0;
    config.bitRate = 8'000'000;

    fs::path outputPath = makeTestOutputPath("bottom_to_top_cpp");
    ccap::VideoWriter writer;
    ASSERT_TRUE(writer.open(outputPath.string(), config));

    ccap::VideoFrame frame;
    initializeBgrFrame(frame, bottomUp.data(), w, h, stride, ccap::FrameOrientation::BottomToTop);

    for (int index = 0; index < 12; ++index) {
        frame.frameIndex = static_cast<uint32_t>(index);
        frame.timestamp = static_cast<uint64_t>(index) * 33'333'333ULL;
        ASSERT_TRUE(writer.writeFrame(frame, frame.timestamp));
    }

    writer.close();

    ccap::Provider reader;
    reader.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGR24);
    reader.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::TopToBottom);
    ASSERT_TRUE(reader.open(outputPath.string()));

    auto decoded = reader.grab(5000);
    ASSERT_NE(decoded, nullptr);
    if (decoded) {
        expectUprightQuadrantPattern(*decoded);
    }

    reader.close();
#else
    GTEST_SKIP() << "File playback not enabled, cannot verify writer output orientation";
#endif
}

TEST_F(VideoWriterTest, WriteFramesWithMovContainer) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;
    config.container = ccap::VideoFormat::MOV;

    fs::path outputPath = makeTestOutputPath("mov_container", ".mov");

    ASSERT_TRUE(writer.open(outputPath.string(), config));

    int w = 320, h = 240;
    int stride = w * 3;
    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);

    ccap::VideoFrame frame{};
    frame.data[0] = frameData.data();
    frame.stride[0] = static_cast<uint32_t>(stride);
    frame.pixelFormat = ccap::PixelFormat::BGR24;
    frame.width = static_cast<uint32_t>(w);
    frame.height = static_cast<uint32_t>(h);
    frame.sizeInBytes = static_cast<uint32_t>(stride * h);
    frame.orientation = ccap::FrameOrientation::Default;

    // Write 10 frames
    for (int i = 0; i < 10; i++) {
        frame.frameIndex = static_cast<uint32_t>(i);
        EXPECT_TRUE(writer.writeFrame(frame));
    }

    writer.close();
    EXPECT_TRUE(fs::exists(outputPath));
    EXPECT_GT(fs::file_size(outputPath), 0);
}

TEST_F(VideoWriterTest, CodecFallback) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;
    config.codec = ccap::VideoCodec::HEVC; // Request HEVC

    fs::path outputPath = makeTestOutputPath("codec_fallback");
    ASSERT_TRUE(writer.open(outputPath.string(), config));

    // Actual codec may differ from requested due to fallback
    ccap::VideoCodec actual = writer.actualCodec();
    EXPECT_TRUE(actual == ccap::VideoCodec::HEVC || actual == ccap::VideoCodec::H264);

    writer.close();
}

TEST_F(VideoWriterTest, WriteAfterCloseFails) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;

    fs::path outputPath = makeTestOutputPath("write_after_close");
    ASSERT_TRUE(writer.open(outputPath.string(), config));
    writer.close();

    // Writing after close should fail
    int w = 320, h = 240;
    int stride = w * 3;
    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);
    ccap::VideoFrame frame{};
    initializeBgrFrame(frame, frameData.data(), w, h, stride, ccap::FrameOrientation::TopToBottom);

    EXPECT_FALSE(writer.writeFrame(frame));
}

TEST_F(VideoWriterTest, ReopenWhileOpenedFails) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;

    fs::path firstOutput = makeTestOutputPath("reopen_first");
    fs::path secondOutput = makeTestOutputPath("reopen_second");

    ASSERT_TRUE(writer.open(firstOutput.string(), config));
    EXPECT_TRUE(writer.isOpened());
    EXPECT_FALSE(writer.open(secondOutput.string(), config));
    EXPECT_TRUE(writer.isOpened());
    EXPECT_EQ(writer.width(), 320u);
    EXPECT_EQ(writer.height(), 240u);

    writer.close();
}

TEST_F(VideoWriterTest, GetPropertiesAfterOpen) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 640;
    config.height = 480;
    config.frameRate = 25.0;
    config.bitRate = 3000000;

    fs::path outputPath = makeTestOutputPath("properties");
    ASSERT_TRUE(writer.open(outputPath.string(), config));

    EXPECT_EQ(writer.width(), 640);
    EXPECT_EQ(writer.height(), 480);
    EXPECT_DOUBLE_EQ(writer.frameRate(), 25.0);

    writer.close();

    // After close, properties should return 0
    EXPECT_EQ(writer.width(), 0);
    EXPECT_EQ(writer.height(), 0);
    EXPECT_DOUBLE_EQ(writer.frameRate(), 0.0);
}

// ---- C API Tests ----

TEST_F(VideoWriterCTest, CreateAndDestroy) {
    CcapVideoWriter* writer = ccap_video_writer_create();
    EXPECT_NE(writer, nullptr);
    if (writer) {
        EXPECT_FALSE(ccap_video_writer_is_opened(writer));
        ccap_video_writer_destroy(writer);
    }
}

TEST_F(VideoWriterCTest, NullHandleSafety) {
    // All C functions should handle null gracefully
    ccap_video_writer_destroy(nullptr);
    EXPECT_FALSE(ccap_video_writer_is_opened(nullptr));
    EXPECT_FALSE(ccap_video_writer_open(nullptr, "test.mp4", nullptr));
    ccap_video_writer_close(nullptr);
    EXPECT_FALSE(ccap_video_writer_write_frame(nullptr, nullptr, 0));
    EXPECT_EQ(ccap_video_writer_actual_codec(nullptr), CCAP_VIDEO_CODEC_H264);
}

TEST_F(VideoWriterCTest, OpenAndWriteFrames) {
    CcapVideoWriter* writer = ccap_video_writer_create();
    ASSERT_NE(writer, nullptr);

    CcapWriterConfig config = CCAP_WRITER_CONFIG_INIT;
    config.width = 320;
    config.height = 240;
    config.bitRate = 2000000;

    fs::path outputPath = makeTestOutputPath("c_api");
    ASSERT_TRUE(ccap_video_writer_open(writer, outputPath.string().c_str(), &config));
    EXPECT_TRUE(ccap_video_writer_is_opened(writer));

    // Create BGR frame
    int w = 320, h = 240;
    int stride = w * 3;
    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);

    CcapVideoFrameInfo frameInfo{};
    frameInfo.data[0] = frameData.data();
    frameInfo.stride[0] = static_cast<uint32_t>(stride);
    frameInfo.pixelFormat = CCAP_PIXEL_FORMAT_BGR24;
    frameInfo.width = static_cast<uint32_t>(w);
    frameInfo.height = static_cast<uint32_t>(h);
    frameInfo.sizeInBytes = static_cast<uint32_t>(stride * h);
    frameInfo.orientation = CCAP_FRAME_ORIENTATION_TOP_TO_BOTTOM;

    // Write 15 frames
    for (int i = 0; i < 15; i++) {
        frameInfo.frameIndex = static_cast<uint32_t>(i);
        EXPECT_TRUE(ccap_video_writer_write_frame(writer, &frameInfo, 0));
    }

    // Check actual codec
    CcapVideoCodec actualCodec = ccap_video_writer_actual_codec(writer);
    EXPECT_TRUE(actualCodec == CCAP_VIDEO_CODEC_HEVC || actualCodec == CCAP_VIDEO_CODEC_H264);

    ccap_video_writer_close(writer);
    EXPECT_FALSE(ccap_video_writer_is_opened(writer));

    ccap_video_writer_destroy(writer);

    // Verify file
    EXPECT_TRUE(fs::exists(outputPath));
    EXPECT_GT(fs::file_size(outputPath), 0);
}

TEST_F(VideoWriterCTest, BottomToTopFramesRoundTripUpright) {
#ifdef CCAP_ENABLE_FILE_PLAYBACK
    constexpr int w = 128;
    constexpr int h = 96;
    constexpr int stride = w * 3;
    std::vector<uint8_t> topDown = createQuadrantBgrFrame(w, h, stride);
    std::vector<uint8_t> bottomUp = flipRows(topDown, stride, h);

    CcapVideoWriter* writer = ccap_video_writer_create();
    ASSERT_NE(writer, nullptr);

    CcapWriterConfig config = CCAP_WRITER_CONFIG_INIT;
    config.codec = CCAP_VIDEO_CODEC_H264;
    config.width = static_cast<uint32_t>(w);
    config.height = static_cast<uint32_t>(h);
    config.bitRate = 8'000'000;

    fs::path outputPath = makeTestOutputPath("bottom_to_top_c_api");
    ASSERT_TRUE(ccap_video_writer_open(writer, outputPath.string().c_str(), &config));

    CcapVideoFrameInfo frameInfo{};
    frameInfo.data[0] = bottomUp.data();
    frameInfo.stride[0] = static_cast<uint32_t>(stride);
    frameInfo.pixelFormat = CCAP_PIXEL_FORMAT_BGR24;
    frameInfo.width = static_cast<uint32_t>(w);
    frameInfo.height = static_cast<uint32_t>(h);
    frameInfo.sizeInBytes = static_cast<uint32_t>(stride * h);
    frameInfo.orientation = CCAP_FRAME_ORIENTATION_BOTTOM_TO_TOP;

    for (int index = 0; index < 12; ++index) {
        frameInfo.frameIndex = static_cast<uint32_t>(index);
        const uint64_t timestamp = static_cast<uint64_t>(index) * 33'333'333ULL;
        ASSERT_TRUE(ccap_video_writer_write_frame(writer, &frameInfo, timestamp));
    }

    ccap_video_writer_close(writer);
    ccap_video_writer_destroy(writer);

    ccap::Provider reader;
    reader.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGR24);
    reader.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::TopToBottom);
    ASSERT_TRUE(reader.open(outputPath.string()));

    auto decoded = reader.grab(5000);
    ASSERT_NE(decoded, nullptr);
    if (decoded) {
        expectUprightQuadrantPattern(*decoded);
    }

    reader.close();
#else
    GTEST_SKIP() << "File playback not enabled, cannot verify writer output orientation";
#endif
}

// Helper: locate the built-in test video by walking up from CWD to find the project root
static fs::path findTestVideo() {
    fs::path projectRoot = fs::current_path();
    while (true) {
        if (fs::exists(projectRoot / "CMakeLists.txt") && fs::exists(projectRoot / "tests")) {
            break;
        }

        const fs::path parent = projectRoot.parent_path();
        if (parent.empty() || parent == projectRoot) {
            break;
        }
        projectRoot = parent;
    }
    return projectRoot / "tests" / "test-data" / "test.mp4";
}

// ---- Transcode Test: verify timestamps survive a read→write→read round-trip ----

TEST_F(VideoWriterTest, TranscodePreservesDuration) {
#ifdef CCAP_ENABLE_FILE_PLAYBACK
    fs::path inputPath = findTestVideo();
    if (!fs::exists(inputPath)) {
        GTEST_SKIP() << "test.mp4 not found at " << inputPath;
    }

    // 1. Read source video metadata
    ccap::Provider reader;
    ASSERT_TRUE(reader.open(inputPath.string())) << "Failed to open source video";

    double srcDuration = reader.get(ccap::PropertyName::Duration);
    int srcWidth = static_cast<int>(reader.get(ccap::PropertyName::Width));
    int srcHeight = static_cast<int>(reader.get(ccap::PropertyName::Height));
    double srcFps = reader.get(ccap::PropertyName::FrameRate);
    ASSERT_GT(srcDuration, 0.0) << "Source video duration should be positive";
    ASSERT_GT(srcWidth, 0);
    ASSERT_GT(srcHeight, 0);
    ASSERT_GT(srcFps, 0.0);

    // 2. Read all frames and write them to a new file, forwarding timestamps
    fs::path outputPath = makeTestOutputPath("transcode_duration");

    ccap::WriterConfig writerConfig;
    writerConfig.width = static_cast<uint32_t>(srcWidth);
    writerConfig.height = static_cast<uint32_t>(srcHeight);
    writerConfig.frameRate = srcFps;
    writerConfig.bitRate = 2'000'000;

    ccap::VideoWriter writer;
    ASSERT_TRUE(writer.open(outputPath.string(), writerConfig)) << "Failed to open writer";

    int frameCount = 0;
    uint64_t firstTimestamp = 0;
    while (true) {
        auto frame = reader.grab(5000);
        if (!frame) break;

        if (frameCount == 0) {
            firstTimestamp = frame->timestamp;
        }
        uint64_t relativeTs = frame->timestamp - firstTimestamp;

        ASSERT_TRUE(writer.writeFrame(*frame, relativeTs))
            << "Failed to write frame " << frameCount;
        frameCount++;
    }

    writer.close();
    reader.close();

    ASSERT_GT(frameCount, 0) << "No frames read from source video";

    // 3. Open the output file and verify its duration matches the source
    ccap::Provider outReader;
    ASSERT_TRUE(outReader.open(outputPath.string())) << "Failed to open output video for verification";

    double outDuration = outReader.get(ccap::PropertyName::Duration);
    outReader.close();

    // Allow 10% tolerance (encode/decode and container overhead may cause slight differences)
    double ratio = outDuration / srcDuration;
    EXPECT_GT(ratio, 0.9) << "Output duration (" << outDuration
                           << "s) is too short vs source (" << srcDuration << "s)";
    EXPECT_LT(ratio, 1.1) << "Output duration (" << outDuration
                           << "s) is too long vs source (" << srcDuration << "s)";
#else
    GTEST_SKIP() << "File playback not enabled, cannot run transcode test";
#endif
}

// ---- Transcode test with auto-timestamp (should produce shorter video if camera is slower) ----

TEST_F(VideoWriterTest, TranscodeWithAutoTimestampProducesDifferentDuration) {
#ifdef CCAP_ENABLE_FILE_PLAYBACK
    fs::path inputPath = findTestVideo();
    if (!fs::exists(inputPath)) {
        GTEST_SKIP() << "test.mp4 not found at " << inputPath;
    }

    ccap::Provider reader;
    ASSERT_TRUE(reader.open(inputPath.string()));

    double srcDuration = reader.get(ccap::PropertyName::Duration);
    int srcWidth = static_cast<int>(reader.get(ccap::PropertyName::Width));
    int srcHeight = static_cast<int>(reader.get(ccap::PropertyName::Height));
    double srcFps = reader.get(ccap::PropertyName::FrameRate);
    ASSERT_GT(srcDuration, 0.0);

    // Write with auto-timestamp (timestampNs = 0) using a HIGHER frame rate than source
    // This simulates the camera-slower-than-configured scenario
    fs::path outputPath = makeTestOutputPath("transcode_auto_ts");

    ccap::WriterConfig writerConfig;
    writerConfig.width = static_cast<uint32_t>(srcWidth);
    writerConfig.height = static_cast<uint32_t>(srcHeight);
    writerConfig.frameRate = srcFps * 2; // Claim 2x the actual fps
    writerConfig.bitRate = 2'000'000;

    ccap::VideoWriter writer;
    ASSERT_TRUE(writer.open(outputPath.string(), writerConfig));

    int frameCount = 0;
    while (true) {
        auto frame = reader.grab(5000);
        if (!frame) break;
        // Deliberately pass timestampNs = 0 (auto-increment mode)
        ASSERT_TRUE(writer.writeFrame(*frame, 0));
        frameCount++;
    }

    writer.close();
    reader.close();

    ASSERT_GT(frameCount, 0);

    // Verify output is approximately half the source duration (2x claimed fps, same frames)
    ccap::Provider outReader;
    ASSERT_TRUE(outReader.open(outputPath.string()));

    double outDuration = outReader.get(ccap::PropertyName::Duration);
    outReader.close();

    // With 2x fps claimed and auto-timestamp, video duration should be ~half the source
    double ratio = outDuration / srcDuration;
    EXPECT_LT(ratio, 0.7) << "Auto-timestamp with 2x fps should produce shorter video, got ratio=" << ratio;
#else
    GTEST_SKIP() << "File playback not enabled";
#endif
}

TEST_F(VideoWriterCTest, InvalidOpenParams) {
    CcapVideoWriter* writer = ccap_video_writer_create();
    ASSERT_NE(writer, nullptr);

    CcapWriterConfig config = CCAP_WRITER_CONFIG_INIT;
    config.codec = CCAP_VIDEO_CODEC_H264;
    config.width = 320;
    config.height = 240;
    config.bitRate = 2000000;

    // Null filePath
    EXPECT_FALSE(ccap_video_writer_open(writer, nullptr, &config));

    // Null config
    EXPECT_FALSE(ccap_video_writer_open(writer, "test.mp4", nullptr));

    ccap_video_writer_destroy(writer);
}
