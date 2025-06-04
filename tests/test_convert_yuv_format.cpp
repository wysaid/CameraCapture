/**
 * @file test_convert_yuv_format.cpp
 * @brief Unit tests for YUV format conversion functions (NV12/I420 to RGB/BGR)
 */

#include <gtest/gtest.h>
#include "test_utils.h"
#include "ccap_convert.h"

using namespace ccap_test;

class YUVFormatConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        width_ = 64;
        height_ = 64;
        tolerance_ = 3; // YUV conversions may have small rounding differences
    }
    
    void TearDown() override {
        // Cleanup if needed
    }
    
    int width_;
    int height_;
    int tolerance_;
};

// ============ NV12 to RGB/BGR Tests ============

TEST_F(YUVFormatConversionTest, NV12_To_BGR24_BT601_VideoRange) {
    TestYUVImage yuv_img(width_, height_, true); // NV12 format
    TestImage bgr_img(width_, height_, 3);
    
    // Fill with known pattern
    yuv_img.generateKnownPattern();
    
    // Convert NV12 -> BGR24
    ccap::nv12ToBgr24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.uv_data(), yuv_img.uv_stride(),
                      bgr_img.data(), bgr_img.stride(),
                      width_, height_, ccap::ConvertFlag::Default);
    
    // Verify no pixels are out of range
    for (int y = 0; y < height_; ++y) {
        const uint8_t* bgr_row = bgr_img.data() + y * bgr_img.stride();
        for (int x = 0; x < width_; ++x) {
            uint8_t b = bgr_row[x * 3 + 0];
            uint8_t g = bgr_row[x * 3 + 1];
            uint8_t r = bgr_row[x * 3 + 2];
            
            EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
                << "Invalid RGB at (" << x << "," << y << "): RGB(" 
                << static_cast<int>(r) << "," << static_cast<int>(g) << "," 
                << static_cast<int>(b) << ")";
        }
    }
}

TEST_F(YUVFormatConversionTest, NV12_To_RGB24_BT601_VideoRange) {
    TestYUVImage yuv_img(width_, height_, true);
    TestImage rgb_img(width_, height_, 3);
    
    yuv_img.generateGradient();

    ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.uv_data(), yuv_img.uv_stride(),
                      rgb_img.data(), rgb_img.stride(),
                      width_, height_, ccap::ConvertFlag::Default);
    
    // Basic sanity check - first pixel should not be pure black for gradient
    const uint8_t* first_pixel = rgb_img.data();
    bool is_not_black = (first_pixel[0] != 0) || (first_pixel[1] != 0) || (first_pixel[2] != 0);
    EXPECT_TRUE(is_not_black) << "First pixel should not be pure black for gradient";
}

TEST_F(YUVFormatConversionTest, NV12_To_BGRA32_BT709_VideoRange) {
    TestYUVImage yuv_img(width_, height_, true);
    TestImage bgra_img(width_, height_, 4);
    
    yuv_img.fillSolid(128, 128, 128); // Fill with gray
    
    ccap::nv12ToBgra32(yuv_img.y_data(), yuv_img.y_stride(),
                       yuv_img.uv_data(), yuv_img.uv_stride(),
                       bgra_img.data(), bgra_img.stride(),
                       width_, height_, ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange);
    
    // Check that alpha channel is properly set to 255
    for (int y = 0; y < height_; ++y) {
        const uint8_t* bgra_row = bgra_img.data() + y * bgra_img.stride();
        for (int x = 0; x < width_; ++x) {
            uint8_t alpha = bgra_row[x * 4 + 3];
            EXPECT_EQ(alpha, 255) << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

TEST_F(YUVFormatConversionTest, NV12_To_RGBA32_FullRange) {
    TestYUVImage yuv_img(width_, height_, true);
    TestImage rgba_img(width_, height_, 4);
    
    yuv_img.generateKnownPattern();
    
    ccap::nv12ToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                       yuv_img.uv_data(), yuv_img.uv_stride(),
                       rgba_img.data(), rgba_img.stride(),
                       width_, height_, ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange);
    
    // Verify alpha channel and basic color validity
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        for (int x = 0; x < width_; ++x) {
            uint8_t r = rgba_row[x * 4 + 0];
            uint8_t g = rgba_row[x * 4 + 1];
            uint8_t b = rgba_row[x * 4 + 2];
            uint8_t a = rgba_row[x * 4 + 3];
            
            EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b));
            EXPECT_EQ(a, 255);
        }
    }
}

// ============ I420 to RGB/BGR Tests ============

TEST_F(YUVFormatConversionTest, I420_To_BGR24_BT601_VideoRange) {
    TestYUVImage yuv_img(width_, height_, false); // I420 format
    TestImage bgr_img(width_, height_, 3);
    
    yuv_img.generateKnownPattern();
    
    ccap::i420ToBgr24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.u_data(), yuv_img.uv_stride(),
                      yuv_img.v_data(), yuv_img.uv_stride(),
                      bgr_img.data(), bgr_img.stride(),
                      width_, height_, ccap::ConvertFlag::Default);
    
    // Basic validation
    for (int y = 0; y < height_; ++y) {
        const uint8_t* bgr_row = bgr_img.data() + y * bgr_img.stride();
        for (int x = 0; x < width_; ++x) {
            uint8_t b = bgr_row[x * 3 + 0];
            uint8_t g = bgr_row[x * 3 + 1];
            uint8_t r = bgr_row[x * 3 + 2];
            
            EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b));
        }
    }
}

