/**
 * @file test_accuracy.cpp
 * @brief Accuracy tests for ccap_convert functions against reference implementations
 */

#include "ccap_convert.h"
#include "test_utils.h"

#include <cmath>
#include <gtest/gtest.h>

using namespace ccap_test;

class AccuracyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        width_ = 64;
        height_ = 64;
        tolerance_ = 2; // Allow small differences due to different rounding
    }

    void TearDown() override {}

    int width_;
    int height_;
    int tolerance_;
};

// ============ YUV to RGB Accuracy Tests ============

TEST_F(AccuracyTest, YUVToRGB_AgainstReference)
{
    // Test our YUV to RGB functions against reference implementations
    auto test_values = TestDataGenerator::getTestYUVValues();

    for (const auto& yuv : test_values) {
        int y = std::get<0>(yuv);
        int u = std::get<1>(yuv);
        int v = std::get<2>(yuv);

        // Test BT.601 Video Range
        {
            int r_ref, g_ref, b_ref;
            int r_impl, g_impl, b_impl;

            PixelTestUtils::yuv2rgbReference(y, u, v, r_ref, g_ref, b_ref, false, false);
            ccap::yuv2rgb601v(y, u, v, r_impl, g_impl, b_impl);

            EXPECT_NEAR(r_impl, r_ref, tolerance_) << "BT601v Red mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(g_impl, g_ref, tolerance_) << "BT601v Green mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(b_impl, b_ref, tolerance_) << "BT601v Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
        }

        // Test BT.709 Video Range
        {
            int r_ref, g_ref, b_ref;
            int r_impl, g_impl, b_impl;

            PixelTestUtils::yuv2rgbReference(y, u, v, r_ref, g_ref, b_ref, true, false);
            ccap::yuv2rgb709v(y, u, v, r_impl, g_impl, b_impl);

            EXPECT_NEAR(r_impl, r_ref, tolerance_) << "BT709v Red mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(g_impl, g_ref, tolerance_) << "BT709v Green mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(b_impl, b_ref, tolerance_) << "BT709v Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
        }

        // Test BT.601 Full Range
        {
            int r_ref, g_ref, b_ref;
            int r_impl, g_impl, b_impl;

            PixelTestUtils::yuv2rgbReference(y, u, v, r_ref, g_ref, b_ref, false, true);
            ccap::yuv2rgb601f(y, u, v, r_impl, g_impl, b_impl);

            EXPECT_NEAR(r_impl, r_ref, tolerance_) << "BT601f Red mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(g_impl, g_ref, tolerance_) << "BT601f Green mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(b_impl, b_ref, tolerance_) << "BT601f Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
        }

        // Test BT.709 Full Range
        {
            int r_ref, g_ref, b_ref;
            int r_impl, g_impl, b_impl;

            PixelTestUtils::yuv2rgbReference(y, u, v, r_ref, g_ref, b_ref, true, true);
            ccap::yuv2rgb709f(y, u, v, r_impl, g_impl, b_impl);

            EXPECT_NEAR(r_impl, r_ref, tolerance_) << "BT709f Red mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(g_impl, g_ref, tolerance_) << "BT709f Green mismatch for YUV(" << y << "," << u << "," << v << ")";
            EXPECT_NEAR(b_impl, b_ref, tolerance_) << "BT709f Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
        }
    }
}

// ============ Round-trip Accuracy Tests ============

TEST_F(AccuracyTest, ColorShuffle_Roundtrip)
{
    TestImage original(width_, height_, 4);
    TestImage intermediate(width_, height_, 4);
    TestImage result(width_, height_, 4);

    original.fillGradient();

    // Apply shuffle and reverse shuffle

    ccap::bgraToRgba(original.data(), original.stride(), intermediate.data(), intermediate.stride(), width_, height_);

    ccap::rgbaToBgra(intermediate.data(), intermediate.stride(), result.data(), result.stride(), width_, height_);

    // Should be identical to original
    EXPECT_TRUE(PixelTestUtils::compareImages(original.data(), result.data(), width_, height_, 4, original.stride(), result.stride(), 0))
        << "Round-trip shuffle should preserve original data";
}

