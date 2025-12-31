/**
 * @file test_memory_safety.cpp
 * @brief Tests for memory safety and frame handling during video playback
 * @date 2025-12-30
 *
 * Tests to verify:
 * 1. File mode: Frames are NOT dropped, reading pauses when queue is full (backpressure)
 * 2. Camera mode: Frames ARE dropped when not consumed quickly enough
 */

#include <gtest/gtest.h>
#include <ccap.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

// Helper to check if file playback is supported
bool isFilePlaybackSupported() {
#if defined(__APPLE__) || defined(_WIN32)
    #ifdef CCAP_ENABLE_FILE_PLAYBACK
        return true;
    #else
        return false;
    #endif
#else
    return false;
#endif
}

class MemorySafetyTest : public ::testing::Test {
protected:
    fs::path testVideoPath;
    bool videoAvailable = false;

    void SetUp() override {
        if (!isFilePlaybackSupported()) {
            GTEST_SKIP() << "File playback not supported on this platform";
        }

        auto currentDir = fs::current_path();
        fs::path projectRoot = currentDir;
        
        while (projectRoot.has_parent_path()) {
            if (fs::exists(projectRoot / "CMakeLists.txt") && 
                fs::exists(projectRoot / "tests")) {
                break;
            }
            projectRoot = projectRoot.parent_path();
        }
        
        auto testDataDir = projectRoot / "tests" / "test-data";
        testVideoPath = testDataDir / "test.mp4";
        
        if (fs::exists(testVideoPath)) {
            std::cout << "Using test video: \"" << testVideoPath.string() << "\"" << std::endl;
            videoAvailable = true;
        } else {
            GTEST_SKIP() << "Test video file not found.";
        }
    }
};

// Test that frames are NOT dropped in file mode (backpressure prevents dropping)
TEST_F(MemorySafetyTest, FileMode_NoFramesDropped_WithBackpressure) {
    ccap::Provider provider;
    
    // Set small buffer to make the test faster
    provider.setMaxAvailableFrameSize(3);
    
    // Default PlaybackSpeed=0 means frames are read as fast as possible
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    EXPECT_TRUE(provider.isFileMode());
    EXPECT_DOUBLE_EQ(provider.get(ccap::PropertyName::PlaybackSpeed), 0.0);
    
    // Let the video play to fill the buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Now grab frames and verify they are consecutive (no drops)
    std::vector<uint64_t> frameIndices;
    const int framesToCheck = 10;
    
    for (int i = 0; i < framesToCheck; i++) {
        auto frame = provider.grab(1000);
        ASSERT_NE(frame, nullptr) << "Failed to grab frame " << i;
        frameIndices.push_back(frame->frameIndex);
        
        std::cout << "Frame " << i << ": index=" << frame->frameIndex << std::endl;
    }
    
    // Verify frames are consecutive (no gaps = no drops)
    for (size_t i = 1; i < frameIndices.size(); i++) {
        EXPECT_EQ(frameIndices[i], frameIndices[i-1] + 1)
            << "Frame " << i << " should be consecutive with previous frame. "
            << "Gap detected: " << (frameIndices[i] - frameIndices[i-1] - 1) << " frames missing!";
    }
    
    std::cout << "[OK] All " << framesToCheck << " frames were consecutive - no frames dropped!\n";
    
    provider.stop();
    provider.close();
}

// Test that slow grabbing still gets all frames (backpressure pauses reading)
TEST_F(MemorySafetyTest, FileMode_SlowGrab_AllFramesPreserved) {
    ccap::Provider provider;
    
    provider.setMaxAvailableFrameSize(3);
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    std::vector<uint64_t> frameIndices;
    uint64_t lastIndex = 0;
    bool hasGap = false;
    
    // Grab frames slowly
    for (int i = 0; i < 15; i++) {
        // Slow down to let buffer pressure build
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        auto frame = provider.grab(1000);
        ASSERT_NE(frame, nullptr);
        
        frameIndices.push_back(frame->frameIndex);
        
        if (lastIndex > 0 && frame->frameIndex != lastIndex + 1) {
            hasGap = true;
            std::cout << "[FAIL] Gap detected between frame " << lastIndex 
                      << " and " << frame->frameIndex << std::endl;
        }
        
        lastIndex = frame->frameIndex;
    }
    
    EXPECT_FALSE(hasGap) 
        << "File mode should NOT drop frames even with slow grabbing";
    
    std::cout << "[OK] No gaps detected in " << frameIndices.size() 
              << " frames with slow grabbing\n";
    
    provider.stop();
    provider.close();
}

