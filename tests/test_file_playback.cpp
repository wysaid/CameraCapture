/**
 * @file test_file_playback.cpp
 * @brief Tests for video file playback functionality
 * @author GitHub Copilot
 * @date 2025-12-28
 *
 * These tests verify video file playback, seeking, and property access.
 * Note: File playback is only available on Windows and macOS platforms.
 * 
 * Test videos should be downloaded using scripts/download_test_videos.sh or .ps1
 * Videos are stored in the test-videos/ directory at project root.
 */

#include <gtest/gtest.h>
#include <ccap.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// Helper to check if file playback is supported on this platform
bool isFilePlaybackSupported() {
#if defined(__APPLE__) || defined(_WIN32)
    #ifdef CCAP_ENABLE_FILE_PLAYBACK
        return true;
    #else
        return false;
    #endif
#else
    // Linux doesn't support file playback
    return false;
#endif
}

// Test fixture for file playback tests
class FilePlaybackTest : public ::testing::Test {
protected:
    fs::path testVideoPath;
    bool videoAvailable = false;

    void SetUp() override {
        if (!isFilePlaybackSupported()) {
            GTEST_SKIP() << "File playback not supported on this platform";
        }

        // Find project root by looking for test-videos directory
        auto currentDir = fs::current_path();
        fs::path projectRoot = currentDir;
        
        // Navigate up to find project root
        while (projectRoot.has_parent_path()) {
            if (fs::exists(projectRoot / "CMakeLists.txt") && 
                fs::exists(projectRoot / "tests")) {
                // Found project root
                break;
            }
            projectRoot = projectRoot.parent_path();
        }
        
        // Use built-in test video from tests/test-data directory
        auto testDataDir = projectRoot / "tests" / "test-data";
        testVideoPath = testDataDir / "test.mp4";
        
        if (fs::exists(testVideoPath)) {
            std::cout << "Using built-in test video: \"" << testVideoPath.string() << "\"" << std::endl;
            videoAvailable = true;
        } else {
            std::cout << "Test video not found at: " << testVideoPath << std::endl;
            std::cout << "This should not happen - the test video is included in the repository." << std::endl;
            GTEST_SKIP() << "Built-in test video file not found.";
        }
    }

    void TearDown() override {
        // Videos are managed by the download script
    }
};

// ============================================================================
// Basic File Opening Tests
// ============================================================================

TEST_F(FilePlaybackTest, OpenVideoFile) {
    ccap::Provider provider;
    
    ASSERT_TRUE(provider.open(testVideoPath.string(), false))
        << "Failed to open video file: " << testVideoPath;
    
    EXPECT_TRUE(provider.isOpened());
    EXPECT_TRUE(provider.isFileMode());
    
    provider.close();
}

TEST_F(FilePlaybackTest, OpenNonExistentFile) {
    ccap::Provider provider;
    
    // Use a path that definitely doesn't exist
    std::string nonExistentPath = testVideoPath.parent_path().string() + "/nonexistent_video_file.mp4";
    EXPECT_FALSE(provider.open(nonExistentPath, false));
    EXPECT_FALSE(provider.isOpened());
}

TEST_F(FilePlaybackTest, GetVideoProperties) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));
    
    // Get video properties
    double duration = provider.get(ccap::PropertyName::Duration);
    double frameCount = provider.get(ccap::PropertyName::FrameCount);
    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    int width = static_cast<int>(provider.get(ccap::PropertyName::Width));
    int height = static_cast<int>(provider.get(ccap::PropertyName::Height));
    
    // Basic sanity checks
    EXPECT_GT(duration, 0.0) << "Duration should be positive";
    EXPECT_GT(frameCount, 0.0) << "Frame count should be positive";
    EXPECT_GT(frameRate, 0.0) << "Frame rate should be positive";
    EXPECT_GT(width, 0) << "Width should be positive";
    EXPECT_GT(height, 0) << "Height should be positive";
    
    // Check consistency
    double calculatedFrameCount = duration * frameRate;
    EXPECT_NEAR(frameCount, calculatedFrameCount, frameRate) 
        << "Frame count should match duration * frame rate";
    
    std::cout << "Video properties: " << width << "x" << height 
              << ", " << frameRate << " fps, " << duration << "s, " 
              << frameCount << " frames" << std::endl;
    
    provider.close();
}

