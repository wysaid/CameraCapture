/**
 * @file test_platform_features.cpp
 * @brief Tests for platform-specific features and backend management
 */

#include "ccap_convert.h"
#include "ccap_def.h" // For Allocator class definition
#include "test_backend_manager.h"
#include "test_utils.h"

#include <gtest/gtest.h>

using namespace ccap_test;

// ============ Platform Feature Detection Tests ============

class PlatformFeaturesTest : public ::testing::Test {
protected:
    void SetUp() override {
        original_backend_ = ccap::getConvertBackend();
    }

    void TearDown() override {
        ccap::setConvertBackend(original_backend_);
    }

    ccap::ConvertBackend original_backend_;
};

TEST_F(PlatformFeaturesTest, AVX2_Hardware_Detection) {
    // Test that hasAVX2() doesn't crash and returns consistent results
    EXPECT_NO_THROW({
        bool avx2_support = ccap::hasAVX2();
        std::cout << "[INFO] AVX2 hardware support: " << (avx2_support ? "YES" : "NO") << std::endl;
    });
}

TEST_F(PlatformFeaturesTest, AVX2_Detection_Consistency) {
    bool first_call = ccap::hasAVX2();
    bool second_call = ccap::hasAVX2();

    EXPECT_EQ(first_call, second_call) << "hasAVX2() should return consistent results";
}

TEST_F(PlatformFeaturesTest, AppleAccelerate_Hardware_Detection) {
    // Test that hasAppleAccelerate() doesn't crash and returns consistent results
    EXPECT_NO_THROW({
        bool apple_support = ccap::hasAppleAccelerate();
        std::cout << "[INFO] Apple Accelerate support: " << (apple_support ? "YES" : "NO") << std::endl;
    });
}

TEST_F(PlatformFeaturesTest, AppleAccelerate_Detection_Consistency) {
    bool first_call = ccap::hasAppleAccelerate();
    bool second_call = ccap::hasAppleAccelerate();

    EXPECT_EQ(first_call, second_call) << "hasAppleAccelerate() should return consistent results";
}

// ============ Backend Management Tests ============

TEST_F(PlatformFeaturesTest, Backend_Setting_And_Getting) {
    // Test CPU backend (always supported)
    bool success = ccap::setConvertBackend(ccap::ConvertBackend::CPU);
    EXPECT_TRUE(success) << "CPU backend should always be settable";

    auto current_backend = ccap::getConvertBackend();
    EXPECT_EQ(current_backend, ccap::ConvertBackend::CPU) << "Backend should be CPU";

    // Test AVX2 backend if supported
    if (ccap::hasAVX2()) {
        success = ccap::setConvertBackend(ccap::ConvertBackend::AVX2);
        EXPECT_TRUE(success) << "AVX2 backend should be settable when supported";

        current_backend = ccap::getConvertBackend();
        EXPECT_EQ(current_backend, ccap::ConvertBackend::AVX2) << "Backend should be AVX2";
    }

    // Test Apple Accelerate backend if supported
    if (ccap::hasAppleAccelerate()) {
        success = ccap::setConvertBackend(ccap::ConvertBackend::AppleAccelerate);
        EXPECT_TRUE(success) << "Apple Accelerate backend should be settable when supported";

        current_backend = ccap::getConvertBackend();
        EXPECT_EQ(current_backend, ccap::ConvertBackend::AppleAccelerate) << "Backend should be Apple Accelerate";
    }
}

