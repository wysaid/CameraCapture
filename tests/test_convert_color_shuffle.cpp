/**
 * @file test_convert_color_shuffle.cpp
 * @brief Unit tests for color channel shuffle functions in ccap_convert.h
 */

#include "ccap_convert.h"
#include "test_utils.h"
#include "test_utils_avx2.h"

#include <gtest/gtest.h>

using namespace ccap_test;

class ColorShuffleTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Set up common test data
        width_ = 64;
        height_ = 64;
        tolerance_ = 0; // Exact match expected for shuffle operations
    }

    void TearDown() override
    {
        // Cleanup if needed
    }

    int width_;
    int height_;
    int tolerance_;
};

// ============ RGB/BGR Conversion Tests ============

TEST_F(ColorShuffleTest, RGBA_To_BGR_Conversion)
{
    TestImage rgba_img(width_, height_, 4);
    TestImage bgr_img(width_, height_, 3);

    // Create a test pattern: set specific RGBA values
    for (int y = 0; y < height_; ++y) {
        uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        for (int x = 0; x < width_; ++x) {
            rgba_row[x * 4 + 0] = static_cast<uint8_t>(x % 256);       // R
            rgba_row[x * 4 + 1] = static_cast<uint8_t>(y % 256);       // G
            rgba_row[x * 4 + 2] = static_cast<uint8_t>((x + y) % 256); // B
            rgba_row[x * 4 + 3] = 255;                                 // A
        }
    }

    // Convert RGBA -> BGR
    ccap::rgbaToBgr(rgba_img.data(), rgba_img.stride(), bgr_img.data(), bgr_img.stride(), width_, height_);

    // Verify conversion: BGR should be [B, G, R] from original [R, G, B, A]
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        const uint8_t* bgr_row = bgr_img.data() + y * bgr_img.stride();

        for (int x = 0; x < width_; ++x) {
            uint8_t orig_r = rgba_row[x * 4 + 0];
            uint8_t orig_g = rgba_row[x * 4 + 1];
            uint8_t orig_b = rgba_row[x * 4 + 2];

            uint8_t conv_b = bgr_row[x * 3 + 0];
            uint8_t conv_g = bgr_row[x * 3 + 1];
            uint8_t conv_r = bgr_row[x * 3 + 2];

            EXPECT_EQ(orig_r, conv_r) << "Red channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_g, conv_g) << "Green channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_b, conv_b) << "Blue channel mismatch at (" << x << "," << y << ")";
        }
    }
}

TEST_F(ColorShuffleTest, BGRA_To_RGB_Conversion)
{
    TestImage bgra_img(width_, height_, 4);
    TestImage rgb_img(width_, height_, 3);

    // Create test pattern
    bgra_img.fillGradient();

    // Convert BGRA -> RGB (should be equivalent to rgbaToBgr)
    ccap::bgraToRgb(bgra_img.data(), bgra_img.stride(), rgb_img.data(), rgb_img.stride(), width_, height_);

    // Verify: RGB should be [R, G, B] from original [B, G, R, A]
    for (int y = 0; y < height_; ++y) {
        const uint8_t* bgra_row = bgra_img.data() + y * bgra_img.stride();
        const uint8_t* rgb_row = rgb_img.data() + y * rgb_img.stride();

        for (int x = 0; x < width_; ++x) {
            uint8_t orig_b = bgra_row[x * 4 + 0];
            uint8_t orig_g = bgra_row[x * 4 + 1];
            uint8_t orig_r = bgra_row[x * 4 + 2];

            uint8_t conv_r = rgb_row[x * 3 + 0];
            uint8_t conv_g = rgb_row[x * 3 + 1];
            uint8_t conv_b = rgb_row[x * 3 + 2];

            EXPECT_EQ(orig_r, conv_r) << "Red channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_g, conv_g) << "Green channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_b, conv_b) << "Blue channel mismatch at (" << x << "," << y << ")";
        }
    }
}

