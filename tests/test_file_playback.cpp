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

#include <ccap.h>
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

// Helper to check if running in CI environment
bool isRunningInCI() {
    static const char* ciEnv = std::getenv("GITHUB_ACTIONS");
    return ciEnv != nullptr && std::string_view(ciEnv) == "true";
}

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

    // Default should be 0.0 (no frame rate control)
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.0);

    // Set normal speed
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 1.0));
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 1.0);

    // Set 2x speed
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 2.0));
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 2.0);

    // Set 0.5x speed
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 0.5));
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.5);

    // Set 0.0 (no frame rate control)
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 0.0));
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.0);

    // Invalid speed (negative) should fail
    EXPECT_FALSE(provider.set(ccap::PropertyName::PlaybackSpeed, -1.0));

    provider.close();
}

// ============================================================================
// Playback Speed and Timing Tests
// ============================================================================

TEST_F(FilePlaybackTest, DefaultPlaybackSpeedNoDelay) {
    if (isRunningInCI()) {
        GTEST_SKIP() << "Skipping timing-sensitive test in CI environment";
    }
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    // Default PlaybackSpeed should be 0.0 (no frame rate control)
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.0);

    // Measure time to grab frames with default speed
    const int framesToCapture = 30;
    auto startTime = std::chrono::steady_clock::now();

    int framesCaptured = 0;
    while (framesCaptured < framesToCapture) {
        auto frame = provider.grab(5000);
        if (!frame) break;
        framesCaptured++;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

    ASSERT_GT(framesCaptured, 0);

    // With PlaybackSpeed=0, frames should be grabbed very quickly
    // Should be much faster than real-time playback
    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    double expectedMinTime = static_cast<double>(framesCaptured) / frameRate;

    std::cout << "Captured " << framesCaptured << " frames in " << elapsedSeconds
              << " seconds (fps: " << frameRate << ", expected min: " << expectedMinTime << "s)" << std::endl;

    // Should be significantly faster than real-time
    EXPECT_LT(elapsedSeconds, expectedMinTime * 0.2)
        << "With PlaybackSpeed=0, frames should be grabbed much faster than real-time";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, PlaybackSpeedOneMatchesFrameRate) {
    if (isRunningInCI()) {
        GTEST_SKIP() << "Skipping timing-sensitive test in CI environment";
    }
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    // Set PlaybackSpeed to 1.0 after opening
    provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);

    // Verify PlaybackSpeed is 1.0
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 1.0);

    // Measure time to grab frames
    const int framesToCapture = 10;
    auto startTime = std::chrono::steady_clock::now();

    int framesCaptured = 0;
    while (framesCaptured < framesToCapture) {
        auto frame = provider.grab(5000);
        if (!frame) break;
        framesCaptured++;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

    ASSERT_GT(framesCaptured, 0);

    // With PlaybackSpeed=1.0, playback should match video's frame rate
    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    double expectedTime = static_cast<double>(framesCaptured) / frameRate;

    std::cout << "Captured " << framesCaptured << " frames in " << elapsedSeconds
              << " seconds (fps: " << frameRate << ", expected: " << expectedTime << "s)" << std::endl;

    // Allow tolerance for timing variations (includes thread scheduling, frame processing overhead)
    EXPECT_NEAR(elapsedSeconds, expectedTime, 0.1)
        << "With PlaybackSpeed=1.0, playback should roughly match video frame rate";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, PlaybackSpeedDoubleIsFaster) {
    if (isRunningInCI()) {
        GTEST_SKIP() << "Skipping timing-sensitive test in CI environment";
    }
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    // Set PlaybackSpeed to 2.0 after opening
    provider.set(ccap::PropertyName::PlaybackSpeed, 2.0);

    // Measure time to grab frames
    const int framesToCapture = 20;
    auto startTime = std::chrono::steady_clock::now();

    int framesCaptured = 0;
    while (framesCaptured < framesToCapture) {
        auto frame = provider.grab(5000);
        if (!frame) break;
        framesCaptured++;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

    ASSERT_GT(framesCaptured, 0);

    // With PlaybackSpeed=2.0, playback should be 2x faster
    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    double expectedTime = static_cast<double>(framesCaptured) / (frameRate * 2.0);

    std::cout << "Captured " << framesCaptured << " frames in " << elapsedSeconds
              << " seconds (fps: " << frameRate << ", 2x speed expected: " << expectedTime << "s)" << std::endl;

    // Allow tolerance for timing variations (2x speed has more decoding overhead)
    EXPECT_NEAR(elapsedSeconds, expectedTime, 0.2)
        << "With PlaybackSpeed=2.0, playback should be roughly 2x faster";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, PlaybackSpeedHalfIsSlower) {
    if (isRunningInCI()) {
        GTEST_SKIP() << "Skipping timing-sensitive test in CI environment";
    }
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    // Set PlaybackSpeed to 0.5 after opening
    provider.set(ccap::PropertyName::PlaybackSpeed, 0.5);

    // Measure time to grab frames (fewer frames for slower speed)
    const int framesToCapture = 5;
    auto startTime = std::chrono::steady_clock::now();

    int framesCaptured = 0;
    while (framesCaptured < framesToCapture) {
        auto frame = provider.grab(5000);
        if (!frame) break;
        framesCaptured++;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

    ASSERT_GT(framesCaptured, 0);

    // With PlaybackSpeed=0.5, playback should be 0.5x speed (slower)
    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    double expectedTime = static_cast<double>(framesCaptured) / (frameRate * 0.5);

    std::cout << "Captured " << framesCaptured << " frames in " << elapsedSeconds
              << " seconds (fps: " << frameRate << ", 0.5x speed expected: " << expectedTime << "s)" << std::endl;

    // Verify playback is not too fast (lower bound is critical for "slower" test)
    // It should take at least 75% of the expected time (if it ran at 1.0x speed, it would be 50% of expected time)
    EXPECT_GE(elapsedSeconds, expectedTime * 0.75)
        << "Playback was too fast for 0.5x speed (speed limiting might be broken)";

    // Upper bound tolerance
    EXPECT_LE(elapsedSeconds, expectedTime * 1.5)
        << "Playback was unreasonably slow";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, DynamicPlaybackSpeedChange) {
    if (isRunningInCI()) {
        GTEST_SKIP() << "Skipping timing-sensitive test in CI environment";
    }
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    // Start with default speed (0.0 - no control)
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.0);

    // Grab a few frames quickly
    for (int i = 0; i < 5; i++) {
        auto frame = provider.grab(3000);
        ASSERT_NE(frame, nullptr);
    }

    // Change to 1.0x speed
    EXPECT_TRUE(provider.set(ccap::PropertyName::PlaybackSpeed, 1.0));

    // Measure time for next frames
    const int framesToCapture = 10;
    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < framesToCapture; i++) {
        auto frame = provider.grab(5000);
        ASSERT_NE(frame, nullptr);
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    double expectedTime = static_cast<double>(framesToCapture) / frameRate;

    std::cout << "After speed change, captured " << framesToCapture << " frames in "
              << elapsedSeconds << " seconds (expected: " << expectedTime << "s)" << std::endl;

    // Should roughly match frame rate after speed change
    // Ensure it's not too fast (lower bound)
    EXPECT_GE(elapsedSeconds, expectedTime * 0.75);
    // Ensure it's not too slow
    EXPECT_LE(elapsedSeconds, expectedTime * 1.5);

    provider.stop();
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
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::CurrentFrameIndex)));

    provider.close();
}

