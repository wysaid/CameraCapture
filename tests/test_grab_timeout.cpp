/**
 * @file test_grab_timeout.cpp
 * @brief Tests for Provider::grab timeout behavior
 * 
 * This test suite verifies that the grab() function correctly respects
 * timeout values. Instead of testing absolute timing (which is unreliable
 * in CI environments), we test relative behavior and logical correctness.
 * 
 * Core validation: Before the fix, any timeout < 1000ms would wait at least
 * 1000ms. After the fix, small timeouts should be much faster.
 */

#include "ccap.h"
#include "test_utils.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace ccap;
using namespace ccap_test;

/**
 * @brief Test fixture for grab timeout tests
 * 
 * Uses a Provider with a callback that consumes all frames. This ensures
 * grab() will always timeout since no frames reach the available queue.
 * This is a reliable way to test timeout behavior with a running camera.
 * 
 * Note: In CI environment without cameras, the provider won't start, and
 * these tests will be skipped.
 */
class GrabTimeoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        provider = std::make_unique<Provider>();
        
        // Try to open first available camera
        auto cameras = provider->findDeviceNames();
        if (!cameras.empty()) {
            // Open without auto-start so we can set callback first
            if (provider->open(0, false)) {
                // Set callback to consume all frames (return true = drop frame)
                // This ensures grab() will always timeout
                provider->setNewFrameCallback([](const std::shared_ptr<VideoFrame>&) {
                    return true; // Drop all frames
                });
                
                // Now start the camera
                if (provider->start()) {
                    hasCamera = true;
                    // Wait a bit for camera to start producing frames
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    // Drain any frames that might be in queue before callback was set
                    while (provider->grab(0)) {
                        // Keep draining
                    }
                }
            }
        }
    }

    void TearDown() override {
        if (provider) {
            provider->stop();
        }
        provider.reset();
    }

    /**
     * @brief Helper function to measure actual elapsed time for a grab call
     * 
     * Since callback consumes all frames, grab() will wait until timeout.
     * 
     * @param timeoutMs The timeout value to pass to grab()
     * @return Actual elapsed time in milliseconds
     */
    int64_t measureGrabTime(uint32_t timeoutMs) {
        auto startTime = std::chrono::steady_clock::now();
        auto frame = provider->grab(timeoutMs);
        auto endTime = std::chrono::steady_clock::now();
        
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        return elapsedMs;
    }

    std::unique_ptr<Provider> provider;
    bool hasCamera = false;
};

/**
 * @brief Test that small timeouts are significantly faster than 1000ms
 * 
 * This is the core fix validation: before the fix, any timeout < 1000ms
 * would wait at least 1000ms. After the fix, small timeouts should be
 * much faster. Callback consumes all frames so grab() will always timeout.
 */
TEST_F(GrabTimeoutTest, SmallTimeout_IsMuchFasterThan1000ms) {
    if (!hasCamera) {
        GTEST_SKIP() << "No camera available, skipping test";
    }
    
    const uint32_t smallTimeout = 10;
    const uint32_t largeTimeout = 1000;
    
    int64_t smallElapsed = measureGrabTime(smallTimeout);
    int64_t largeElapsed = measureGrabTime(largeTimeout);
    
    // Core validation: 10ms timeout should be MUCH faster than 1000ms timeout
    // Before the fix, they would both take ~1000ms
    // After the fix, 10ms should be dramatically faster (at least 5x faster)
    EXPECT_LT(smallElapsed, largeElapsed / 5)
        << "10ms timeout took " << smallElapsed << "ms, but 1000ms timeout took " 
        << largeElapsed << "ms. Small timeout should be much faster!";
    
    // Verify timeout is reasonably close to requested value (within 50% margin for CI)
    EXPECT_GT(smallElapsed, smallTimeout / 2)
        << "10ms timeout only took " << smallElapsed << "ms, too short";
    EXPECT_LT(smallElapsed, smallTimeout * 3)
        << "10ms timeout took " << smallElapsed << "ms, too long";
}

/**
 * @brief Test that 500ms timeout is faster than 1000ms timeout
 */
