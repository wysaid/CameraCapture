/**
 * @file test_grab_timeout.cpp
 * @brief Tests for ProviderImp::grab timeout behavior
 * 
 * This test suite verifies that the grab() function correctly respects
 * timeout values, including those less than 1000ms and values that don't
 * evenly divide by 1000ms.
 * 
 * Test cases:
 * - Timeout values < 1000ms (e.g., 10ms, 500ms)
 * - Timeout values not divisible by 1000ms (e.g., 2500ms, 1750ms)
 * - Boundary values (0ms, 1ms, 999ms, 1000ms, 1001ms)
 */

#include "ccap_imp.h"
#include "test_utils.h"
#include <gtest/gtest.h>
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
 * @brief Test that timeout < 1000ms is respected
 * 
 * Before the fix, a 10ms timeout would wait for at least 1000ms.
 * After the fix, it should return in approximately 10ms.
 */
TEST_F(GrabTimeoutTest, SmallTimeout_10ms) {
    const uint32_t timeoutMs = 10;
    const int64_t tolerance = 100; // Allow 100ms tolerance for CI environment
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    // Should not wait much longer than specified timeout
    EXPECT_LT(elapsedMs, timeoutMs + tolerance) 
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
    
    // Should wait at least the specified timeout (minus small tolerance)
    EXPECT_GT(elapsedMs, timeoutMs - 5)
        << "Timeout of " << timeoutMs << "ms completed too quickly: " << elapsedMs << "ms";
}

/**
 * @brief Test that 500ms timeout is respected
 */
TEST_F(GrabTimeoutTest, SmallTimeout_500ms) {
    const uint32_t timeoutMs = 500;
    const int64_t tolerance = 100;
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    EXPECT_LT(elapsedMs, timeoutMs + tolerance)
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
    EXPECT_GT(elapsedMs, timeoutMs - 5)
        << "Timeout of " << timeoutMs << "ms completed too quickly: " << elapsedMs << "ms";
}

/**
 * @brief Test that non-divisible timeout 2500ms is respected
 * 
 * This should wait: 1000ms + 1000ms + 500ms = 2500ms
 */
TEST_F(GrabTimeoutTest, NonDivisibleTimeout_2500ms) {
    const uint32_t timeoutMs = 2500;
    const int64_t tolerance = 200; // Larger tolerance for longer timeout in CI
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    EXPECT_LT(elapsedMs, timeoutMs + tolerance)
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
    EXPECT_GT(elapsedMs, timeoutMs - 50)
        << "Timeout of " << timeoutMs << "ms completed too quickly: " << elapsedMs << "ms";
}

/**
 * @brief Test that 1750ms timeout is respected
 * 
 * This should wait: 1000ms + 750ms = 1750ms
 */
TEST_F(GrabTimeoutTest, NonDivisibleTimeout_1750ms) {
    const uint32_t timeoutMs = 1750;
    const int64_t tolerance = 150;
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    EXPECT_LT(elapsedMs, timeoutMs + tolerance)
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
    EXPECT_GT(elapsedMs, timeoutMs - 50)
        << "Timeout of " << timeoutMs << "ms completed too quickly: " << elapsedMs << "ms";
}

/**
 * @brief Test boundary case: 1ms timeout
 */
TEST_F(GrabTimeoutTest, BoundaryTimeout_1ms) {
    const uint32_t timeoutMs = 1;
    const int64_t tolerance = 100;
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    // With 1ms timeout, should return very quickly
    EXPECT_LT(elapsedMs, timeoutMs + tolerance)
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
}

/**
 * @brief Test boundary case: 999ms timeout
 */
TEST_F(GrabTimeoutTest, BoundaryTimeout_999ms) {
    const uint32_t timeoutMs = 999;
    const int64_t tolerance = 100;
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    EXPECT_LT(elapsedMs, timeoutMs + tolerance)
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
    EXPECT_GT(elapsedMs, timeoutMs - 5)
        << "Timeout of " << timeoutMs << "ms completed too quickly: " << elapsedMs << "ms";
}

/**
 * @brief Test boundary case: 1000ms timeout
 */
TEST_F(GrabTimeoutTest, BoundaryTimeout_1000ms) {
    const uint32_t timeoutMs = 1000;
    const int64_t tolerance = 100;
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    EXPECT_LT(elapsedMs, timeoutMs + tolerance)
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
    EXPECT_GT(elapsedMs, timeoutMs - 10)
        << "Timeout of " << timeoutMs << "ms completed too quickly: " << elapsedMs << "ms";
}

/**
 * @brief Test boundary case: 1001ms timeout
 */
TEST_F(GrabTimeoutTest, BoundaryTimeout_1001ms) {
    const uint32_t timeoutMs = 1001;
    const int64_t tolerance = 100;
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    EXPECT_LT(elapsedMs, timeoutMs + tolerance)
        << "Timeout of " << timeoutMs << "ms took " << elapsedMs << "ms";
    EXPECT_GT(elapsedMs, timeoutMs - 10)
        << "Timeout of " << timeoutMs << "ms completed too quickly: " << elapsedMs << "ms";
}

/**
 * @brief Test that 0ms timeout returns immediately
 */
TEST_F(GrabTimeoutTest, ZeroTimeout) {
    const uint32_t timeoutMs = 0;
    const int64_t maxTime = 20; // Should return almost immediately
    
    int64_t elapsedMs = measureGrabTime(timeoutMs);
    
    EXPECT_LT(elapsedMs, maxTime)
        << "Zero timeout took " << elapsedMs << "ms, expected < " << maxTime << "ms";
}