TEST_F(PlatformFeaturesTest, Backend_Enable_Disable_Functions) {
    // Test AVX2 enable/disable
    if (ccap::hasAVX2()) {
        bool original_avx2_state = ccap::canUseAVX2();

        // Disable AVX2
        bool result = ccap::enableAVX2(false);
        EXPECT_FALSE(ccap::canUseAVX2()) << "AVX2 should be disabled";

        // Re-enable AVX2
        result = ccap::enableAVX2(true);
        if (ccap::hasAVX2()) {
            EXPECT_TRUE(ccap::canUseAVX2()) << "AVX2 should be enabled";
        }
    }

    // Test Apple Accelerate enable/disable
    if (ccap::hasAppleAccelerate()) {
        bool original_apple_state = ccap::canUseAppleAccelerate();

        // Disable Apple Accelerate
        bool result = ccap::enableAppleAccelerate(false);
        EXPECT_FALSE(ccap::canUseAppleAccelerate()) << "Apple Accelerate should be disabled";

        // Re-enable Apple Accelerate
        result = ccap::enableAppleAccelerate(true);
        if (ccap::hasAppleAccelerate()) {
            EXPECT_TRUE(ccap::canUseAppleAccelerate()) << "Apple Accelerate should be enabled";
        }
    }
}

TEST_F(PlatformFeaturesTest, AUTO_Backend_Selection) {
    // Set to AUTO and check what gets selected
    bool success = ccap::setConvertBackend(ccap::ConvertBackend::AUTO);
    EXPECT_TRUE(success) << "AUTO backend should always be settable";

    auto selected_backend = ccap::getConvertBackend();

    // AUTO should resolve to a concrete backend
    EXPECT_NE(selected_backend, ccap::ConvertBackend::AUTO)
        << "AUTO should resolve to a concrete backend";

    // Should be one of the supported backends
    auto supported_backends = BackendTestManager::getSupportedBackends();
    bool is_supported = std::find(supported_backends.begin(), supported_backends.end(), selected_backend) != supported_backends.end();
    EXPECT_TRUE(is_supported) << "Selected backend should be supported";

    std::cout << "[INFO] AUTO backend resolved to: "
              << BackendTestManager::getBackendName(selected_backend) << std::endl;
}

// ============ Backend Functionality Tests ============

class BackendFunctionalityTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
    }
};

TEST_P(BackendFunctionalityTest, Basic_Color_Conversion_Works) {
    auto backend = GetParam();

    TestImage rgba_img(32, 32, 4);
    TestImage bgr_img(32, 32, 3);

    rgba_img.fillSolid(128);

    // This should work on all backends
    ccap::rgbaToBgr(rgba_img.data(), rgba_img.stride(),
                    bgr_img.data(), bgr_img.stride(),
                    32, 32);

    // Verify conversion worked
    const uint8_t* bgr_pixel = bgr_img.data();
    EXPECT_EQ(bgr_pixel[0], 128) << "B value, backend: " << BackendTestManager::getBackendName(backend);
    EXPECT_EQ(bgr_pixel[1], 128) << "G value, backend: " << BackendTestManager::getBackendName(backend);
    EXPECT_EQ(bgr_pixel[2], 128) << "R value, backend: " << BackendTestManager::getBackendName(backend);
}

TEST_P(BackendFunctionalityTest, Basic_YUV_Conversion_Works) {
    auto backend = GetParam();

    TestYUVImage yuv_img(32, 32, true); // NV12
    TestImage rgb_img(32, 32, 3);

    yuv_img.fillSolid(128, 128, 128);

    // This should work on all backends
    ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.uv_data(), yuv_img.uv_stride(),
                      rgb_img.data(), rgb_img.stride(),
                      32, 32, ccap::ConvertFlag::Default);

    // Should produce valid output
    EXPECT_NE(rgb_img.data(), nullptr) << "Backend: " << BackendTestManager::getBackendName(backend);

    // Verify conversion produced some output (not all zeros)
    const uint8_t* rgb_pixel = rgb_img.data();
    bool has_non_zero = (rgb_pixel[0] != 0) || (rgb_pixel[1] != 0) || (rgb_pixel[2] != 0);
    EXPECT_TRUE(has_non_zero) << "Should produce non-zero RGB output, backend: " << BackendTestManager::getBackendName(backend);
}

INSTANTIATE_BACKEND_TEST(BackendFunctionalityTest);

// ============ Memory Management Tests ============

class MemoryManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset allocator state before each test
        ccap::resetSharedAllocator();
    }

    void TearDown() override {
        // Clean up after each test
        ccap::resetSharedAllocator();
    }
};

TEST_F(MemoryManagementTest, GetSharedAllocator_ReturnsValidAllocator) {
    auto allocator = ccap::getSharedAllocator();

    EXPECT_NE(allocator, nullptr) << "Shared allocator should not be null";
    EXPECT_NE(allocator.get(), nullptr) << "Shared allocator pointer should not be null";
}

TEST_F(MemoryManagementTest, GetSharedAllocator_DualInstanceMechanism) {
    // First call should return the first instance
    auto allocator1 = ccap::getSharedAllocator();
    EXPECT_NE(allocator1.get(), nullptr) << "First allocator should not be null";

    // Second call while first is held should return a different instance (second allocator)
    auto allocator2 = ccap::getSharedAllocator();
    EXPECT_NE(allocator2.get(), nullptr) << "Second allocator should not be null";
    EXPECT_NE(allocator1.get(), allocator2.get()) << "Should return different instances when first is held";

    // Release first allocator
    allocator1.reset();

    // Third call should now return the first instance again (since it's available)
    auto allocator3 = ccap::getSharedAllocator();
    EXPECT_NE(allocator3.get(), nullptr) << "Third allocator should not be null";
    // Note: allocator3 could be either the original first instance or the second,
    // depending on implementation details
}

TEST_F(MemoryManagementTest, ResetSharedAllocator_CreatesNewInstance) {
    auto allocator1 = ccap::getSharedAllocator();
    auto original_ptr = allocator1.get();

    ccap::resetSharedAllocator();
    auto allocator2 = ccap::getSharedAllocator();

    // After reset, should get a different allocator instance
    EXPECT_NE(original_ptr, allocator2.get()) << "Should create new allocator after reset";
}

TEST_F(MemoryManagementTest, SharedAllocator_SizeTracking) {
    // Test size tracking without holding allocator reference during conversion
    size_t initial_size, final_size;

    {
        auto allocator = ccap::getSharedAllocator();
        initial_size = allocator->size();
        EXPECT_GE(initial_size, 0) << "Initial allocator size should be non-negative";
        // Release allocator reference before conversion
    }

    // Perform a conversion that might use the shared allocator
    TestYUVImage yuv_img(256, 256, true);
    TestImage rgb_img(256, 256, 3);

    yuv_img.fillSolid(128, 128, 128);

    ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.uv_data(), yuv_img.uv_stride(),
                      rgb_img.data(), rgb_img.stride(),
                      256, 256, ccap::ConvertFlag::Default);

    // Should complete without error
    EXPECT_NE(rgb_img.data(), nullptr) << "Conversion should succeed";

    // Check final allocator size
    {
        auto allocator = ccap::getSharedAllocator();
        final_size = allocator->size();
        EXPECT_GE(final_size, initial_size) << "Allocator size should not decrease";
    }
}

TEST_F(MemoryManagementTest, SharedAllocator_MultipleConversions) {
    // Test multiple conversions using the allocator mechanism properly

    for (int i = 0; i < 5; ++i) {
        // Each iteration uses a fresh allocator reference
        TestYUVImage yuv_img(128, 128, true);
        TestImage rgb_img(128, 128, 3);

        yuv_img.generateRandomYUVImage(128, 128, i * 100);

        // Conversion will internally get and use the shared allocator
        ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                          yuv_img.uv_data(), yuv_img.uv_stride(),
                          rgb_img.data(), rgb_img.stride(),
                          128, 128, ccap::ConvertFlag::Default);

        // Each conversion should succeed
        EXPECT_NE(rgb_img.data(), nullptr) << "Conversion " << i << " should succeed";

        // Verify conversion produced some output (not all zeros)
        const uint8_t* pixel = rgb_img.data();
        bool has_non_zero = (pixel[0] != 0) || (pixel[1] != 0) || (pixel[2] != 0);
        EXPECT_TRUE(has_non_zero) << "Conversion " << i << " should produce non-zero RGB output";
    }
}