TEST_F(AccuracyTest, RGB_RGBA_Roundtrip)
{
    TestImage rgb_original(width_, height_, 3);
    TestImage rgba_intermediate(width_, height_, 4);
    TestImage rgb_result(width_, height_, 3);

    rgb_original.fillRandom(42);

    // RGB -> RGBA -> RGB
    ccap::rgbToRgba(rgb_original.data(), rgb_original.stride(), rgba_intermediate.data(), rgba_intermediate.stride(), width_, height_);

    ccap::rgbaToRgb(rgba_intermediate.data(), rgba_intermediate.stride(), rgb_result.data(), rgb_result.stride(), width_, height_);

    // Should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(rgb_original.data(), rgb_result.data(), width_, height_, 3, rgb_original.stride(),
                                              rgb_result.stride(), 0))
        << "RGB->RGBA->RGB round-trip should preserve data";

    // Check that alpha was properly set
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgba_row = rgba_intermediate.data() + y * rgba_intermediate.stride();
        for (int x = 0; x < width_; ++x) {
            EXPECT_EQ(rgba_row[x * 4 + 3], 255) << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

// ============ Consistency Tests ============

TEST_F(AccuracyTest, NV12_I420_Consistency)
{
    // Test that NV12 and I420 with same data produce same RGB output
    TestYUVImage nv12_img(width_, height_, true);
    TestYUVImage i420_img(width_, height_, false);

    // Fill both with same YUV values
    nv12_img.fillSolid(128, 100, 150);
    i420_img.fillSolid(128, 100, 150);

    TestImage rgb_from_nv12(width_, height_, 3);
    TestImage rgb_from_i420(width_, height_, 3);

    // Convert both to RGB
    ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_from_nv12.data(),
                      rgb_from_nv12.stride(), width_, height_);

    ccap::i420ToRgb24(i420_img.y_data(), i420_img.y_stride(), i420_img.u_data(), i420_img.uv_stride(), i420_img.v_data(),
                      i420_img.uv_stride(), rgb_from_i420.data(), rgb_from_i420.stride(), width_, height_);

    // Results should be very similar (allowing for minor implementation differences)
    EXPECT_TRUE(PixelTestUtils::compareImages(rgb_from_nv12.data(), rgb_from_i420.data(), width_, height_, 3, rgb_from_nv12.stride(),
                                              rgb_from_i420.stride(), 1))
        << "NV12 and I420 with same data should produce similar RGB results";
}

TEST_F(AccuracyTest, RGB_BGR_Relationship)
{
    TestImage rgba_src(width_, height_, 4);
    TestImage rgb_dst(width_, height_, 3);
    TestImage bgr_dst(width_, height_, 3);

    rgba_src.fillGradient();

    // Convert to both RGB and BGR
    ccap::rgbaToRgb(rgba_src.data(), rgba_src.stride(), rgb_dst.data(), rgb_dst.stride(), width_, height_);

    ccap::rgbaToBgr(rgba_src.data(), rgba_src.stride(), bgr_dst.data(), bgr_dst.stride(), width_, height_);

    // Verify the channel relationship
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgb_row = rgb_dst.data() + y * rgb_dst.stride();
        const uint8_t* bgr_row = bgr_dst.data() + y * bgr_dst.stride();

        for (int x = 0; x < width_; ++x) {
            EXPECT_EQ(rgb_row[x * 3 + 0], bgr_row[x * 3 + 2]) // R == B
                << "Red/Blue channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(rgb_row[x * 3 + 1], bgr_row[x * 3 + 1]) // G == G
                << "Green channel mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(rgb_row[x * 3 + 2], bgr_row[x * 3 + 0]) // B == R
                << "Blue/Red channel mismatch at (" << x << "," << y << ")";
        }
    }
}

// ============ Boundary Value Tests ============