// ============================================================================
// Comprehensive Property Get Tests
// ============================================================================

TEST_F(FilePlaybackTest, GetAllFileProperties) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    // Test all file-specific properties
    double duration = provider.get(ccap::PropertyName::Duration);
    double frameCount = provider.get(ccap::PropertyName::FrameCount);
    double currentTime = provider.get(ccap::PropertyName::CurrentTime);
    double currentFrameIndex = provider.get(ccap::PropertyName::CurrentFrameIndex);
    double playbackSpeed = provider.get(ccap::PropertyName::PlaybackSpeed);

    // All should return valid values
    EXPECT_FALSE(std::isnan(duration));
    EXPECT_FALSE(std::isnan(frameCount));
    EXPECT_FALSE(std::isnan(currentTime));
    EXPECT_FALSE(std::isnan(currentFrameIndex));
    EXPECT_FALSE(std::isnan(playbackSpeed));

    // Initial values should be sensible
    EXPECT_GT(duration, 0.0);
    EXPECT_GT(frameCount, 0.0);
    EXPECT_GE(currentTime, 0.0);
    EXPECT_GE(currentFrameIndex, 0.0);
    EXPECT_EQ(playbackSpeed, 0.0); // Default is 0.0

    std::cout << "All file properties retrieved successfully:" << std::endl;
    std::cout << "  Duration: " << duration << "s" << std::endl;
    std::cout << "  Frame Count: " << frameCount << std::endl;
    std::cout << "  Current Time: " << currentTime << "s" << std::endl;
    std::cout << "  Current Frame Index: " << currentFrameIndex << std::endl;
    std::cout << "  Playback Speed: " << playbackSpeed << std::endl;

    provider.close();
}

TEST_F(FilePlaybackTest, GetAllCameraProperties) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    // Test all camera-compatible properties
    int width = static_cast<int>(provider.get(ccap::PropertyName::Width));
    int height = static_cast<int>(provider.get(ccap::PropertyName::Height));
    double frameRate = provider.get(ccap::PropertyName::FrameRate);

    EXPECT_GT(width, 0);
    EXPECT_GT(height, 0);
    EXPECT_GT(frameRate, 0.0);

    std::cout << "Camera-compatible properties:" << std::endl;
    std::cout << "  Resolution: " << width << "x" << height << std::endl;
    std::cout << "  Frame Rate: " << frameRate << " fps" << std::endl;

    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesBeforeOpen) {
    ccap::Provider provider;

    // File-specific properties should return NaN before opening
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::Duration)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::FrameCount)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::CurrentTime)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::CurrentFrameIndex)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::PlaybackSpeed)));

    // Camera properties may return default values even before opening
    // (implementation detail - Width/Height/FrameRate may have defaults)
    // So we don't test for NaN on those properties
}

TEST_F(FilePlaybackTest, GetPropertiesAfterClose) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    // Properties should work before close
    EXPECT_FALSE(std::isnan(provider.get(ccap::PropertyName::Duration)));

    provider.close();

    // All properties should return NaN after closing
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::Duration)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::FrameCount)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::CurrentTime)));
    EXPECT_TRUE(std::isnan(provider.get(ccap::PropertyName::CurrentFrameIndex)));
}