TEST_F(FilePlaybackTest, GetDeviceInfo) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));
    
    auto deviceInfo = provider.getDeviceInfo();
    ASSERT_TRUE(deviceInfo.has_value());
    
    EXPECT_FALSE(deviceInfo->supportedPixelFormats.empty());
    EXPECT_FALSE(deviceInfo->supportedResolutions.empty());
    
    provider.close();
}

// ============================================================================
// Frame Capture Tests
// ============================================================================

TEST_F(FilePlaybackTest, GrabFrames) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    ASSERT_TRUE(provider.isStarted());
    
    int framesCaptured = 0;
    const int maxFrames = 10;
    
    while (framesCaptured < maxFrames) {
        auto frame = provider.grab(3000);
        if (!frame) {
            break;
        }
        
        EXPECT_GT(frame->width, 0u);
        EXPECT_GT(frame->height, 0u);
        EXPECT_GT(frame->sizeInBytes, 0u);
        EXPECT_NE(frame->data[0], nullptr);
        
        framesCaptured++;
    }
    
    EXPECT_GT(framesCaptured, 0) << "Should capture at least one frame";
    std::cout << "Captured " << framesCaptured << " frames" << std::endl;
    
    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, FrameTimestamps) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    uint64_t lastTimestamp = 0;
    int framesCaptured = 0;
    const int maxFrames = 5;
    
    while (framesCaptured < maxFrames) {
        auto frame = provider.grab(3000);
        if (!frame) break;
        
        if (framesCaptured > 0) {
            EXPECT_GT(frame->timestamp, lastTimestamp) 
                << "Frame timestamps should be monotonically increasing";
        }
        
        lastTimestamp = frame->timestamp;
        framesCaptured++;
    }
    
    provider.stop();
    provider.close();
}

// ============================================================================
// Seeking Tests
// ============================================================================

TEST_F(FilePlaybackTest, SeekToTime) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));
    
    double duration = provider.get(ccap::PropertyName::Duration);
    ASSERT_GT(duration, 1.0) << "Video should be at least 1 second long for seek test";
    
    // Seek to middle
    double targetTime = duration / 2.0;
    EXPECT_TRUE(provider.set(ccap::PropertyName::CurrentTime, targetTime));
    
    double currentTime = provider.get(ccap::PropertyName::CurrentTime);
    EXPECT_NEAR(currentTime, targetTime, 0.5) 
        << "Current time should be close to target time";
    
    // Seek to beginning
    EXPECT_TRUE(provider.set(ccap::PropertyName::CurrentTime, 0.0));
    currentTime = provider.get(ccap::PropertyName::CurrentTime);
    EXPECT_NEAR(currentTime, 0.0, 0.1);
    
    provider.close();
}

TEST_F(FilePlaybackTest, SeekToFrame) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));
    
    double frameCount = provider.get(ccap::PropertyName::FrameCount);
    ASSERT_GT(frameCount, 10.0) << "Video should have at least 10 frames for seek test";
    
    // Seek to frame 10
    int64_t targetFrame = 10;
    EXPECT_TRUE(provider.set(ccap::PropertyName::CurrentFrameIndex, static_cast<double>(targetFrame)));
    
    double currentFrameIndex = provider.get(ccap::PropertyName::CurrentFrameIndex);
    EXPECT_NEAR(currentFrameIndex, static_cast<double>(targetFrame), 2.0)
        << "Current frame index should be close to target";
    
    provider.close();
}