TEST_F(YUVFormatConversionTest, I420_To_RGB24_BT709_FullRange) {
    TestYUVImage yuv_img(width_, height_, false);
    TestImage rgb_img(width_, height_, 3);
    
    yuv_img.generateGradient();
    
    ccap::i420ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.u_data(), yuv_img.uv_stride(),
                      yuv_img.v_data(), yuv_img.uv_stride(),
                      rgb_img.data(), rgb_img.stride(),
                      width_, height_, ccap::ConvertFlag::BT709 | ccap::ConvertFlag::FullRange);
    
    // Basic sanity check - gradient should produce varying colors
    const uint8_t* first_pixel = rgb_img.data();
    const uint8_t* last_pixel = rgb_img.data() + (height_ - 1) * rgb_img.stride() + (width_ - 1) * 3;
    
    bool has_variation = (first_pixel[0] != last_pixel[0]) || 
                        (first_pixel[1] != last_pixel[1]) || 
                        (first_pixel[2] != last_pixel[2]);
    EXPECT_TRUE(has_variation) << "Gradient should produce color variation";
}

TEST_F(YUVFormatConversionTest, I420_To_BGRA32_Consistency) {
    TestYUVImage yuv_img(width_, height_, false);
    TestImage bgra_img(width_, height_, 4);
    
    yuv_img.fillSolid(64, 128, 192); // Specific color
    
    ccap::i420ToBgra32(yuv_img.y_data(), yuv_img.y_stride(),
                       yuv_img.u_data(), yuv_img.uv_stride(),
                       yuv_img.v_data(), yuv_img.uv_stride(),
                       bgra_img.data(), bgra_img.stride(),
                       width_, height_);
    
    // All pixels should be the same since we filled with solid color
    const uint8_t* reference_pixel = bgra_img.data();
    for (int y = 0; y < height_; ++y) {
        const uint8_t* bgra_row = bgra_img.data() + y * bgra_img.stride();
        for (int x = 0; x < width_; ++x) {
            for (int c = 0; c < 4; ++c) {
                EXPECT_EQ(bgra_row[x * 4 + c], reference_pixel[c])
                    << "Inconsistent pixel at (" << x << "," << y << ") channel " << c;
            }
        }
    }
}

TEST_F(YUVFormatConversionTest, I420_To_RGBA32_BT709_VideoRange) {
    TestYUVImage yuv_img(width_, height_, false);
    TestImage rgba_img(width_, height_, 4);
    
    yuv_img.generateKnownPattern();
    
    ccap::i420ToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                       yuv_img.u_data(), yuv_img.uv_stride(),
                       yuv_img.v_data(), yuv_img.uv_stride(),
                       rgba_img.data(), rgba_img.stride(),
                       width_, height_, ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange);
    
    // Validate RGB values and alpha
    for (int y = 0; y < height_; ++y) {
        const uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        for (int x = 0; x < width_; ++x) {
            uint8_t r = rgba_row[x * 4 + 0];
            uint8_t g = rgba_row[x * 4 + 1];
            uint8_t b = rgba_row[x * 4 + 2];
            uint8_t a = rgba_row[x * 4 + 3];
            
            EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b));
            EXPECT_EQ(a, 255);
        }
    }
}

// ============ Edge Case Tests ============

TEST_F(YUVFormatConversionTest, SmallImageSize) {
    const int small_width = 4;
    const int small_height = 4;
    
    TestYUVImage yuv_img(small_width, small_height, true);
    TestImage rgb_img(small_width, small_height, 3);
    
    yuv_img.generateKnownPattern();
    
    // Should not crash with small images
    EXPECT_NO_THROW({
        ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                          yuv_img.uv_data(), yuv_img.uv_stride(),
                          rgb_img.data(), rgb_img.stride(),
                          small_width, small_height);
    });
}

TEST_F(YUVFormatConversionTest, LargeImageSize) {
    const int large_width = 256;
    const int large_height = 256;
    
    TestYUVImage yuv_img(large_width, large_height, false);
    TestImage bgr_img(large_width, large_height, 3);
    
    yuv_img.generateGradient();
    
    // Should handle large images without issues
    EXPECT_NO_THROW({
        ccap::i420ToBgr24(yuv_img.y_data(), yuv_img.y_stride(),
                          yuv_img.u_data(), yuv_img.uv_stride(),
                          yuv_img.v_data(), yuv_img.uv_stride(),
                          bgr_img.data(), bgr_img.stride(),
                          large_width, large_height);
    });
}

// ============ Performance Tests ============

TEST_F(YUVFormatConversionTest, PerformanceBaseline) {
    const int perf_width = 1920;
    const int perf_height = 1080;
    
    TestYUVImage yuv_img(perf_width, perf_height, true);
    TestImage rgb_img(perf_width, perf_height, 3);
    
    yuv_img.generateGradient();
    
    PerformanceTimer timer;
    timer.start();
    
    ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                      yuv_img.uv_data(), yuv_img.uv_stride(),
                      rgb_img.data(), rgb_img.stride(),
                      perf_width, perf_height);
    
    double elapsed_ms = timer.stopAndGetMs();
    
    // Performance should be reasonable (less than 100ms for 1080p on modern hardware)
    EXPECT_LT(elapsed_ms, 100.0) << "Conversion took too long: " << elapsed_ms << "ms";
    
    std::cout << "NV12 to RGB24 conversion (1920x1080): " << elapsed_ms << "ms" << std::endl;
}