TEST_F(GrabTimeoutTest, MediumTimeout_IsFasterThan1000ms) {
    if (!hasCamera) {
        GTEST_SKIP() << "No camera available, skipping test";
    }
    
    const uint32_t mediumTimeout = 500;
    const uint32_t largeTimeout = 1000;
    
    int64_t mediumElapsed = measureGrabTime(mediumTimeout);
    int64_t largeElapsed = measureGrabTime(largeTimeout);
    
    // 500ms should be noticeably faster than 1000ms
    EXPECT_LT(mediumElapsed, largeElapsed)
        << "500ms timeout took " << mediumElapsed << "ms, but 1000ms timeout took " 
        << largeElapsed << "ms. 500ms should be faster!";
    
    // Verify timeout is reasonably close to requested values
    EXPECT_GT(mediumElapsed, mediumTimeout / 2)
        << "500ms timeout only took " << mediumElapsed << "ms";
    EXPECT_LT(mediumElapsed, mediumTimeout * 2)
        << "500ms timeout took " << mediumElapsed << "ms, too long";
}

/**
 * @brief Test that non-divisible timeouts work correctly
 * 
 * The fix ensures that timeouts not evenly divisible by 1000ms work correctly.
 * We test that 2500ms takes the expected time,
 * validating the loop correctly handles: 1000ms + 1000ms + 500ms = 2500ms
 */
TEST_F(GrabTimeoutTest, NonDivisibleTimeout_WorksCorrectly) {
    if (!hasCamera) {
        GTEST_SKIP() << "No camera available, skipping test";
    }
    
    const uint32_t timeout2500 = 2500;
    
    int64_t elapsed = measureGrabTime(timeout2500);
    
    // Should take at least 2000ms (proves it's not rounding down)
    EXPECT_GT(elapsed, 2000)
        << "2500ms timeout only took " << elapsed << "ms, seems too short";
    
    // Should take less than 3500ms (proves it's not rounding up too much)
    // Generous upper bound for CI variability
    EXPECT_LT(elapsed, 3500)
        << "2500ms timeout took " << elapsed << "ms, way too long";
}

/**
 * @brief Test boundary values behave reasonably
 * 
 * Tests that timeouts around the 1000ms boundary all behave reasonably.
 */
TEST_F(GrabTimeoutTest, BoundaryTimeouts_BehaveReasonably) {
    if (!hasCamera) {
        GTEST_SKIP() << "No camera available, skipping test";
    }
    
    int64_t elapsed1ms = measureGrabTime(1);
    int64_t elapsed999ms = measureGrabTime(999);
    int64_t elapsed1000ms = measureGrabTime(1000);
    int64_t elapsed1001ms = measureGrabTime(1001);
    
    // Before the fix, 1ms would wait 1000ms
    // After fix, should be much faster
    EXPECT_LT(elapsed1ms, 100)
        << "1ms timeout took " << elapsed1ms << "ms, before fix it would take 1000ms";
    
    // All boundary values should be in reasonable ranges relative to their timeout
    EXPECT_GT(elapsed999ms, 500) << "999ms timeout only took " << elapsed999ms << "ms";
    EXPECT_LT(elapsed999ms, 1500) << "999ms timeout took " << elapsed999ms << "ms";
    
    EXPECT_GT(elapsed1000ms, 500) << "1000ms timeout only took " << elapsed1000ms << "ms";
    EXPECT_LT(elapsed1000ms, 1500) << "1000ms timeout took " << elapsed1000ms << "ms";
    
    EXPECT_GT(elapsed1001ms, 500) << "1001ms timeout only took " << elapsed1001ms << "ms";
    EXPECT_LT(elapsed1001ms, 1500) << "1001ms timeout took " << elapsed1001ms << "ms";
    
    // They should all be relatively close to each other around 1000ms
    int64_t maxTime = std::max({elapsed999ms, elapsed1000ms, elapsed1001ms});
    int64_t minTime = std::min({elapsed999ms, elapsed1000ms, elapsed1001ms});
    
    EXPECT_LT(maxTime - minTime, 500)
        << "Times around 1000ms boundary vary too much: "
        << "999ms=" << elapsed999ms << "ms, "
        << "1000ms=" << elapsed1000ms << "ms, "
        << "1001ms=" << elapsed1001ms << "ms";
}

/**
 * @brief Test that 0ms timeout returns immediately
 */
TEST_F(GrabTimeoutTest, ZeroTimeout_ReturnsImmediately) {
    if (!hasCamera) {
        GTEST_SKIP() << "No camera available, skipping test";
    }
    
    int64_t elapsed = measureGrabTime(0);
    
    // Should return almost immediately, much faster than 1000ms
    EXPECT_LT(elapsed, 100)
        << "Zero timeout took " << elapsed << "ms, should be nearly instant";
}