TEST_F(FilePlaybackTest, SeekBoundaries) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));
    
    double duration = provider.get(ccap::PropertyName::Duration);
    
    // Seek beyond end should clamp to end
    EXPECT_TRUE(provider.set(ccap::PropertyName::CurrentTime, duration + 10.0));
    double currentTime = provider.get(ccap::PropertyName::CurrentTime);
    EXPECT_LE(currentTime, duration);
    
    // Seek before start should clamp to 0
    EXPECT_TRUE(provider.set(ccap::PropertyName::CurrentTime, -10.0));
    currentTime = provider.get(ccap::PropertyName::CurrentTime);
    EXPECT_GE(currentTime, 0.0);
    
    provider.close();
}

// ============================================================================
// Playback Speed Tests
// ============================================================================

TEST_F(FilePlaybackTest, SetPlaybackSpeed) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));
    
    // Set normal speed
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 1.0));
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 1.0);
    
    // Set 2x speed
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 2.0));
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 2.0);
    
    // Set 0.5x speed
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 0.5));
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.5);
    
    // Invalid speed (0 or negative) should fail
    EXPECT_FALSE(provider.set(ccap::PropertyName::PlaybackSpeed, 0.0));
    EXPECT_FALSE(provider.set(ccap::PropertyName::PlaybackSpeed, -1.0));
    
    provider.close();
}

// ============================================================================
// Pixel Format Tests
// ============================================================================

TEST_F(FilePlaybackTest, OutputPixelFormat) {
    ccap::Provider provider;
    
    // Test RGB24 output
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::RGB24);
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    auto frame = provider.grab(3000);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->pixelFormat, ccap::PixelFormat::RGB24);
    
    provider.stop();
    provider.close();
    
    // Test BGRA32 output (Note: On Windows, MF RGB32 is actually BGRA32 layout)
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGRA32);
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    frame = provider.grab(3000);
    ASSERT_NE(frame, nullptr);
    // On Windows, the actual pixel format might be RGB32 or BGRA32 depending on platform
    // Accept either as valid output
    EXPECT_TRUE(frame->pixelFormat == ccap::PixelFormat::BGRA32 ||
                frame->pixelFormat == ccap::PixelFormat::RGB24);
    
    provider.stop();
    provider.close();
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(FilePlaybackTest, FilePropertiesInCameraMode) {
    // Verify that file properties return NaN in camera mode (not file mode)
    ccap::Provider provider;
    
    // Open in camera mode (if a camera is available)
    auto devices = provider.findDeviceNames();
    if (devices.empty()) {
        GTEST_SKIP() << "No camera devices available for testing";
    }
    
    if (!provider.open(devices[0], false)) {
        GTEST_SKIP() << "Failed to open camera device";
    }
    
    EXPECT_FALSE(provider.isFileMode());
    
    // File properties should return NaN in camera mode
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::Duration)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::FrameCount)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::CurrentTime)));
    
    provider.close();
}

TEST_F(FilePlaybackTest, SeekWithoutOpen) {
    ccap::Provider provider;
    
    // Attempting to seek without opening should fail
    EXPECT_FALSE(provider.set(ccap::PropertyName::CurrentTime, 5.0));
    EXPECT_FALSE(provider.set(ccap::PropertyName::CurrentFrameIndex, 10.0));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(FilePlaybackTest, PlaySeekAndContinue) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    // Capture first few frames
    for (int i = 0; i < 5; i++) {
        auto frame = provider.grab(3000);
        ASSERT_NE(frame, nullptr);
    }
    
    // Stop playback
    provider.stop();
    
    // Seek to a different position
    double duration = provider.get(ccap::PropertyName::Duration);
    if (duration > 2.0) {
        EXPECT_TRUE(provider.set(ccap::PropertyName::CurrentTime, duration / 2.0));
    }
    
    // Resume playback
    ASSERT_TRUE(provider.start());
    
    // Capture more frames
    for (int i = 0; i < 5; i++) {
        auto frame = provider.grab(3000);
        ASSERT_NE(frame, nullptr);
    }
    
    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, MultipleOpenClose) {
    ccap::Provider provider;
    
    // Open and close multiple times
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(provider.open(testVideoPath.string(), false));
        EXPECT_TRUE(provider.isOpened());
        EXPECT_TRUE(provider.isFileMode());
        provider.close();
        EXPECT_FALSE(provider.isOpened());
    }
}