TEST_F(MemoryManagementTest, SharedAllocator_ConversionWithHeldAllocator) {
    // Test that demonstrates the dual allocator mechanism
    // Hold one allocator and verify we can get a second different one

    auto held_allocator = ccap::getSharedAllocator();
    EXPECT_NE(held_allocator.get(), nullptr) << "Held allocator should be valid";

    // Get second allocator while first is held - should be different
    auto second_allocator = ccap::getSharedAllocator();
    EXPECT_NE(second_allocator.get(), nullptr) << "Second allocator should be valid";
    EXPECT_NE(held_allocator.get(), second_allocator.get()) << "Should get different allocator instances";

    // At this point:
    // - held_allocator has use_count of 2 (held by held_allocator and internal sharedAllocator)
    // - second_allocator has use_count of 2 (held by second_allocator and internal sharedAllocator2)

    EXPECT_GE(held_allocator.use_count(), 1) << "Held allocator should have use count >= 1";
    EXPECT_GE(second_allocator.use_count(), 1) << "Second allocator should have use count >= 1";

    // Release second allocator
    second_allocator.reset();

    // Now the second allocator slot should be available again
    // held_allocator might still have use_count > 1 due to internal reference
    EXPECT_GE(held_allocator.use_count(), 1) << "Held allocator should still be valid";

    // Test that we can still get an allocator (should reuse the second slot)
    auto third_allocator = ccap::getSharedAllocator();
    EXPECT_NE(third_allocator.get(), nullptr) << "Third allocator should be valid";
}

TEST_F(MemoryManagementTest, SharedAllocator_DualInstanceBehavior) {
    // Test the dual allocator mechanism more thoroughly

    // Get first allocator and hold it
    auto allocator1 = ccap::getSharedAllocator();
    EXPECT_NE(allocator1.get(), nullptr) << "First allocator should be valid";

    // Get second allocator while first is held - should be different
    auto allocator2 = ccap::getSharedAllocator();
    EXPECT_NE(allocator2.get(), nullptr) << "Second allocator should be valid";
    EXPECT_NE(allocator1.get(), allocator2.get()) << "Second allocator should be different from first";

    // At this point, both allocators are held (use_count > 1)
    // A third call should either return one of the existing or potentially trigger an error
    // depending on the implementation. Let's test this carefully.

    // Release the second allocator
    allocator2.reset();

    // Now get a third allocator - should be able to reuse the second slot
    auto allocator3 = ccap::getSharedAllocator();
    EXPECT_NE(allocator3.get(), nullptr) << "Third allocator should be valid";

    // Release first allocator
    allocator1.reset();

    // Now get a fourth allocator - should be able to reuse the first slot
    auto allocator4 = ccap::getSharedAllocator();
    EXPECT_NE(allocator4.get(), nullptr) << "Fourth allocator should be valid";
}

TEST_F(MemoryManagementTest, SharedAllocator_SequentialUsage) {
    // Test sequential usage without holding multiple references
    {
        auto allocator = ccap::getSharedAllocator();
        EXPECT_NE(allocator.get(), nullptr) << "Sequential allocator 1 should be valid";
        // allocator goes out of scope here
    }

    {
        auto allocator = ccap::getSharedAllocator();
        EXPECT_NE(allocator.get(), nullptr) << "Sequential allocator 2 should be valid";
        // allocator goes out of scope here
    }

    {
        auto allocator = ccap::getSharedAllocator();
        EXPECT_NE(allocator.get(), nullptr) << "Sequential allocator 3 should be valid";
        // allocator goes out of scope here
    }

    // All should work fine since we're not holding multiple references simultaneously
}