TEST_F(FilePlaybackTest, GetCurrentTimeProgression) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    // Set PlaybackSpeed to 1.0 for controlled progression
    provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);

    double time1 = provider.get(ccap::PropertyName::CurrentTime);

    // Grab a few frames
    for (int i = 0; i < 5; i++) {
        auto frame = provider.grab(3000);
        ASSERT_NE(frame, nullptr);
    }

    double time2 = provider.get(ccap::PropertyName::CurrentTime);

    // Time should have progressed
    EXPECT_GT(time2, time1) << "CurrentTime should increase as frames are grabbed";

    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    double expectedTimeDelta = 5.0 / frameRate;

    // Allow some tolerance for timing variations
    EXPECT_NEAR(time2 - time1, expectedTimeDelta, expectedTimeDelta * 0.5)
        << "Time progression should roughly match frame rate";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, GetCurrentFrameIndexProgression) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    double index1 = provider.get(ccap::PropertyName::CurrentFrameIndex);

    // Grab frames
    const int framesToGrab = 3;
    for (int i = 0; i < framesToGrab; i++) {
        auto frame = provider.grab(3000);
        ASSERT_NE(frame, nullptr);
    }

    double index2 = provider.get(ccap::PropertyName::CurrentFrameIndex);

    // Frame index should have progressed
    EXPECT_GE(index2, index1 + framesToGrab - 1)
        << "CurrentFrameIndex should increase as frames are grabbed";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesConsistencyAfterSeek) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    double duration = provider.get(ccap::PropertyName::Duration);
    double frameCount = provider.get(ccap::PropertyName::FrameCount);
    double frameRate = provider.get(ccap::PropertyName::FrameRate);

    // Seek to middle
    double targetTime = duration / 2.0;
    provider.set(ccap::PropertyName::CurrentTime, targetTime);

    // Properties like duration, frameCount, frameRate should remain unchanged
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::Duration), duration);
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::FrameCount), frameCount);
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::FrameRate), frameRate);

    // CurrentTime should reflect the seek
    double newTime = provider.get(ccap::PropertyName::CurrentTime);
    EXPECT_NEAR(newTime, targetTime, 0.5);

    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesWhileStopped) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    double durationWhilePlaying = provider.get(ccap::PropertyName::Duration);

    provider.stop();

    // Properties should still be accessible while stopped
    double durationWhileStopped = provider.get(ccap::PropertyName::Duration);
    EXPECT_DOUBLE_EQ(durationWhilePlaying, durationWhileStopped);

    double frameCount = provider.get(ccap::PropertyName::FrameCount);
    EXPECT_GT(frameCount, 0.0);

    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesAtStartAndEnd) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    // At start
    double timeAtStart = provider.get(ccap::PropertyName::CurrentTime);
    double indexAtStart = provider.get(ccap::PropertyName::CurrentFrameIndex);

    EXPECT_NEAR(timeAtStart, 0.0, 0.1) << "Should start at time 0";
    EXPECT_NEAR(indexAtStart, 0.0, 1.0) << "Should start at frame 0";

    // Seek to end
    double duration = provider.get(ccap::PropertyName::Duration);
    double frameCount = provider.get(ccap::PropertyName::FrameCount);

    provider.set(ccap::PropertyName::CurrentTime, duration);

    double timeAtEnd = provider.get(ccap::PropertyName::CurrentTime);
    double indexAtEnd = provider.get(ccap::PropertyName::CurrentFrameIndex);

    // Should be at or near the end
    EXPECT_NEAR(timeAtEnd, duration, 0.5);
    EXPECT_NEAR(indexAtEnd, frameCount - 1, 2.0);

    provider.close();
}

TEST_F(FilePlaybackTest, GetInvalidProperty) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    // Test with an invalid/unsupported property value
    // PropertyName is an enum, so we can't easily pass invalid values
    // But we can test properties that don't apply to file mode
    // (This is more of a documentation test)

    // These properties work in both camera and file modes
    EXPECT_FALSE(std::isnan(provider.get(ccap::PropertyName::Width)));
    EXPECT_FALSE(std::isnan(provider.get(ccap::PropertyName::Height)));

    provider.close();
}

TEST_F(FilePlaybackTest, SeekWithoutOpen) {
    ccap::Provider provider;

    // Attempting to seek without opening should fail
    EXPECT_FALSE(provider.set(ccap::PropertyName::CurrentTime, 5.0));
    EXPECT_FALSE(provider.set(ccap::PropertyName::CurrentFrameIndex, 10.0));
}

// ============================================================================
// Concurrent Property Access Tests
// ============================================================================

