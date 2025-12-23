/**
 * @file test_grab_timeout.cpp
 * @brief Tests for ProviderImp::grab timeout behavior
 * 
 * This test suite verifies that the grab() function correctly respects
 * timeout values. Instead of testing absolute timing (which is unreliable
 * in CI environments), we test relative behavior and logical correctness.
 * 
 * Core validation: Before the fix, any timeout < 1000ms would wait at least
 * 1000ms. After the fix, small timeouts should be much faster.
 */

#include "ccap_imp.h"
#include "test_utils.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace ccap;
using namespace ccap_test;

/**
 * @brief Mock implementation of ProviderImp for testing grab timeout
 * 
 * This mock implementation simulates a camera that never produces frames,
 * allowing us to test timeout behavior in isolation.
 */
class MockProviderForTimeout : public ProviderImp {
public:
    MockProviderForTimeout() : m_isOpened(false), m_isStarted(false) {}

    std::vector<std::string> findDeviceNames() override {
        return {"MockCamera"};
    }

    bool open(std::string_view deviceName) override {
        m_isOpened = true;
        return true;
    }

    bool isOpened() const override {
        return m_isOpened;
    }

    std::optional<DeviceInfo> getDeviceInfo() const override {
        return DeviceInfo{};
    }

    void close() override {
        m_isOpened = false;
        m_isStarted = false;
    }

    bool start() override {
        m_isStarted = true;
        return true;
    }

    void stop() override {
        m_isStarted = false;
    }

    bool isStarted() const override {
        return m_isStarted;
    }

private:
    bool m_isOpened;
    bool m_isStarted;
};

/**
 * @brief Test fixture for grab timeout tests
 */
class GrabTimeoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        provider = std::make_unique<MockProviderForTimeout>();
        provider->open("MockCamera");
        provider->start();
    }

    void TearDown() override {
        if (provider) {
            provider->stop();
            provider->close();
        }
    }

    /**
     * @brief Helper function to measure actual elapsed time for a grab call
     * 
     * @param timeoutMs The timeout value to pass to grab()
     * @return Actual elapsed time in milliseconds
     */
    int64_t measureGrabTime(uint32_t timeoutMs) {
        auto startTime = std::chrono::steady_clock::now();
        auto frame = provider->grab(timeoutMs);
        auto endTime = std::chrono::steady_clock::now();
        
        // Frame should be null since mock never produces frames
        EXPECT_EQ(frame, nullptr) << "Mock provider should not produce frames";
        
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        return elapsedMs;
    }

    std::unique_ptr<MockProviderForTimeout> provider;
};

/**
 * @brief Test that small timeouts are significantly faster than 1000ms
 * 
 * This is the core fix validation: before the fix, any timeout < 1000ms
 * would wait at least 1000ms. After the fix, small timeouts should be
 * much faster. We test the relative behavior rather than absolute timing.
 */
TEST_F(GrabTimeoutTest, SmallTimeout_IsMuchFasterThan1000ms) {
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
    
    // Sanity check: should still be reasonably close to requested time
    // Very loose bound that works for any CI environment
    EXPECT_LT(smallElapsed, 500) 
        << "10ms timeout took " << smallElapsed << "ms, way too long";
}

/**
 * @brief Test that 500ms timeout is faster than 1000ms timeout
 */
TEST_F(GrabTimeoutTest, MediumTimeout_IsFasterThan1000ms) {
    const uint32_t mediumTimeout = 500;
    const uint32_t largeTimeout = 1000;
    
    int64_t mediumElapsed = measureGrabTime(mediumTimeout);
    int64_t largeElapsed = measureGrabTime(largeTimeout);
    
    // 500ms should be noticeably faster than 1000ms
    EXPECT_LT(mediumElapsed, largeElapsed)
        << "500ms timeout took " << mediumElapsed << "ms, but 1000ms timeout took " 
        << largeElapsed << "ms. 500ms should be faster!";
    
    // Loose upper bound for CI environment
    EXPECT_LT(mediumElapsed, 1000)
        << "500ms timeout took " << mediumElapsed << "ms, should be under 1000ms";
}

/**
 * @brief Test that non-divisible timeouts work correctly
 * 
 * The fix ensures that timeouts not evenly divisible by 1000ms work correctly.
 * We test that 2500ms takes longer than 2000ms but less than 3000ms,
 * validating the loop correctly handles: 1000ms + 1000ms + 500ms = 2500ms
 */
TEST_F(GrabTimeoutTest, NonDivisibleTimeout_WorksCorrectly) {
    const uint32_t timeout2500 = 2500;
    
    int64_t elapsed = measureGrabTime(timeout2500);
    
    // Should take longer than 2000ms (proves it's not rounding down)
    EXPECT_GT(elapsed, 2000)
        << "2500ms timeout only took " << elapsed << "ms, seems too short";
    
    // Should take less than 3500ms (proves it's not rounding up)
    // Generous upper bound for CI variability
    EXPECT_LT(elapsed, 3500)
        << "2500ms timeout took " << elapsed << "ms, way too long";
}

/**
 * @brief Test boundary values behave reasonably
 * 
 * Tests that timeouts around the 1000ms boundary all behave reasonably.
 * We don't test exact timing, just that they're in a reasonable range.
 */
TEST_F(GrabTimeoutTest, BoundaryTimeouts_BehaveReasonably) {
    int64_t elapsed1ms = measureGrabTime(1);
    int64_t elapsed999ms = measureGrabTime(999);
    int64_t elapsed1000ms = measureGrabTime(1000);
    int64_t elapsed1001ms = measureGrabTime(1001);
    
    // Before the fix, 1ms would wait 1000ms
    // After fix, should be much faster
    EXPECT_LT(elapsed1ms, 500)
        << "1ms timeout took " << elapsed1ms << "ms, before fix it would take 1000ms";
    
    // All boundary values should be in reasonable ranges
    EXPECT_LT(elapsed999ms, 1500) << "999ms timeout took " << elapsed999ms << "ms";
    EXPECT_LT(elapsed1000ms, 1500) << "1000ms timeout took " << elapsed1000ms << "ms";
    EXPECT_LT(elapsed1001ms, 1500) << "1001ms timeout took " << elapsed1001ms << "ms";
    
    // They should all be relatively close to each other
    // (not like before where 1ms would take 1000ms but 1000ms would also take 1000ms)
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
    int64_t elapsed = measureGrabTime(0);
    
    // Should return almost immediately, much faster than 1000ms
    EXPECT_LT(elapsed, 100)
        << "Zero timeout took " << elapsed << "ms, should be nearly instant";
}