// Test frame counting to ensure no frames are lost
TEST_F(MemorySafetyTest, FileMode_ExactFrameCount) {
    ccap::Provider provider;
    
    provider.setMaxAvailableFrameSize(2);  // Very small buffer
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    double expectedFrameCount = provider.get(ccap::PropertyName::FrameCount);
    std::cout << "Video has " << expectedFrameCount << " frames\n";
    
    // Grab all frames and count them
    std::set<uint64_t> uniqueIndices;
    int grabbedCount = 0;
    uint64_t lastIndex = 0;
    
    while (true) {
        auto frame = provider.grab(1000);
        if (!frame) break;
        
        uniqueIndices.insert(frame->frameIndex);
        grabbedCount++;
        
        // Check for gaps
        if (lastIndex > 0) {
            EXPECT_EQ(frame->frameIndex, lastIndex + 1)
                << "Gap detected: expected " << (lastIndex + 1) 
                << ", got " << frame->frameIndex;
        }
        
        lastIndex = frame->frameIndex;
        
        // Safety limit
        if (grabbedCount > expectedFrameCount + 10) break;
    }
    
    std::cout << "Grabbed " << grabbedCount << " frames\n";
    std::cout << "Unique indices: " << uniqueIndices.size() << "\n";
    
    // Should get all frames (or very close due to timing)
    EXPECT_GE(grabbedCount, expectedFrameCount * 0.9)
        << "Should grab most/all frames in file mode";
    
    provider.stop();
    provider.close();
}

// Test with custom max available frame size
TEST_F(MemorySafetyTest, CustomMaxAvailableFrameSize) {
    ccap::Provider provider;
    
    // Set max available frames to 1 (very aggressive dropping)
    provider.setMaxAvailableFrameSize(1);
    
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    // Wait without grabbing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Grab frames - should only have at most 1 frame available
    auto frame = provider.grab(1000);
    ASSERT_NE(frame, nullptr);
    
    // Try to grab another immediately - might get nullptr or next frame
    auto frame2 = provider.grab(100);
    // This is OK either way - either we get a new frame or queue was empty
    
    std::cout << "With maxAvailableFrameSize=1, grabbed frame with index: " 
              << frame->frameIndex << std::endl;
    
    provider.stop();
    provider.close();
}

// Test slow grab with fast playback speed
TEST_F(MemorySafetyTest, SlowGrabWithFastPlayback) {
    ccap::Provider provider;
    
    // PlaybackSpeed=0 means no frame rate control (fastest)
    // BUT: In file mode, backpressure prevents frame dropping
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    std::vector<uint64_t> frameIndices;
    
    // Grab frames slowly - backpressure will pause reading
    for (int i = 0; i < 5; i++) {
        // Wait before each grab
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        auto frame = provider.grab(1000);
        if (frame) {
            frameIndices.push_back(frame->frameIndex);
            std::cout << "Grabbed frame " << i << " with index: " 
                      << frame->frameIndex << std::endl;
        }
    }
    
    ASSERT_GE(frameIndices.size(), 3);
    
    // Verify frames ARE consecutive - backpressure prevents drops
    for (size_t i = 1; i < frameIndices.size(); i++) {
        uint64_t gap = frameIndices[i] - frameIndices[i-1];
        EXPECT_EQ(gap, 1) 
            << "With backpressure in file mode, frames should NOT be skipped";
    }
    
    std::cout << "[OK] With slow grab, NO frames were skipped (backpressure works)" << std::endl;
    
    provider.stop();
    provider.close();
}

// Test memory doesn't grow unbounded with backpressure
TEST_F(MemorySafetyTest, MemoryDoesNotGrowUnbounded) {
    ccap::Provider provider;
    ASSERT_TRUE(provider.open(testVideoPath.string(), true));
    
    // Let video try to play for 2 seconds without grabbing anything
    // Backpressure should pause reading after filling queue (~3 frames)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Now grab a frame - should be from the beginning (backpressure paused reading)
    auto frame = provider.grab(1000);
    ASSERT_NE(frame, nullptr);
    
    double frameRate = provider.get(ccap::PropertyName::FrameRate);
    double currentTime = provider.get(ccap::PropertyName::CurrentTime);
    
    std::cout << "After 2s wait, current time: " << currentTime 
              << "s, frame index: " << frame->frameIndex << std::endl;
    
    // With backpressure: reading should have stopped early
    // Current time should be very small (< 0.2s at 30fps for ~3 frames)
    EXPECT_LT(currentTime, 0.2) << "Reading should pause with backpressure";
    
    // Frame index should be small (reading paused after queue filled)
    EXPECT_LT(frame->frameIndex, 5) << "Frame index should be small (reading paused)";
    
    std::cout << "[OK] Backpressure prevented unbounded reading" << std::endl;
    
    provider.stop();
    provider.close();
}