TEST_F(AccuracyTest, BoundaryValues)
{
    // Test extreme YUV values to ensure no overflow/underflow
    struct BoundaryTest {
        int y, u, v;
        std::string description;
    };

    std::vector<BoundaryTest> boundary_tests = {
        { 0, 0, 0, "All minimum" },
        { 255, 255, 255, "All maximum" },
        { 0, 128, 128, "Y min, UV neutral" },
        { 255, 128, 128, "Y max, UV neutral" },
        { 128, 0, 255, "Y neutral, UV extremes" },
        { 128, 255, 0, "Y neutral, UV extremes swapped" },
    };

    for (const auto& test : boundary_tests) {
        int r, g, b;

        ccap::yuv2rgb601v(test.y, test.u, test.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT601v boundary test failed for " << test.description << " YUV(" << test.y << "," << test.u << "," << test.v << ")"
            << " -> RGB(" << r << "," << g << "," << b << ")";

        ccap::yuv2rgb709v(test.y, test.u, test.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b)) << "BT709v boundary test failed for " << test.description;

        ccap::yuv2rgb601f(test.y, test.u, test.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b)) << "BT601f boundary test failed for " << test.description;

        ccap::yuv2rgb709f(test.y, test.u, test.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b)) << "BT709f boundary test failed for " << test.description;
    }
}

// ============ Statistical Accuracy Tests ============

TEST_F(AccuracyTest, StatisticalAccuracy)
{
    const int large_width = 256;
    const int large_height = 256;

    TestYUVImage yuv_img(large_width, large_height, true);
    TestImage rgb_dst(large_width, large_height, 3);
    TestImage reference_rgb(large_width, large_height, 3);

    yuv_img.generateRandomYUVImage(large_width, large_height, 12345);

    // Convert using our implementation
    ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(), yuv_img.uv_data(), yuv_img.uv_stride(), rgb_dst.data(), rgb_dst.stride(),
                      large_width, large_height);

    // Create reference using pixel-by-pixel conversion
    for (int y = 0; y < large_height; ++y) {
        const uint8_t* y_row = yuv_img.y_data() + y * yuv_img.y_stride();
        const uint8_t* uv_row = yuv_img.uv_data() + (y / 2) * yuv_img.uv_stride();
        uint8_t* rgb_row = reference_rgb.data() + y * reference_rgb.stride();

        for (int x = 0; x < large_width; ++x) {
            uint8_t Y = y_row[x];
            uint8_t U = uv_row[(x / 2) * 2 + 0];
            uint8_t V = uv_row[(x / 2) * 2 + 1];

            int r, g, b;
            PixelTestUtils::yuv2rgbReference(Y, U, V, r, g, b, false, false);

            rgb_row[x * 3 + 0] = static_cast<uint8_t>(r);
            rgb_row[x * 3 + 1] = static_cast<uint8_t>(g);
            rgb_row[x * 3 + 2] = static_cast<uint8_t>(b);
        }
    }

    // Calculate MSE and PSNR
    double mse = PixelTestUtils::calculateMSE(rgb_dst.data(), reference_rgb.data(), large_width, large_height, 3, rgb_dst.stride(),
                                              reference_rgb.stride());

    double psnr = PixelTestUtils::calculatePSNR(rgb_dst.data(), reference_rgb.data(), large_width, large_height, 3, rgb_dst.stride(),
                                                reference_rgb.stride());

    std::cout << "[ACCURACY] MSE: " << mse << std::endl;
    std::cout << "[ACCURACY] PSNR: " << psnr << " dB" << std::endl;

    // Quality thresholds
    EXPECT_LT(mse, 4.0) << "MSE should be very low (< 4.0)";
    EXPECT_GT(psnr, 40.0) << "PSNR should be high (> 40 dB)";

    // Most pixels should be within tolerance
    int pixels_within_tolerance = 0;
    int total_pixels = large_width * large_height;

    for (int y = 0; y < large_height; ++y) {
        const uint8_t* impl_row = rgb_dst.data() + y * rgb_dst.stride();
        const uint8_t* ref_row = reference_rgb.data() + y * reference_rgb.stride();

        for (int x = 0; x < large_width; ++x) {
            bool within_tolerance = true;
            for (int c = 0; c < 3; ++c) {
                int diff = std::abs(static_cast<int>(impl_row[x * 3 + c]) - static_cast<int>(ref_row[x * 3 + c]));
                if (diff > tolerance_) {
                    within_tolerance = false;
                    break;
                }
            }
            if (within_tolerance) {
                pixels_within_tolerance++;
            }
        }
    }

    double accuracy_percentage = 100.0 * pixels_within_tolerance / total_pixels;
    std::cout << "[ACCURACY] Pixels within tolerance: " << accuracy_percentage << "%" << std::endl;

    EXPECT_GT(accuracy_percentage, 95.0) << "At least 95% of pixels should be within tolerance";
}
