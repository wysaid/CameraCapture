/**
 * @file test_convert_comprehensive.cpp
 * @brief Comprehensive unit tests for all ccap_convert.h functionality
 */

#include "ccap_convert.h"
#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>

using namespace ccap_test;

class ComprehensiveConvertTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        width_ = 32;
        height_ = 32;
    }

    int width_;
    int height_;
};

// ============ Specific Conversion Function Tests ============

TEST_F(ComprehensiveConvertTest, AllPredefinedConversions)
{
    // Test all the predefined conversion functions

    // RGBA to BGR
    {
        TestImage rgba(width_, height_, 4);
        TestImage bgr(width_, height_, 3);
        rgba.fillChecker(50, 200);

        ccap::rgbaToBgr(rgba.data(), rgba.stride(),
                        bgr.data(), bgr.stride(),
                        width_, height_);

        // Verify conversion
        EXPECT_EQ(rgba.data()[0], bgr.data()[2]); // R->B
        EXPECT_EQ(rgba.data()[1], bgr.data()[1]); // G->G
        EXPECT_EQ(rgba.data()[2], bgr.data()[0]); // B->R
    }

    // RGBA to RGB
    {
        TestImage rgba(width_, height_, 4);
        TestImage rgb(width_, height_, 3);
        rgba.fillGradient();

        ccap::rgbaToRgb(rgba.data(), rgba.stride(),
                        rgb.data(), rgb.stride(),
                        width_, height_);

        // Should preserve RGB order, drop alpha
        EXPECT_EQ(rgba.data()[0], rgb.data()[0]); // R->R
        EXPECT_EQ(rgba.data()[1], rgb.data()[1]); // G->G
        EXPECT_EQ(rgba.data()[2], rgb.data()[2]); // B->B
    }

    // RGB to BGRA
    {
        TestImage rgb(width_, height_, 3);
        TestImage bgra(width_, height_, 4);
        rgb.fillSolid(128);

        ccap::rgbToBgra(rgb.data(), rgb.stride(),
                        bgra.data(), bgra.stride(),
                        width_, height_);

        // Check color swap and alpha addition
        EXPECT_EQ(rgb.data()[0], bgra.data()[2]); // R->B
        EXPECT_EQ(rgb.data()[1], bgra.data()[1]); // G->G
        EXPECT_EQ(rgb.data()[2], bgra.data()[0]); // B->R
        EXPECT_EQ(bgra.data()[3], 255);           // Alpha should be 255
    }

    // RGB to RGBA
    {
        TestImage rgb(width_, height_, 3);
        TestImage rgba(width_, height_, 4);
        rgb.fillRandom();

        ccap::rgbToRgba(rgb.data(), rgb.stride(),
                        rgba.data(), rgba.stride(),
                        width_, height_);

        // Should preserve RGB order, add alpha
        EXPECT_EQ(rgb.data()[0], rgba.data()[0]); // R->R
        EXPECT_EQ(rgb.data()[1], rgba.data()[1]); // G->G
        EXPECT_EQ(rgb.data()[2], rgba.data()[2]); // B->B
        EXPECT_EQ(rgba.data()[3], 255);           // Alpha should be 255
    }
}

// ============ Alias Function Tests ============

TEST_F(ComprehensiveConvertTest, FunctionAliases)
{
    TestImage src(width_, height_, 4);
    TestImage dst1(width_, height_, 3);
    TestImage dst2(width_, height_, 3);

    src.fillGradient();

    // Test that aliases produce the same result as original functions
    ccap::rgbaToBgr(src.data(), src.stride(),
                    dst1.data(), dst1.stride(),
                    width_, height_);

    ccap::bgraToRgb(src.data(), src.stride(),
                    dst2.data(), dst2.stride(),
                    width_, height_);

    // Should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(
        dst1.data(), dst2.data(),
        width_, height_, 3,
        dst1.stride(), dst2.stride(), 0));

    // Test bgra2bgr alias
    TestImage dst3(width_, height_, 3);
    ccap::bgra2bgr(src.data(), src.stride(),
                   dst3.data(), dst3.stride(),
                   width_, height_);

    EXPECT_TRUE(PixelTestUtils::compareImages(
        dst1.data(), dst3.data(),
        width_, height_, 3,
        dst1.stride(), dst3.stride(), 0));
}

// ============ YUV Single Pixel Conversion Tests ============

TEST_F(ComprehensiveConvertTest, YUVPixelConversions_EdgeCases)
{
    struct TestCase
    {
        int y, u, v;
        std::string description;
    };

    std::vector<TestCase> edge_cases = {
        { 0, 0, 0, "Min values" },
        { 255, 255, 255, "Max values" },
        { 16, 128, 128, "Video black" },
        { 235, 128, 128, "Video white" },
        { 0, 128, 128, "Full black" },
        { 255, 128, 128, "Full white" },
        { 128, 0, 0, "Min UV" },
        { 128, 255, 255, "Max UV" },
    };

    for (const auto& test_case : edge_cases)
    {
        int r, g, b;

        // Test all YUV conversion functions
        ccap::yuv2rgb601v(test_case.y, test_case.u, test_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT601 video range failed for " << test_case.description;

        ccap::yuv2rgb709v(test_case.y, test_case.u, test_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT709 video range failed for " << test_case.description;

        ccap::yuv2rgb601f(test_case.y, test_case.u, test_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT601 full range failed for " << test_case.description;

        ccap::yuv2rgb709f(test_case.y, test_case.u, test_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT709 full range failed for " << test_case.description;
    }
}

// ============ ConvertFlag Enum Tests ============

TEST_F(ComprehensiveConvertTest, ConvertFlagOperations)
{
    using ccap::ConvertFlag;

    // Test bitwise operations
    ConvertFlag combined = ConvertFlag::BT709 | ConvertFlag::FullRange;

    EXPECT_TRUE(combined & ConvertFlag::BT709);
    EXPECT_TRUE(combined & ConvertFlag::FullRange);
    EXPECT_FALSE(combined & ConvertFlag::BT601);
    EXPECT_FALSE(combined & ConvertFlag::VideoRange);

    // Test default flag
    ConvertFlag default_flag = ConvertFlag::Default;
    EXPECT_TRUE(default_flag & ConvertFlag::BT601);
    EXPECT_TRUE(default_flag & ConvertFlag::VideoRange);
    EXPECT_FALSE(default_flag & ConvertFlag::BT709);
    EXPECT_FALSE(default_flag & ConvertFlag::FullRange);
}

// ============ Memory Alignment Tests ============

TEST_F(ComprehensiveConvertTest, MemoryAlignment)
{
    // Test that functions work with various stride alignments
    const int test_width = 17; // Odd width to test alignment
    const int test_height = 13;

    // Test with different stride alignments
    for (int alignment : { 1, 4, 16, 32 })
    {
        TestImage src(test_width, test_height, 3, alignment);
        TestImage dst(test_width, test_height, 4, alignment);

        src.fillRandom(42);

        EXPECT_NO_THROW({
            ccap::rgbToRgba(src.data(), src.stride(),
                            dst.data(), dst.stride(),
                            test_width, test_height);
        }) << "Failed with alignment "
           << alignment;

        // Basic verification
        EXPECT_EQ(dst.data()[3], 255) << "Alpha not set with alignment " << alignment;
    }
}