TEST_F(FilePlaybackTest, ConcurrentGetProperties) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    std::atomic<bool> testFailed{ false };
    std::atomic<int> completedThreads{ 0 };
    const int numThreads = 4;

    auto readPropertiesWorker = [&]() {
        try {
            for (int i = 0; i < 20; i++) {
                double duration = provider.get(ccap::PropertyName::Duration);
                double frameCount = provider.get(ccap::PropertyName::FrameCount);
                double currentTime = provider.get(ccap::PropertyName::CurrentTime);
                double frameRate = provider.get(ccap::PropertyName::FrameRate);

                // Validate returned values
                if (std::isnan(duration) || duration <= 0.0) {
                    testFailed = true;
                    break;
                }
                if (std::isnan(frameCount) || frameCount <= 0.0) {
                    testFailed = true;
                    break;
                }
                if (std::isnan(frameRate) || frameRate <= 0.0) {
                    testFailed = true;
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } catch (...) {
            testFailed = true;
        }
        completedThreads++;
    };

    // Launch multiple threads reading properties concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(readPropertiesWorker);
    }

    // Main thread also grabs frames
    for (int i = 0; i < 10; i++) {
        auto frame = provider.grab(1000);
        if (!frame) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(testFailed) << "Concurrent property access should be safe";
    EXPECT_EQ(completedThreads, numThreads) << "All threads should complete";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesWhileGrabbing) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    std::atomic<bool> keepReading{ true };
    std::atomic<bool> propertyReadFailed{ false };

    // Thread continuously reading properties
    std::thread propertyReader([&]() {
        while (keepReading) {
            double duration = provider.get(ccap::PropertyName::Duration);
            double frameCount = provider.get(ccap::PropertyName::FrameCount);

            if (std::isnan(duration) || std::isnan(frameCount)) {
                propertyReadFailed = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Main thread grabs frames
    for (int i = 0; i < 20; i++) {
        auto frame = provider.grab(1000);
        if (!frame) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    keepReading = false;
    propertyReader.join();

    EXPECT_FALSE(propertyReadFailed) << "Property reads should work while grabbing frames";

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesDuringSeek) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    double duration = provider.get(ccap::PropertyName::Duration);

    std::atomic<bool> keepSeeking{ true };
    std::atomic<int> propertyReadCount{ 0 };

    // Thread seeking back and forth
    std::thread seeker([&]() {
        double seekPositions[] = { 0.0, duration * 0.25, duration * 0.5, duration * 0.75, 0.0 };
        int index = 0;

        while (keepSeeking) {
            provider.set(ccap::PropertyName::CurrentTime, seekPositions[index]);
            index = (index + 1) % 5;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Main thread reads properties
    for (int i = 0; i < 30; i++) {
        double currentTime = provider.get(ccap::PropertyName::CurrentTime);
        double currentFrame = provider.get(ccap::PropertyName::CurrentFrameIndex);

        // Values should always be valid (not NaN)
        EXPECT_FALSE(std::isnan(currentTime));
        EXPECT_FALSE(std::isnan(currentFrame));

        propertyReadCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    keepSeeking = false;
    seeker.join();

    EXPECT_GT(propertyReadCount, 20) << "Should successfully read properties while seeking";

    provider.stop();
    provider.close();
}

// ============================================================================
// Property Accuracy and Precision Tests
// ============================================================================

TEST_F(FilePlaybackTest, CurrentTimeAccuracy) {
    if (isRunningInCI()) {
        GTEST_SKIP() << "Skipping timing-sensitive test in CI environment";
    }
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);

    double frameRate = provider.get(ccap::PropertyName::FrameRate);

    // Grab frames and check time progression accuracy
    std::vector<double> times;
    for (int i = 0; i < 10; i++) {
        auto frame = provider.grab(3000);
        if (!frame) break;

        double currentTime = provider.get(ccap::PropertyName::CurrentTime);
        times.push_back(currentTime);
    }

    ASSERT_GE(times.size(), 5);

    // Check time deltas
    for (size_t i = 1; i < times.size(); i++) {
        double delta = times[i] - times[i - 1];
        double expectedDelta = 1.0 / frameRate;

        // Allow tolerance for timing variations
        EXPECT_NEAR(delta, expectedDelta, expectedDelta * 0.5)
            << "Time delta between frames should roughly match frame rate";
    }

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, FrameIndexAccuracy) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    std::vector<uint64_t> frameIndices;

    // Grab frames and collect indices
    for (int i = 0; i < 10; i++) {
        auto frame = provider.grab(3000);
        if (!frame) break;

        frameIndices.push_back(frame->frameIndex);

        // Check that CurrentFrameIndex is at or ahead of the grabbed frame
        // Due to async decoding with backpressure, CurrentFrameIndex may lead
        // by up to DEFAULT_MAX_AVAILABLE_FRAME_SIZE frames (default is 3)
        // This is controlled by shouldReadMoreFrames() in file mode
        double currentIndex = provider.get(ccap::PropertyName::CurrentFrameIndex);
        EXPECT_GE(currentIndex, static_cast<double>(frame->frameIndex))
            << "CurrentFrameIndex should be at or ahead of grabbed frame index";
        EXPECT_LE(currentIndex - static_cast<double>(frame->frameIndex), ccap::DEFAULT_MAX_AVAILABLE_FRAME_SIZE)
            << "CurrentFrameIndex should not lead grabbed frame by more than available frame queue size";
    }

    // Verify frame indices are consecutive
    for (size_t i = 1; i < frameIndices.size(); i++) {
        EXPECT_EQ(frameIndices[i], frameIndices[i - 1] + 1)
            << "Frame indices should be consecutive";
    }

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, SeekAccuracyMultiplePositions) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    double duration = provider.get(ccap::PropertyName::Duration);

    // Test seeking to various positions
    std::vector<double> seekTargets = { 0.0, duration * 0.1, duration * 0.25, duration * 0.5,
                                        duration * 0.75, duration * 0.9 };

    for (double targetTime : seekTargets) {
        if (targetTime > duration) continue;

        ASSERT_TRUE(provider.set(ccap::PropertyName::CurrentTime, targetTime))
            << "Seek to time " << targetTime << " should succeed";

        double actualTime = provider.get(ccap::PropertyName::CurrentTime);

        // Allow 0.5 second tolerance
        EXPECT_NEAR(actualTime, targetTime, 0.5)
            << "Seek to " << targetTime << "s accuracy check";

        std::cout << "Seek target: " << targetTime << "s, actual: " << actualTime << "s" << std::endl;
    }

    provider.close();
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(FilePlaybackTest, GetPropertiesAfterMultipleSeeks) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    double duration = provider.get(ccap::PropertyName::Duration);
    double initialFrameCount = provider.get(ccap::PropertyName::FrameCount);

    // Perform multiple seeks
    for (int i = 0; i < 10; i++) {
        double seekTime = (duration / 10.0) * i;
        provider.set(ccap::PropertyName::CurrentTime, seekTime);
    }

    // Static properties should remain unchanged
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::Duration), duration);
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::FrameCount), initialFrameCount);

    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesAfterPlaybackSpeedChange) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    double duration = provider.get(ccap::PropertyName::Duration);
    double frameCount = provider.get(ccap::PropertyName::FrameCount);

    // Change playback speed multiple times
    provider.set(ccap::PropertyName::PlaybackSpeed, 2.0);
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 2.0);

    provider.set(ccap::PropertyName::PlaybackSpeed, 0.5);
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.5);

    provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 1.0);

    // Static properties should remain unchanged
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::Duration), duration);
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::FrameCount), frameCount);

    provider.stop();
    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesRapidQueries) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    // Rapidly query properties many times
    for (int i = 0; i < 100; i++) {
        double duration = provider.get(ccap::PropertyName::Duration);
        double frameCount = provider.get(ccap::PropertyName::FrameCount);
        double currentTime = provider.get(ccap::PropertyName::CurrentTime);

        EXPECT_FALSE(std::isnan(duration));
        EXPECT_FALSE(std::isnan(frameCount));
        EXPECT_FALSE(std::isnan(currentTime));
    }

    provider.close();
}