TEST_F(ColorShuffleTest, RGBA_To_RGB_Conversion)
{
    TestImage rgba_img(width_, height_, 4);
    TestImage rgb_img(width_, height_, 3);

    // Create specific pattern to test alpha removal
    for (int y = 0; y < height_; ++y) {
        uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        for (int x = 0; x < width_; ++x) {
            rgba_row[x * 4 + 0] = static_cast<uint8_t>(x % 256);
            rgba_row[x * 4 + 1] = static_cast<uint8_t>(y % 256);
            rgba_row[x * 4 + 2] = static_cast<uint8_t>((x + y) % 256);
            rgba_row[x * 4 + 3] = 128; // Alpha should be ignored
        }
    }

    ccap::rgbaToRgb(rgba_img.data(), rgba_img.stride(), rgb_img.data(), rgb_img.stride(), width_, height_);

    // Verify RGB channels are preserved, alpha is removed
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        const uint8_t* rgb_row = rgb_img.data() + y * rgb_img.stride();

        for (int x = 0; x < width_; ++x) {
            EXPECT_EQ(rgba_row[x * 4 + 0], rgb_row[x * 3 + 0]) << "R mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(rgba_row[x * 4 + 1], rgb_row[x * 3 + 1]) << "G mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(rgba_row[x * 4 + 2], rgb_row[x * 3 + 2]) << "B mismatch at (" << x << "," << y << ")";
        }
    }
}

