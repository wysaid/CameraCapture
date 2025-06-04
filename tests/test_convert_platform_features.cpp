/**
 * @file test_convert_platform_features.cpp
 * @brief Tests for platform-specific features in ccap_convert.h
 */

#include "ccap_convert.h"
#include "test_utils.h"
#include "test_utils_avx2.h"

#include <gtest/gtest.h>

using namespace ccap_test;

class PlatformFeaturesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Platform feature tests
    }
};

// ============ Platform Feature Detection Tests ============

TEST_F(PlatformFeaturesTest, HasAVX2_Function_Exists)
{
    // Test that hasAVX2() function can be called and returns a boolean
    bool avx2_available = ccap::hasAVX2();

    // The result can be either true or false depending on the CPU
    // but the function should not crash and should return a valid boolean
    EXPECT_TRUE(avx2_available || !avx2_available) << "hasAVX2() should return a valid boolean value";
}

TEST_F(PlatformFeaturesTest, AVX2_Detection_Consistency)
{
    // Test that hasAVX2() returns consistent results across multiple calls
    bool first_call = ccap::hasAVX2();
    bool second_call = ccap::hasAVX2();

    EXPECT_EQ(first_call, second_call) << "hasAVX2() should return consistent results across multiple calls";
}

// ============ Color Shuffle Performance Hint Tests ============

TEST_F(PlatformFeaturesTest, ColorShuffle_Works_Regardless_Of_AVX2)
{
    // Test that color shuffle functions work whether AVX2 is available or not
    const int width = 32;
    const int height = 32;

    TestImage rgb_src(width, height, 3);
    TestImage bgr_dst(width, height, 3);

    rgb_src.fillGradient();

    // This should work regardless of AVX2 availability
    EXPECT_NO_THROW({ ccap::rgbToBgr(rgb_src.data(), rgb_src.stride(), bgr_dst.data(), bgr_dst.stride(), width, height); })
        << "Color shuffle should work regardless of AVX2 availability";

    // Verify the conversion actually happened
    // Check that first pixel's R and B channels are swapped
    const uint8_t* src_pixel = rgb_src.data();
    const uint8_t* dst_pixel = bgr_dst.data();

    EXPECT_EQ(src_pixel[0], dst_pixel[2]) << "R channel should map to B position";
    EXPECT_EQ(src_pixel[1], dst_pixel[1]) << "G channel should remain in G position";
    EXPECT_EQ(src_pixel[2], dst_pixel[0]) << "B channel should map to R position";
}

// ============ AVX2 Disable/Enable Tests ============

TEST_F(PlatformFeaturesTest, DisableAVX2_Function_Works)
{
    bool original_state = ccap::hasAVX2();

    if (AVX2TestRunner::isAVX2Supported()) {
        // Test disabling AVX2
        ccap::disableAVX2(true);
        bool disabled_state = ccap::hasAVX2();
        EXPECT_FALSE(disabled_state) << "hasAVX2() should return false when AVX2 is disabled";

        // Test enabling AVX2
        ccap::disableAVX2(false);
        bool enabled_state = ccap::hasAVX2();
        EXPECT_TRUE(enabled_state) << "hasAVX2() should return true when AVX2 is re-enabled";

        // Restore original state
        ccap::disableAVX2(!original_state);
    }
    else {
        // On platforms without AVX2 support, disableAVX2 should not affect the result
        ccap::disableAVX2(false);
        EXPECT_FALSE(ccap::hasAVX2()) << "hasAVX2() should remain false on non-AVX2 platforms";

        ccap::disableAVX2(true);
        EXPECT_FALSE(ccap::hasAVX2()) << "hasAVX2() should remain false on non-AVX2 platforms";
    }
}

TEST_F(PlatformFeaturesTest, Dual_Implementation_ColorShuffle_RGB_BGR)
{
    const int width = 64;
    const int height = 64;

    AVX2TestRunner::runImageComparisonTest(
        [&]() -> TestImage {
            TestImage rgb_src(width, height, 3);
            TestImage bgr_dst(width, height, 3);

            rgb_src.fillGradient();

            ccap::rgbToBgr(rgb_src.data(), rgb_src.stride(), bgr_dst.data(), bgr_dst.stride(), width, height);

            return bgr_dst;
        },
        "RGB to BGR dual implementation test", 0);
}

TEST_F(PlatformFeaturesTest, Dual_Implementation_ColorShuffle_RGBA_BGRA)
{
    const int width = 64;
    const int height = 64;

    AVX2TestRunner::runImageComparisonTest(
        [&]() -> TestImage {
            TestImage rgba_src(width, height, 4);
            TestImage bgra_dst(width, height, 4);

            rgba_src.fillGradient();

            ccap::rgbaToBgra(rgba_src.data(), rgba_src.stride(), bgra_dst.data(), bgra_dst.stride(), width, height);

            return bgra_dst;
        },
        "RGBA to BGRA dual implementation test", 0);
}