TEST_F(FilePlaybackTest, GetPropertiesStressTest) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    std::atomic<bool> testFailed{ false };
    std::atomic<int> totalOperations{ 0 };

    // Multiple threads doing different operations
    auto worker = [&](int threadId) {
        try {
            for (int i = 0; i < 50; i++) {
                switch (threadId % 3) {
                case 0: {
                    // Read properties
                    double d = provider.get(ccap::PropertyName::Duration);
                    double fc = provider.get(ccap::PropertyName::FrameCount);
                    if (std::isnan(d) || std::isnan(fc)) testFailed = true;
                    break;
                }
                case 1: {
                    // Grab frames
                    auto frame = provider.grab(100);
                    if (frame) {
                        double ct = provider.get(ccap::PropertyName::CurrentTime);
                        if (std::isnan(ct)) testFailed = true;
                    }
                    break;
                }
                case 2: {
                    // Read dynamic properties
                    double ct = provider.get(ccap::PropertyName::CurrentTime);
                    double ci = provider.get(ccap::PropertyName::CurrentFrameIndex);
                    if (std::isnan(ct) || std::isnan(ci)) testFailed = true;
                    break;
                }
                }
                totalOperations++;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        } catch (...) {
            testFailed = true;
        }
    };

    std::vector<std::thread> threads;
    const int numThreads = 6;
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(testFailed) << "Stress test should not fail";
    EXPECT_GT(totalOperations, numThreads * 40) << "Most operations should complete";

    std::cout << "Stress test completed " << totalOperations << " operations" << std::endl;

    provider.stop();
    provider.close();
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

// ============================================================================
// Frame Count Validation with FFmpeg
// ============================================================================

TEST_F(FilePlaybackTest, FrameCountMatchesFFmpeg) {
    // Check if ffmpeg is available
    bool hasFFmpeg = false;

#ifdef _WIN32
    // Windows: try to run ffmpeg -version
    int ret = system("ffmpeg -version >nul 2>&1");
    hasFFmpeg = (ret == 0);
#else
    // Unix-like: try to run ffmpeg -version
    int ret = system("ffmpeg -version >/dev/null 2>&1");
    hasFFmpeg = (ret == 0);
#endif

    if (!hasFFmpeg) {
        GTEST_SKIP() << "ffmpeg not found in system PATH, skipping test";
    }

    std::cout << "[OK] ffmpeg detected, comparing frame counts..." << std::endl;

    // Get frame count from ffmpeg by using a temp file for output
    fs::path tempFile = fs::temp_directory_path() / "ccap_ffmpeg_output.txt";

    std::string ffmpegCmd = "ffmpeg -i \"" + testVideoPath.string() +
        "\" -map 0:v:0 -c copy -f null -";
#ifdef _WIN32
    ffmpegCmd += " 2>\"" + tempFile.string() + "\"";
#else
    ffmpegCmd += " 2>\"" + tempFile.string() + "\"";
#endif

    int cmdResult = system(ffmpegCmd.c_str());

    // Wait a bit for file to be written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int ffmpegFrameCount = 0;

    // Read the temporary file (binary mode to handle \r line endings)
    std::ifstream infile(tempFile, std::ios::binary);
    if (infile.is_open()) {
        // Read entire content
        std::string content((std::istreambuf_iterator<char>(infile)),
                            std::istreambuf_iterator<char>());
        infile.close();

        // Process content character by character to handle \r line endings
        size_t pos = 0;
        std::string currentLine;
        while (pos < content.length()) {
            char ch = content[pos++];
            if (ch == '\r' || ch == '\n') {
                if (!currentLine.empty()) {
                    // Look for "frame= XXXX" in output
                    size_t framePos = currentLine.find("frame=");
                    if (framePos != std::string::npos) {
                        size_t startPos = framePos + 6;
                        // Skip whitespace
                        while (startPos < currentLine.length() && std::isspace(currentLine[startPos])) {
                            startPos++;
                        }
                        // Extract number
                        size_t endPos = startPos;
                        while (endPos < currentLine.length() && std::isdigit(currentLine[endPos])) {
                            endPos++;
                        }
                        if (endPos > startPos) {
                            std::string numStr = currentLine.substr(startPos, endPos - startPos);
                            try {
                                int frameNum = std::stoi(numStr);
                                // Keep the highest frame number (ffmpeg outputs progress)
                                if (frameNum > ffmpegFrameCount) {
                                    ffmpegFrameCount = frameNum;
                                }
                            } catch (...) {
                                // Continue trying to parse
                            }
                        }
                    }
                    currentLine.clear();
                }
            } else {
                currentLine += ch;
            }
        }
        // Process last line if any
        if (!currentLine.empty()) {
            size_t framePos = currentLine.find("frame=");
            if (framePos != std::string::npos) {
                size_t startPos = framePos + 6;
                while (startPos < currentLine.length() && std::isspace(currentLine[startPos])) {
                    startPos++;
                }
                size_t endPos = startPos;
                while (endPos < currentLine.length() && std::isdigit(currentLine[endPos])) {
                    endPos++;
                }
                if (endPos > startPos) {
                    std::string numStr = currentLine.substr(startPos, endPos - startPos);
                    try {
                        int frameNum = std::stoi(numStr);
                        if (frameNum > ffmpegFrameCount) {
                            ffmpegFrameCount = frameNum;
                        }
                    } catch (...) {
                        // Continue trying to parse
                    }
                }
            }
        }
    }

    // Clean up temp file
    try {
        fs::remove(tempFile);
    } catch (...) {
        // Ignore cleanup errors
    }

    ASSERT_GT(ffmpegFrameCount, 0) << "Failed to get frame count from ffmpeg";
    std::cout << "ffmpeg reported: " << ffmpegFrameCount << " frames" << std::endl;

    // Get frame count from ccap with random delays
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    int ccapFrameCount = 0;
    double totalWaitBudget = 2.0; // 2 seconds total wait budget
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(50, 100); // 50-100 ms

    auto startTime = std::chrono::steady_clock::now();

    while (auto frame = provider.grab(1000)) {
        ccapFrameCount++;

        // Random wait with budget control
        if (totalWaitBudget > 0) {
            int waitMs = dis(gen);
            double waitSec = waitMs / 1000.0;

            if (waitSec <= totalWaitBudget) {
                std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
                totalWaitBudget -= waitSec;
            } else {
                // Use remaining budget
                if (totalWaitBudget > 0.001) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(static_cast<int>(totalWaitBudget * 1000)));
                }
                totalWaitBudget = 0;
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        endTime - startTime)
                        .count() /
        1000.0;

    std::cout << "ccap counted: " << ccapFrameCount << " frames in "
              << duration << " seconds (with random delays)" << std::endl;
    std::cout << "Remaining wait budget: " << totalWaitBudget << " seconds" << std::endl;

    // Verify frame counts match
    EXPECT_EQ(ccapFrameCount, ffmpegFrameCount)
        << "ccap frame count does not match ffmpeg frame count";

    provider.stop();
    provider.close();
}

// Helper function to check if ffmpeg is available
static bool isFFmpegAvailable() {
#ifdef _WIN32
    int ret = system("ffmpeg -version >nul 2>&1");
    return (ret == 0);
#else
    int ret = system("ffmpeg -version >/dev/null 2>&1");
    return (ret == 0);
#endif
}

// Helper to parse ffmpeg video info
struct FFmpegVideoInfo {
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double duration = 0.0;
    int frameCount = 0;
};

static FFmpegVideoInfo getFFmpegVideoInfo(const std::string& videoPath) {
    FFmpegVideoInfo info;

    fs::path tempFile = fs::temp_directory_path() / "ccap_ffprobe_output.txt";

    // Use ffprobe for better metadata extraction
    std::string cmd = "ffprobe -v error -select_streams v:0 "
                      "-show_entries stream=width,height,r_frame_rate,duration,nb_frames "
                      "-of default=noprint_wrappers=1 \"" +
        videoPath + "\"";
#ifdef _WIN32
    cmd += " >\"" + tempFile.string() + "\" 2>&1";
#else
    cmd += " >\"" + tempFile.string() + "\" 2>&1";
#endif

    int ret = system(cmd.c_str());

    if (ret != 0) {
        // Fallback to ffmpeg -i
        cmd = "ffmpeg -i \"" + videoPath + "\"";
#ifdef _WIN32
        cmd += " 2>\"" + tempFile.string() + "\"";
#else
        cmd += " 2>\"" + tempFile.string() + "\"";
#endif
        system(cmd.c_str());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Parse output
    std::ifstream infile(tempFile);
    if (infile.is_open()) {
        std::string line;
        while (std::getline(infile, line)) {
            // Parse width
            if (line.find("width=") != std::string::npos) {
                size_t pos = line.find("width=") + 6;
                info.width = std::stoi(line.substr(pos));
            }
            // Parse height
            else if (line.find("height=") != std::string::npos) {
                size_t pos = line.find("height=") + 7;
                info.height = std::stoi(line.substr(pos));
            }
            // Parse fps (r_frame_rate)
            else if (line.find("r_frame_rate=") != std::string::npos) {
                size_t pos = line.find("r_frame_rate=") + 13;
                std::string fpsStr = line.substr(pos);
                // Parse fraction like "30/1"
                size_t slashPos = fpsStr.find('/');
                if (slashPos != std::string::npos) {
                    double num = std::stod(fpsStr.substr(0, slashPos));
                    double den = std::stod(fpsStr.substr(slashPos + 1));
                    info.fps = num / den;
                }
            }
            // Parse duration
            else if (line.find("duration=") != std::string::npos) {
                size_t pos = line.find("duration=") + 9;
                info.duration = std::stod(line.substr(pos));
            }
            // Parse frame count
            else if (line.find("nb_frames=") != std::string::npos) {
                size_t pos = line.find("nb_frames=") + 10;
                info.frameCount = std::stoi(line.substr(pos));
            }
            // Fallback: parse from ffmpeg -i output "Stream #0:0: Video: ... 1920x1080, ... 30 fps"
            else if (line.find("Stream") != std::string::npos && line.find("Video:") != std::string::npos) {
                // Extract resolution
                size_t resPos = line.find("x", line.find("Video:"));
                if (resPos != std::string::npos && info.width == 0) {
                    // Find start of width
                    size_t widthStart = resPos;
                    while (widthStart > 0 && std::isdigit(line[widthStart - 1])) {
                        widthStart--;
                    }
                    info.width = std::stoi(line.substr(widthStart, resPos - widthStart));

                    // Find end of height
                    size_t heightStart = resPos + 1;
                    size_t heightEnd = heightStart;
                    while (heightEnd < line.length() && std::isdigit(line[heightEnd])) {
                        heightEnd++;
                    }
                    info.height = std::stoi(line.substr(heightStart, heightEnd - heightStart));
                }

                // Extract fps
                size_t fpsPos = line.find(" fps");
                if (fpsPos != std::string::npos && info.fps == 0.0) {
                    size_t fpsStart = fpsPos;
                    while (fpsStart > 0 && (std::isdigit(line[fpsStart - 1]) || line[fpsStart - 1] == '.')) {
                        fpsStart--;
                    }
                    info.fps = std::stod(line.substr(fpsStart, fpsPos - fpsStart));
                }
            }
            // Extract duration from "Duration: HH:MM:SS.ms"
            else if (line.find("Duration:") != std::string::npos && info.duration == 0.0) {
                size_t durPos = line.find("Duration:") + 10;
                std::string durStr = line.substr(durPos);
                // Parse HH:MM:SS.ms
                if (durStr.length() >= 11) {
                    int hours = std::stoi(durStr.substr(0, 2));
                    int minutes = std::stoi(durStr.substr(3, 2));
                    double seconds = std::stod(durStr.substr(6, 5));
                    info.duration = hours * 3600.0 + minutes * 60.0 + seconds;
                }
            }
        }
        infile.close();
    }

    // Calculate frame count if not available
    if (info.frameCount == 0 && info.duration > 0 && info.fps > 0) {
        info.frameCount = static_cast<int>(info.duration * info.fps + 0.5);
    }

    // Clean up
    try {
        fs::remove(tempFile);
    } catch (...) {
    }

    return info;
}

// ============================================================================
// FFmpeg Property Validation Tests
// ============================================================================

TEST_F(FilePlaybackTest, ResolutionMatchesFFmpeg) {
    if (!isFFmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found in system PATH, skipping test";
    }

    std::cout << "[OK] ffmpeg detected, comparing resolution..." << std::endl;

    auto ffmpegInfo = getFFmpegVideoInfo(testVideoPath.string());

    ASSERT_GT(ffmpegInfo.width, 0) << "Failed to get width from ffmpeg";
    ASSERT_GT(ffmpegInfo.height, 0) << "Failed to get height from ffmpeg";

    std::cout << "ffmpeg reported: " << ffmpegInfo.width << "x" << ffmpegInfo.height << std::endl;

    // Get resolution from ccap
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    int ccapWidth = static_cast<int>(provider.get(ccap::PropertyName::Width));
    int ccapHeight = static_cast<int>(provider.get(ccap::PropertyName::Height));

    std::cout << "ccap reported: " << ccapWidth << "x" << ccapHeight << std::endl;

    // Verify resolution matches
    EXPECT_EQ(ccapWidth, ffmpegInfo.width) << "Width does not match ffmpeg";
    EXPECT_EQ(ccapHeight, ffmpegInfo.height) << "Height does not match ffmpeg";

    provider.close();
}

TEST_F(FilePlaybackTest, FrameRateMatchesFFmpeg) {
    if (!isFFmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found in system PATH, skipping test";
    }

    std::cout << "[OK] ffmpeg detected, comparing frame rate..." << std::endl;

    auto ffmpegInfo = getFFmpegVideoInfo(testVideoPath.string());

    ASSERT_GT(ffmpegInfo.fps, 0.0) << "Failed to get fps from ffmpeg";

    std::cout << "ffmpeg reported: " << ffmpegInfo.fps << " fps" << std::endl;

    // Get frame rate from ccap
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    double ccapFps = provider.get(ccap::PropertyName::FrameRate);

    std::cout << "ccap reported: " << ccapFps << " fps" << std::endl;

    // Verify frame rate matches (allow small tolerance)
    EXPECT_NEAR(ccapFps, ffmpegInfo.fps, 0.1) << "Frame rate does not match ffmpeg";

    provider.close();
}

TEST_F(FilePlaybackTest, DurationMatchesFFmpeg) {
    if (!isFFmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found in system PATH, skipping test";
    }

    std::cout << "[OK] ffmpeg detected, comparing duration..." << std::endl;

    auto ffmpegInfo = getFFmpegVideoInfo(testVideoPath.string());

    ASSERT_GT(ffmpegInfo.duration, 0.0) << "Failed to get duration from ffmpeg";

    std::cout << "ffmpeg reported: " << ffmpegInfo.duration << " seconds" << std::endl;

    // Get duration from ccap
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    double ccapDuration = provider.get(ccap::PropertyName::Duration);

    std::cout << "ccap reported: " << ccapDuration << " seconds" << std::endl;

    // Verify duration matches (allow 0.1 second tolerance)
    EXPECT_NEAR(ccapDuration, ffmpegInfo.duration, 0.1) << "Duration does not match ffmpeg";

    provider.close();
}

TEST_F(FilePlaybackTest, AllPropertiesMatchFFmpeg) {
    if (!isFFmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found in system PATH, skipping test";
    }

    std::cout << "[OK] ffmpeg detected, comparing all properties..." << std::endl;

    auto ffmpegInfo = getFFmpegVideoInfo(testVideoPath.string());

    std::cout << "ffmpeg info:" << std::endl;
    std::cout << "  Resolution: " << ffmpegInfo.width << "x" << ffmpegInfo.height << std::endl;
    std::cout << "  Frame Rate: " << ffmpegInfo.fps << " fps" << std::endl;
    std::cout << "  Duration: " << ffmpegInfo.duration << " seconds" << std::endl;
    std::cout << "  Frame Count: " << ffmpegInfo.frameCount << " frames" << std::endl;

    // Get properties from ccap
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), false));

    int ccapWidth = static_cast<int>(provider.get(ccap::PropertyName::Width));
    int ccapHeight = static_cast<int>(provider.get(ccap::PropertyName::Height));
    double ccapFps = provider.get(ccap::PropertyName::FrameRate);
    double ccapDuration = provider.get(ccap::PropertyName::Duration);
    double ccapFrameCount = provider.get(ccap::PropertyName::FrameCount);

    std::cout << "\nccap info:" << std::endl;
    std::cout << "  Resolution: " << ccapWidth << "x" << ccapHeight << std::endl;
    std::cout << "  Frame Rate: " << ccapFps << " fps" << std::endl;
    std::cout << "  Duration: " << ccapDuration << " seconds" << std::endl;
    std::cout << "  Frame Count: " << ccapFrameCount << " frames" << std::endl;

    // Verify all properties match
    bool allMatch = true;

    if (ffmpegInfo.width > 0 && ccapWidth != ffmpegInfo.width) {
        std::cout << "\n[FAIL] Width mismatch!" << std::endl;
        allMatch = false;
    }

    if (ffmpegInfo.height > 0 && ccapHeight != ffmpegInfo.height) {
        std::cout << "[FAIL] Height mismatch!" << std::endl;
        allMatch = false;
    }

    if (ffmpegInfo.fps > 0 && std::abs(ccapFps - ffmpegInfo.fps) > 0.1) {
        std::cout << "[FAIL] Frame rate mismatch!" << std::endl;
        allMatch = false;
    }

    if (ffmpegInfo.duration > 0 && std::abs(ccapDuration - ffmpegInfo.duration) > 0.1) {
        std::cout << "[FAIL] Duration mismatch!" << std::endl;
        allMatch = false;
    }

    if (ffmpegInfo.frameCount > 0 && std::abs(ccapFrameCount - ffmpegInfo.frameCount) > 1.0) {
        std::cout << "[FAIL] Frame count mismatch!" << std::endl;
        allMatch = false;
    }

    if (allMatch) {
        std::cout << "\n[OK] All properties match!" << std::endl;
    }

    EXPECT_TRUE(allMatch) << "Some properties do not match ffmpeg";

    provider.close();
}
// ============================================================================
// Frame Orientation Tests
// ============================================================================

