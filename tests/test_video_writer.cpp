/**
 * @file test_video_writer.cpp
 * @brief Tests for video writer functionality
 *
 * Tests verify basic writer lifecycle, frame writing, and output validation.
 * Output files are written to a temporary directory and cleaned up after.
 */

#include <ccap.h>
#include <ccap_writer.h>
#include <ccap_writer_c.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <random>
#include <string_view>
#include <thread>

namespace fs = std::filesystem;

// Helper to check if video writer is supported on this platform
bool isVideoWriterSupported() {
#if (defined(__APPLE__) || defined(_WIN32)) && defined(CCAP_ENABLE_VIDEO_WRITER)
    return true;
#else
    return false;
#endif
}

// Generate a unique temp path for test output
fs::path getTestOutputPath(const std::string& name) {
    return fs::temp_directory_path() / ("ccap_writer_test_" + name + ".mp4");
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

// Test fixture for video writer tests
class VideoWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!isVideoWriterSupported()) {
            GTEST_SKIP() << "Video writer not supported on this platform/build";
        }
    }

    void TearDown() override {
        // Clean up any test output files
        for (const auto& entry : fs::directory_iterator(fs::temp_directory_path())) {
            std::string filename = entry.path().filename().string();
            if (filename.find("ccap_writer_test_") == 0) {
                fs::remove(entry.path());
            }
        }
    }
};

// Test fixture for C API tests
class VideoWriterCTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!isVideoWriterSupported()) {
            GTEST_SKIP() << "Video writer not supported on this platform/build";
        }
    }

    void TearDown() override {
        for (const auto& entry : fs::directory_iterator(fs::temp_directory_path())) {
            std::string filename = entry.path().filename().string();
            if (filename.find("ccap_writer_test_") == 0) {
                fs::remove(entry.path());
            }
        }
    }
};

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

    bool result = writer.open(getTestOutputPath("zero_dim").string(), config);
    EXPECT_FALSE(result);
}

TEST_F(VideoWriterTest, OpenAndClose) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 640;
    config.height = 480;
    config.frameRate = 30.0;
    config.bitRate = 5000000;

    fs::path outputPath = getTestOutputPath("open_close");
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

    fs::path outputPath = getTestOutputPath("write_frames");
    ASSERT_TRUE(writer.open(outputPath.string(), config));

    // Create and write 30 frames (1 second at 30fps)
    int w = 320, h = 240;
    int stride = w * 3; // BGR24
    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);

    ccap::VideoFrame frame;
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
        bool writeResult = writer.writeFrame(frame);
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
    ccap::Provider provider;
    EXPECT_TRUE(provider.open(outputPath.string()));
    auto framePtr = provider.grab(5000);
    EXPECT_NE(framePtr, nullptr);
    if (framePtr) {
        EXPECT_EQ(framePtr->width, 320);
        EXPECT_EQ(framePtr->height, 240);
    }
    provider.close();
}

TEST_F(VideoWriterTest, WriteFramesWithMovContainer) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;
    config.container = ccap::VideoFormat::MOV;

    fs::path outputPath = getTestOutputPath("mov_container");
    // Change extension
    outputPath.replace_extension(".mov");

    ASSERT_TRUE(writer.open(outputPath.string(), config));

    int w = 320, h = 240;
    int stride = w * 3;
    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);

    ccap::VideoFrame frame;
    frame.data[0] = frameData.data();
    frame.stride[0] = static_cast<uint32_t>(stride);
    frame.pixelFormat = ccap::PixelFormat::BGR24;
    frame.width = static_cast<uint32_t>(w);
    frame.height = static_cast<uint32_t>(h);
    frame.sizeInBytes = static_cast<uint32_t>(stride * h);
    frame.timestamp = 0;
    frame.frameIndex = 0;
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

    fs::path outputPath = getTestOutputPath("codec_fallback");
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

    fs::path outputPath = getTestOutputPath("write_after_close");
    ASSERT_TRUE(writer.open(outputPath.string(), config));
    writer.close();

    // Writing after close should fail
    ccap::VideoFrame frame;
    frame.data[0] = nullptr;
    frame.pixelFormat = ccap::PixelFormat::BGR24;
    frame.width = 320;
    frame.height = 240;

    EXPECT_FALSE(writer.writeFrame(frame));
}

TEST_F(VideoWriterTest, GetPropertiesAfterOpen) {
    ccap::VideoWriter writer;
    ccap::WriterConfig config;
    config.width = 640;
    config.height = 480;
    config.frameRate = 25.0;
    config.bitRate = 3000000;

    fs::path outputPath = getTestOutputPath("properties");
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

    CcapWriterConfig config;
    config.codec = CCAP_VIDEO_CODEC_HEVC;
    config.container = CCAP_VIDEO_FORMAT_MP4;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;

    fs::path outputPath = getTestOutputPath("c_api");
    ASSERT_TRUE(ccap_video_writer_open(writer, outputPath.string().c_str(), &config));
    EXPECT_TRUE(ccap_video_writer_is_opened(writer));

    // Create BGR frame
    int w = 320, h = 240;
    int stride = w * 3;
    std::vector<uint8_t> frameData = createBgrFrame(w, h, stride);

    CcapVideoFrameInfo frameInfo;
    frameInfo.data[0] = frameData.data();
    frameInfo.stride[0] = static_cast<uint32_t>(stride);
    frameInfo.data[1] = nullptr;
    frameInfo.stride[1] = 0;
    frameInfo.data[2] = nullptr;
    frameInfo.stride[2] = 0;
    frameInfo.pixelFormat = CCAP_PIXEL_FORMAT_BGR24;
    frameInfo.width = static_cast<uint32_t>(w);
    frameInfo.height = static_cast<uint32_t>(h);
    frameInfo.sizeInBytes = static_cast<uint32_t>(stride * h);
    frameInfo.timestamp = 0;
    frameInfo.frameIndex = 0;
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

TEST_F(VideoWriterCTest, InvalidOpenParams) {
    CcapVideoWriter* writer = ccap_video_writer_create();
    ASSERT_NE(writer, nullptr);

    CcapWriterConfig config;
    config.codec = CCAP_VIDEO_CODEC_H264;
    config.container = CCAP_VIDEO_FORMAT_MP4;
    config.width = 320;
    config.height = 240;
    config.frameRate = 30.0;
    config.bitRate = 2000000;

    // Null filePath
    EXPECT_FALSE(ccap_video_writer_open(writer, nullptr, &config));

    // Null config
    EXPECT_FALSE(ccap_video_writer_open(writer, "test.mp4", nullptr));

    ccap_video_writer_destroy(writer);
}