TEST_F(MemoryManagementTest, SharedAllocator_LargeAllocation) {
    // Test large allocation without holding allocator reference
    size_t initial_size, final_size;

    {
        auto allocator = ccap::getSharedAllocator();
        initial_size = allocator->size();
        // Release allocator reference before large conversion
    }

    // Test with large image that will require allocator expansion
    const int large_width = 1920;
    const int large_height = 1080;

    TestYUVImage yuv_img(large_width, large_height, true);
    TestImage rgb_img(large_width, large_height, 3);

    yuv_img.generateGradient();

    ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.uv_data(), yuv_img.uv_stride(),
                      rgb_img.data(), rgb_img.stride(),
                      large_width, large_height, ccap::ConvertFlag::Default);

    // Conversion should succeed
    EXPECT_NE(rgb_img.data(), nullptr) << "Large image conversion should succeed";

    // Verify conversion produced some output (not all zeros)
    const uint8_t* pixel = rgb_img.data();
    bool has_non_zero = (pixel[0] != 0) || (pixel[1] != 0) || (pixel[2] != 0);
    EXPECT_TRUE(has_non_zero) << "Large image conversion should produce non-zero RGB output";

    // Check that allocator size may have increased
    {
        auto allocator = ccap::getSharedAllocator();
        final_size = allocator->size();
        if (final_size > initial_size) {
            std::cout << "[INFO] Allocator expanded from " << initial_size
                      << " to " << final_size << " bytes for large image" << std::endl;
        }
    }
}

// ============ Memory Allocator Edge Cases ============

class MemoryAllocatorEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        ccap::resetSharedAllocator();
    }

    void TearDown() override {
        ccap::resetSharedAllocator();
    }
};

TEST_F(MemoryAllocatorEdgeCaseTest, AllocatorAfterMultipleResets) {
    // Test allocator behavior after multiple resets
    for (int i = 0; i < 10; ++i) {
        auto allocator = ccap::getSharedAllocator();
        EXPECT_NE(allocator, nullptr) << "Allocator should be valid after reset " << i;

        ccap::resetSharedAllocator();
    }

    // Final allocator should still work
    auto final_allocator = ccap::getSharedAllocator();
    EXPECT_NE(final_allocator, nullptr) << "Final allocator should be valid";
}

TEST_F(MemoryAllocatorEdgeCaseTest, ConversionWithAllocatorManagement) {
    // Test conversions work correctly with allocator lifecycle management
    TestYUVImage yuv_img(64, 64, true);
    TestImage rgb_img(64, 64, 3);

    yuv_img.fillSolid(128, 128, 128);

    // Get and immediately release an allocator
    {
        auto temp_allocator = ccap::getSharedAllocator();
        EXPECT_NE(temp_allocator.get(), nullptr) << "Temp allocator should be valid";
        // temp_allocator goes out of scope here
    }

    // Conversion should still work (will get a fresh allocator internally)
    ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.uv_data(), yuv_img.uv_stride(),
                      rgb_img.data(), rgb_img.stride(),
                      64, 64, ccap::ConvertFlag::Default);

    EXPECT_NE(rgb_img.data(), nullptr) << "Conversion should succeed";

    // Verify conversion produced some output (not all zeros)
    const uint8_t* pixel = rgb_img.data();
    bool has_non_zero = (pixel[0] != 0) || (pixel[1] != 0) || (pixel[2] != 0);
    EXPECT_TRUE(has_non_zero) << "Conversion should produce non-zero RGB output";
}

// Note: This test is disabled by default since it's designed to trigger an abort()
// Uncomment ONLY if you want to test the error condition in a controlled environment
/*
TEST_F(MemoryAllocatorEdgeCaseTest, DISABLED_TooManyAllocators_ShouldAbort) {
    // This test verifies the abort behavior when both allocators are in use
    // WARNING: This test will cause the program to abort if both allocators are held

    auto allocator1 = ccap::getSharedAllocator();
    auto allocator2 = ccap::getSharedAllocator();

    // At this point, both allocators should be held
    // Attempting to get a third one should trigger the abort condition
    // EXPECT_DEATH(ccap::getSharedAllocator(), ".*");
}
*/