// Helper function to compare two BMP files pixel by pixel
static bool compareImages(const std::string& file1, const std::string& file2,
                          int& width, int& height, double& similarity) {
    std::ifstream f1(file1, std::ios::binary);
    std::ifstream f2(file2, std::ios::binary);

    if (!f1 || !f2) {
        return false;
    }

    // Read BMP header from first file
    char header1[54];
    f1.read(header1, 54);
    if (!f1) {
        return false;
    }
    std::memcpy(&width, &header1[18], sizeof(int));
    std::memcpy(&height, &header1[22], sizeof(int));

    // Read BMP header from second file
    char header2[54];
    f2.read(header2, 54);
    if (!f2) {
        return false;
    }
    int width2 = 0, height2 = 0;
    std::memcpy(&width2, &header2[18], sizeof(int));
    std::memcpy(&height2, &header2[22], sizeof(int));

    if (width != width2 || height != height2) {
        return false;
    }

    // Read pixel data
    size_t imageSize = std::abs(height) * ((width * 3 + 3) & ~3); // BMP row padding
    std::vector<uint8_t> pixels1(imageSize);
    std::vector<uint8_t> pixels2(imageSize);

    f1.read(reinterpret_cast<char*>(pixels1.data()), imageSize);
    f2.read(reinterpret_cast<char*>(pixels2.data()), imageSize);

    // Calculate similarity
    uint64_t totalDiff = 0;
    for (size_t i = 0; i < imageSize; ++i) {
        int diff = std::abs(static_cast<int>(pixels1[i]) - static_cast<int>(pixels2[i]));
        totalDiff += diff;
    }

    double avgDiff = static_cast<double>(totalDiff) / imageSize;
    similarity = 100.0 * (1.0 - avgDiff / 255.0);

    return true;
}