TEST_F(ColorShuffleTest, RGB_To_BGRA_Conversion)
{
    TestImage rgb_img(width_, height_, 3);
    TestImage bgra_img(width_, height_, 4);

    rgb_img.fillGradient();

    ccap::rgbToBgra(rgb_img.data(), rgb_img.stride(), bgra_img.data(), bgra_img.stride(), width_, height_);

    // Verify: BGRA should be [B, G, R, 255] from original [R, G, B]
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgb_row = rgb_img.data() + y * rgb_img.stride();
        const uint8_t* bgra_row = bgra_img.data() + y * bgra_img.stride();

        for (int x = 0; x < width_; ++x) {
            uint8_t orig_r = rgb_row[x * 3 + 0];
            uint8_t orig_g = rgb_row[x * 3 + 1];
            uint8_t orig_b = rgb_row[x * 3 + 2];

            uint8_t conv_b = bgra_row[x * 4 + 0];
            uint8_t conv_g = bgra_row[x * 4 + 1];
            uint8_t conv_r = bgra_row[x * 4 + 2];
            uint8_t conv_a = bgra_row[x * 4 + 3];

            EXPECT_EQ(orig_r, conv_r) << "Red channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_g, conv_g) << "Green channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_b, conv_b) << "Blue channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(255, conv_a) << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

TEST_F(ColorShuffleTest, BGR_To_RGBA_Conversion)
{
    TestImage bgr_img(width_, height_, 3);
    TestImage rgba_img(width_, height_, 4);

    bgr_img.fillGradient();

    ccap::bgrToRgba(bgr_img.data(), bgr_img.stride(), rgba_img.data(), rgba_img.stride(), width_, height_);

    // Verify: RGBA should be [R, G, B, 255] from original [B, G, R]
    for (int y = 0; y < height_; ++y) {
        const uint8_t* bgr_row = bgr_img.data() + y * bgr_img.stride();
        const uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();

        for (int x = 0; x < width_; ++x) {
            uint8_t orig_b = bgr_row[x * 3 + 0];
            uint8_t orig_g = bgr_row[x * 3 + 1];
            uint8_t orig_r = bgr_row[x * 3 + 2];

            uint8_t conv_r = rgba_row[x * 4 + 0];
            uint8_t conv_g = rgba_row[x * 4 + 1];
            uint8_t conv_b = rgba_row[x * 4 + 2];
            uint8_t conv_a = rgba_row[x * 4 + 3];

            EXPECT_EQ(orig_r, conv_r) << "Red channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_g, conv_g) << "Green channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(orig_b, conv_b) << "Blue channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(255, conv_a) << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

TEST_F(ColorShuffleTest, RGB_To_RGBA_Conversion)
{
    TestImage rgb_img(width_, height_, 3);
    TestImage rgba_img(width_, height_, 4);

    rgb_img.fillGradient();

    ccap::rgbToRgba(rgb_img.data(), rgb_img.stride(), rgba_img.data(), rgba_img.stride(), width_, height_);

    // Verify: RGBA should be [R, G, B, 255] from original [R, G, B]
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgb_row = rgb_img.data() + y * rgb_img.stride();
        const uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();

        for (int x = 0; x < width_; ++x) {
            EXPECT_EQ(rgb_row[x * 3 + 0], rgba_row[x * 4 + 0]) << "R mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(rgb_row[x * 3 + 1], rgba_row[x * 4 + 1]) << "G mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(rgb_row[x * 3 + 2], rgba_row[x * 4 + 2]) << "B mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(255, rgba_row[x * 4 + 3]) << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

// ============ Roundtrip Tests ============

TEST_F(ColorShuffleTest, RGB_RGBA_Roundtrip)
{
    TestImage original_rgb(width_, height_, 3);
    TestImage rgba_temp(width_, height_, 4);
    TestImage final_rgb(width_, height_, 3);

    original_rgb.fillGradient();

    // RGB -> RGBA -> RGB
    ccap::rgbToRgba(original_rgb.data(), original_rgb.stride(), rgba_temp.data(), rgba_temp.stride(), width_, height_);

    ccap::rgbaToRgb(rgba_temp.data(), rgba_temp.stride(), final_rgb.data(), final_rgb.stride(), width_, height_);

    // Should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(original_rgb.data(), final_rgb.data(), width_, height_, 3, original_rgb.stride(),
                                              final_rgb.stride(), 0))
        << "RGB->RGBA->RGB roundtrip failed";
}

TEST_F(ColorShuffleTest, BGR_BGRA_Roundtrip)
{
    TestImage original_bgr(width_, height_, 3);
    TestImage bgra_temp(width_, height_, 4);
    TestImage final_bgr(width_, height_, 3);

    original_bgr.fillGradient();

    // BGR -> BGRA -> BGR
    ccap::bgrToBgra(original_bgr.data(), original_bgr.stride(), bgra_temp.data(), bgra_temp.stride(), width_, height_);

    ccap::bgra2bgr(bgra_temp.data(), bgra_temp.stride(), final_bgr.data(), final_bgr.stride(), width_, height_);

    // Should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(original_bgr.data(), final_bgr.data(), width_, height_, 3, original_bgr.stride(),
                                              final_bgr.stride(), 0))
        << "BGR->BGRA->BGR roundtrip failed";
}

// ============ Edge Cases ============

class ColorShuffleEdgeCaseTest : public ::testing::TestWithParam<std::pair<int, int>> {
protected:
    void SetUp() override
    {
        auto [w, h] = GetParam();
        width_ = w;
        height_ = h;
    }

    int width_;
    int height_;
};

TEST_P(ColorShuffleEdgeCaseTest, VariousImageSizes)
{
    TestImage rgb_img(width_, height_, 3);
    TestImage bgr_img(width_, height_, 3);

    rgb_img.fillGradient();

    // Test RGB to BGR conversion with various sizes
    ccap::rgbToBgr(rgb_img.data(), rgb_img.stride(), bgr_img.data(), bgr_img.stride(), width_, height_);

    // Verify the shuffle worked correctly
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgb_row = rgb_img.data() + y * rgb_img.stride();
        const uint8_t* bgr_row = bgr_img.data() + y * bgr_img.stride();

        for (int x = 0; x < width_; ++x) {
            EXPECT_EQ(rgb_row[x * 3 + 0], bgr_row[x * 3 + 2]) << "R->B mismatch";
            EXPECT_EQ(rgb_row[x * 3 + 1], bgr_row[x * 3 + 1]) << "G->G mismatch";
            EXPECT_EQ(rgb_row[x * 3 + 2], bgr_row[x * 3 + 0]) << "B->R mismatch";
        }
    }
}

INSTANTIATE_TEST_SUITE_P(ColorShuffleEdgeCase, ColorShuffleEdgeCaseTest, ::testing::ValuesIn(TestDataGenerator::getTestImageSizes()));

// ============ Vertical Flip Tests ============

TEST_F(ColorShuffleTest, Vertical_Flip_RGB_To_BGR)
{
    TestImage rgb_img(width_, height_, 3);
    TestImage bgr_normal(width_, height_, 3);
    TestImage bgr_flipped(width_, height_, 3);

    rgb_img.fillGradient();

    // Normal conversion
    ccap::rgbToBgr(rgb_img.data(), rgb_img.stride(), bgr_normal.data(), bgr_normal.stride(), width_, height_);

    // Flipped conversion (negative height)
    // Only negative height is needed to trigger vertical flip
    ccap::rgbToBgr(rgb_img.data(), rgb_img.stride(), bgr_flipped.data(), bgr_flipped.stride(), width_,
                   -height_); // Negative height triggers flip

    // Verify that flipped version has the same content but vertically mirrored
    for (int y = 0; y < height_; ++y) {
        const uint8_t* normal_row = bgr_normal.data() + y * bgr_normal.stride();
        const uint8_t* flipped_row = bgr_flipped.data() + (height_ - 1 - y) * bgr_flipped.stride();

        for (int x = 0; x < width_; ++x) {
            EXPECT_EQ(normal_row[x * 3 + 0], flipped_row[x * 3 + 0]) << "B channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(normal_row[x * 3 + 1], flipped_row[x * 3 + 1]) << "G channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(normal_row[x * 3 + 2], flipped_row[x * 3 + 2]) << "R channel mismatch at (" << x << "," << y << ")";
        }
    }
}

TEST_F(ColorShuffleTest, Vertical_Flip_RGBA_To_BGRA)
{
    TestImage rgba_img(width_, height_, 4);
    TestImage bgra_normal(width_, height_, 4);
    TestImage bgra_flipped(width_, height_, 4);

    rgba_img.fillGradient();

    // Normal conversion
    ccap::rgbaToBgra(rgba_img.data(), rgba_img.stride(), bgra_normal.data(), bgra_normal.stride(), width_, height_);

    // Flipped conversion
    // Only negative height is needed to trigger vertical flip
    ccap::rgbaToBgra(rgba_img.data(), rgba_img.stride(), bgra_flipped.data(), bgra_flipped.stride(), width_, -height_);

    // Verify vertical flip occurred correctly
    for (int y = 0; y < height_; ++y) {
        const uint8_t* normal_row = bgra_normal.data() + y * bgra_normal.stride();
        const uint8_t* flipped_row = bgra_flipped.data() + (height_ - 1 - y) * bgra_flipped.stride();

        for (int x = 0; x < width_; ++x) {
            for (int c = 0; c < 4; ++c) {
                EXPECT_EQ(normal_row[x * 4 + c], flipped_row[x * 4 + c]) << "Channel " << c << " mismatch at (" << x << "," << y << ")";
            }
        }
    }
}

// ============ Dual Implementation Tests (AVX2 vs CPU) ============

TEST_F(ColorShuffleTest, Dual_Implementation_RGBA_To_BGR)
{
    const int width = 128;
    const int height = 128;

    AVX2TestRunner::runImageComparisonTest(
        [&]() -> TestImage {
            TestImage rgba_src(width, height, 4);
            TestImage bgr_dst(width, height, 3);

            rgba_src.fillGradient();

            ccap::rgbaToBgr(rgba_src.data(), rgba_src.stride(), bgr_dst.data(), bgr_dst.stride(), width, height);

            return bgr_dst;
        },
        "RGBA to BGR dual implementation test", 0);
}

TEST_F(ColorShuffleTest, Dual_Implementation_RGBA_To_BGRA)
{
    const int width = 128;
    const int height = 128;

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

TEST_F(ColorShuffleTest, Dual_Implementation_RGB_To_BGR)
{
    const int width = 128;
    const int height = 128;

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

TEST_F(ColorShuffleTest, Dual_Implementation_BGRA_To_RGB)
{
    const int width = 128;
    const int height = 128;

    AVX2TestRunner::runImageComparisonTest(
        [&]() -> TestImage {
            TestImage bgra_src(width, height, 4);
            TestImage rgb_dst(width, height, 3);

            bgra_src.fillGradient();

            ccap::bgraToRgb(bgra_src.data(), bgra_src.stride(), rgb_dst.data(), rgb_dst.stride(), width, height);

            return rgb_dst;
        },
        "BGRA to RGB dual implementation test", 0);
}

TEST_F(ColorShuffleTest, Dual_Implementation_Vertical_Flip)
{
    const int width = 64;
    const int height = 64;

    AVX2TestRunner::runImageComparisonTest(
        [&]() -> TestImage {
            TestImage rgba_src(width, height, 4);
            TestImage bgra_dst(width, height, 4);

            rgba_src.fillGradient();

            // Test vertical flip with negative height
            // Only negative height is needed to trigger vertical flip
            ccap::rgbaToBgra(rgba_src.data(), rgba_src.stride(), bgra_dst.data(), bgra_dst.stride(), width, -height);

            return bgra_dst;
        },
        "Vertical flip dual implementation test", 0);
}