TEST_F(FilePlaybackTest, FrameOrientationSettingInVideoMode) {
    ccap::Provider provider;

    fs::path tempDir = fs::temp_directory_path() / "ccap_orientation_test";
    fs::create_directories(tempDir);

    // Test 1: Set orientation to TopToBottom, capture a frame
    provider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::TopToBottom);
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGR24);

    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    auto frame1 = provider.grab(3000);
    ASSERT_NE(frame1, nullptr) << "Failed to grab first frame";

    // Check that frame1 has the correct orientation
    ASSERT_EQ(frame1->orientation, ccap::FrameOrientation::TopToBottom)
        << "Frame 1 should have TopToBottom orientation";

    // Copy frame1 data for later comparison
    std::vector<uint8_t> frame1Data(frame1->stride[0] * frame1->height);
    memcpy(frame1Data.data(), frame1->data[0], frame1Data.size());
    uint32_t frame1Width = frame1->width;
    uint32_t frame1Height = frame1->height;
    uint32_t frame1Stride = frame1->stride[0];

    // Save frame1 for visual verification (using same orientation to make files comparable)
    std::string file1 = (tempDir / "frame_TopToBottom.bmp").string();
    bool isBGR1 = ccap::pixelFormatInclude(frame1->pixelFormat, ccap::kPixelFormatBGRBit);
    bool hasAlpha1 = ccap::pixelFormatInclude(frame1->pixelFormat, ccap::kPixelFormatAlphaColorBit);
    ccap::saveRgbDataAsBMP(file1.c_str(), frame1->data[0], frame1->width,
                           frame1->stride[0], frame1->height, isBGR1, hasAlpha1,
                           frame1->orientation == ccap::FrameOrientation::TopToBottom);

    std::cout << "Frame 1 orientation (requested TopToBottom): "
              << (frame1->orientation == ccap::FrameOrientation::TopToBottom ? "TopToBottom" : "BottomToTop")
              << std::endl;

    provider.stop();
    provider.close();

    // Test 2: Set orientation to BottomToTop, capture a frame from the same video
    provider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::BottomToTop);
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGR24);

    ASSERT_TRUE(provider.open(testVideoPath.string(), true));

    auto frame2 = provider.grab(3000);
    ASSERT_NE(frame2, nullptr) << "Failed to grab second frame";

    // Check that frame2 has the correct orientation
    ASSERT_EQ(frame2->orientation, ccap::FrameOrientation::BottomToTop)
        << "Frame 2 should have BottomToTop orientation";

    // Copy frame2 data for comparison
    std::vector<uint8_t> frame2Data(frame2->stride[0] * frame2->height);
    memcpy(frame2Data.data(), frame2->data[0], frame2Data.size());

    // Save frame2 for visual verification
    std::string file2 = (tempDir / "frame_BottomToTop.bmp").string();
    bool isBGR2 = ccap::pixelFormatInclude(frame2->pixelFormat, ccap::kPixelFormatBGRBit);
    bool hasAlpha2 = ccap::pixelFormatInclude(frame2->pixelFormat, ccap::kPixelFormatAlphaColorBit);
    ccap::saveRgbDataAsBMP(file2.c_str(), frame2->data[0], frame2->width,
                           frame2->stride[0], frame2->height, isBGR2, hasAlpha2,
                           frame2->orientation == ccap::FrameOrientation::TopToBottom);

    std::cout << "Frame 2 orientation (requested BottomToTop): "
              << (frame2->orientation == ccap::FrameOrientation::BottomToTop ? "BottomToTop" : "TopToBottom")
              << std::endl;

    provider.stop();
    provider.close();

    // Verify that both frames have same dimensions
    ASSERT_EQ(frame1Width, frame2->width) << "Frame widths should match";
    ASSERT_EQ(frame1Height, frame2->height) << "Frame heights should match";
    ASSERT_EQ(frame1Stride, frame2->stride[0]) << "Frame strides should match";

    // Compare raw frame data - they should be DIFFERENT (one is flipped relative to the other)
    // Calculate similarity between raw data
    uint64_t totalDiff = 0;
    size_t dataSize = frame1Data.size();
    for (size_t i = 0; i < dataSize; ++i) {
        int diff = std::abs(static_cast<int>(frame1Data[i]) - static_cast<int>(frame2Data[i]));
        totalDiff += diff;
    }
    double avgDiff = static_cast<double>(totalDiff) / dataSize;
    double rawSimilarity = 100.0 * (1.0 - avgDiff / 255.0);

    std::cout << "Raw data comparison:" << std::endl;
    std::cout << "  Data size: " << dataSize << " bytes" << std::endl;
    std::cout << "  Similarity: " << rawSimilarity << "%" << std::endl;
    std::cout << "  Saved images for visual verification:" << std::endl;
    std::cout << "    " << file1 << std::endl;
    std::cout << "    " << file2 << std::endl;

    // If FrameOrientation setting works correctly:
    // - The raw data should be DIFFERENT (one is flipped)
    // - But the saved BMP files should look the SAME (both show correct orientation after adjustment)

    if (rawSimilarity > 95.0) {
        // Raw data is nearly identical - orientation setting had no effect on the actual pixel data
        std::cout << "\n[WARNING] Raw frame data is nearly identical (" << rawSimilarity << "% similar)" << std::endl;
        std::cout << "   This indicates that FrameOrientation setting has NO EFFECT on pixel data!" << std::endl;
        std::cout << "   Expected: Different orientations should produce different raw data (flipped)" << std::endl;

        FAIL() << "FrameOrientation setting does not affect pixel data in video playback mode. "
               << "Raw data with different orientation settings should be different, but got "
               << rawSimilarity << "% similarity (expected <95%)";
    } else {
        // Raw data is different - this is expected!
        std::cout << "\n[OK]  SUCCESS: Raw frame data is different (" << rawSimilarity << "% similar)" << std::endl;
        std::cout << "   FrameOrientation setting correctly affects pixel data layout" << std::endl;
    }

    // Clean up (keep files for debugging if test fails)
    if (!::testing::Test::HasFailure()) {
        fs::remove_all(tempDir);
    } else {
        std::cout << "\nTest failed. Keeping output files for debugging in: " << tempDir << std::endl;
    }
}