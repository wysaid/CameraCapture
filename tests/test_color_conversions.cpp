/**
 * @file test_color_conversions.cpp
 * @brief Comprehensive tests for color format conversions with explicit backend specification
 */

#include "ccap_convert.h"
#include "test_utils.h"
#include "test_backend_manager.h"
#include <gtest/gtest.h>

using namespace ccap_test;

// ============ Color Shuffle Tests (Backend-Specific) ============

class ColorShuffleBackendTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
        width_ = 64;
        height_ = 64;
    }

    int width_;
    int height_;
};

TEST_P(ColorShuffleBackendTest, RGBA_To_BGRA_Conversion) {
    auto backend = GetParam();
    
    TestImage rgba_img(width_, height_, 4);
    TestImage bgra_img(width_, height_, 4);
    
    // Create test pattern
    rgba_img.fillGradient();
    
    // Convert RGBA -> BGRA
    ccap::rgbaToBgra(rgba_img.data(), rgba_img.stride(), 
                     bgra_img.data(), bgra_img.stride(), 
                     width_, height_);
    
    // Verify conversion: BGRA should be [B, G, R, A] from original [R, G, B, A]
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const uint8_t* rgba_pixel = rgba_img.data() + y * rgba_img.stride() + x * 4;
            const uint8_t* bgra_pixel = bgra_img.data() + y * bgra_img.stride() + x * 4;
            
            EXPECT_EQ(rgba_pixel[0], bgra_pixel[2]) << "R->R mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(rgba_pixel[1], bgra_pixel[1]) << "G->G mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(rgba_pixel[2], bgra_pixel[0]) << "B->B mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(rgba_pixel[3], bgra_pixel[3]) << "A->A mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
        }
    }
}

TEST_P(ColorShuffleBackendTest, RGB_To_BGR_Conversion) {
    auto backend = GetParam();
    
    TestImage rgb_img(width_, height_, 3);
    TestImage bgr_img(width_, height_, 3);
    
    rgb_img.fillRandom(42);
    
    ccap::rgbToBgr(rgb_img.data(), rgb_img.stride(), 
                   bgr_img.data(), bgr_img.stride(), 
                   width_, height_);
    
    // Verify channel swap for first few pixels
    for (int i = 0; i < 5; ++i) {
        const uint8_t* rgb_pixel = rgb_img.data() + i * 3;
        const uint8_t* bgr_pixel = bgr_img.data() + i * 3;
        
        EXPECT_EQ(rgb_pixel[0], bgr_pixel[2]) << "R->B mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgb_pixel[1], bgr_pixel[1]) << "G->G mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgb_pixel[2], bgr_pixel[0]) << "B->R mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
    }
}

TEST_P(ColorShuffleBackendTest, RGBA_To_BGR_Conversion) {
    auto backend = GetParam();
    
    TestImage rgba_img(width_, height_, 4);
    TestImage bgr_img(width_, height_, 3);
    
    rgba_img.fillChecker(100, 200);
    
    ccap::rgbaToBgr(rgba_img.data(), rgba_img.stride(), 
                    bgr_img.data(), bgr_img.stride(), 
                    width_, height_);
    
    // Verify conversion and alpha removal
    for (int i = 0; i < 3; ++i) {
        const uint8_t* rgba_pixel = rgba_img.data() + i * 4;
        const uint8_t* bgr_pixel = bgr_img.data() + i * 3;
        
        EXPECT_EQ(rgba_pixel[0], bgr_pixel[2]) << "R->B mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgba_pixel[1], bgr_pixel[1]) << "G->G mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgba_pixel[2], bgr_pixel[0]) << "B->R mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
    }
}

TEST_P(ColorShuffleBackendTest, RGBA_To_RGB_Conversion) {
    auto backend = GetParam();
    
    TestImage rgba_img(width_, height_, 4);
    TestImage rgb_img(width_, height_, 3);
    
    rgba_img.fillGradient();
    
    ccap::rgbaToRgb(rgba_img.data(), rgba_img.stride(), 
                    rgb_img.data(), rgb_img.stride(), 
                    width_, height_);
    
    // Verify alpha removal but channel order preserved
    for (int i = 0; i < 3; ++i) {
        const uint8_t* rgba_pixel = rgba_img.data() + i * 4;
        const uint8_t* rgb_pixel = rgb_img.data() + i * 3;
        
        EXPECT_EQ(rgba_pixel[0], rgb_pixel[0]) << "R->R mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgba_pixel[1], rgb_pixel[1]) << "G->G mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgba_pixel[2], rgb_pixel[2]) << "B->B mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
    }
}

TEST_P(ColorShuffleBackendTest, RGB_To_BGRA_Conversion) {
    auto backend = GetParam();
    
    TestImage rgb_img(width_, height_, 3);
    TestImage bgra_img(width_, height_, 4);
    
    rgb_img.fillSolid(128);
    
    ccap::rgbToBgra(rgb_img.data(), rgb_img.stride(), 
                    bgra_img.data(), bgra_img.stride(), 
                    width_, height_);
    
    // Check color swap and alpha addition
    for (int i = 0; i < 3; ++i) {
        const uint8_t* rgb_pixel = rgb_img.data() + i * 3;
        const uint8_t* bgra_pixel = bgra_img.data() + i * 4;
        
        EXPECT_EQ(rgb_pixel[0], bgra_pixel[2]) << "R->R mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgb_pixel[1], bgra_pixel[1]) << "G->G mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgb_pixel[2], bgra_pixel[0]) << "B->B mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(bgra_pixel[3], 255) << "Alpha should be 255 at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
    }
}

TEST_P(ColorShuffleBackendTest, RGB_To_RGBA_Conversion) {
    auto backend = GetParam();
    
    TestImage rgb_img(width_, height_, 3);
    TestImage rgba_img(width_, height_, 4);
    
    rgb_img.fillRandom(123);
    
    ccap::rgbToRgba(rgb_img.data(), rgb_img.stride(), 
                    rgba_img.data(), rgba_img.stride(), 
                    width_, height_);
    
    // Check channel preservation and alpha addition
    for (int i = 0; i < 3; ++i) {
        const uint8_t* rgb_pixel = rgb_img.data() + i * 3;
        const uint8_t* rgba_pixel = rgba_img.data() + i * 4;
        
        EXPECT_EQ(rgb_pixel[0], rgba_pixel[0]) << "R->R mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgb_pixel[1], rgba_pixel[1]) << "G->G mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgb_pixel[2], rgba_pixel[2]) << "B->B mismatch at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(rgba_pixel[3], 255) << "Alpha should be 255 at pixel " << i << ", backend: " << BackendTestManager::getBackendName(backend);
    }
}

// Test function aliases
TEST_P(ColorShuffleBackendTest, Function_Aliases_Work) {
    auto backend = GetParam();
    
    TestImage bgra_img(width_, height_, 4);
    TestImage rgb_result1(width_, height_, 3);
    TestImage rgb_result2(width_, height_, 3);
    
    bgra_img.fillGradient();
    
    // Test alias: bgraToRgb = rgbaToBgr
    ccap::bgraToRgb(bgra_img.data(), bgra_img.stride(), 
                    rgb_result1.data(), rgb_result1.stride(), 
                    width_, height_);
    
    ccap::rgbaToBgr(bgra_img.data(), bgra_img.stride(), 
                    rgb_result2.data(), rgb_result2.stride(), 
                    width_, height_);
    
    // Results should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(rgb_result1.data(), rgb_result2.data(), 
                                            width_, height_, 3, 
                                            rgb_result1.stride(), rgb_result2.stride(), 0))
        << "Function aliases should produce identical results, backend: " << BackendTestManager::getBackendName(backend);
}

INSTANTIATE_BACKEND_TEST(ColorShuffleBackendTest);

// ============ RGB/BGR Conversion Tests ============

class RGBBGRConversionTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
        width_ = 32;
        height_ = 32;
    }

    int width_;
    int height_;
};

TEST_P(RGBBGRConversionTest, RGB_To_BGR_CompleteTest) {
    auto backend = GetParam();
    
    TestImage rgb_img(width_, height_, 3);
    TestImage bgr_img(width_, height_, 3);
    
    rgb_img.fillRandom(42);
    
    ccap::rgbToBgr(rgb_img.data(), rgb_img.stride(),
                   bgr_img.data(), bgr_img.stride(),
                   width_, height_);
    
    // Verify channel swap for all pixels
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const uint8_t* rgb_pixel = rgb_img.data() + y * rgb_img.stride() + x * 3;
            const uint8_t* bgr_pixel = bgr_img.data() + y * bgr_img.stride() + x * 3;
            
            EXPECT_EQ(rgb_pixel[0], bgr_pixel[2]) << "R->B mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(rgb_pixel[1], bgr_pixel[1]) << "G->G mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(rgb_pixel[2], bgr_pixel[0]) << "B->R mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
        }
    }
}

TEST_P(RGBBGRConversionTest, BGR_To_RGB_CompleteTest) {
    auto backend = GetParam();
    
    TestImage bgr_img(width_, height_, 3);
    TestImage rgb_img(width_, height_, 3);
    
    bgr_img.fillChecker(100, 200);
    
    ccap::bgrToRgb(bgr_img.data(), bgr_img.stride(),
                   rgb_img.data(), rgb_img.stride(),
                   width_, height_);
    
    // Verify channel swap for all pixels
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const uint8_t* bgr_pixel = bgr_img.data() + y * bgr_img.stride() + x * 3;
            const uint8_t* rgb_pixel = rgb_img.data() + y * rgb_img.stride() + x * 3;
            
            EXPECT_EQ(bgr_pixel[0], rgb_pixel[2]) << "B->R mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(bgr_pixel[1], rgb_pixel[1]) << "G->G mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(bgr_pixel[2], rgb_pixel[0]) << "R->B mismatch at (" << x << "," << y << ") backend: " << BackendTestManager::getBackendName(backend);
        }
    }
}

INSTANTIATE_BACKEND_TEST(RGBBGRConversionTest);

// ============ Function Alias Tests ============

class FunctionAliasTest : public BackendTestManager::BackendTestFixture {
protected:
    void SetUp() override {
        BackendTestFixture::SetUp();
        setBackend(ccap::ConvertBackend::CPU); // Use CPU for precise verification
        width_ = 16;
        height_ = 16;
    }

    int width_;
    int height_;
};

TEST_F(FunctionAliasTest, BGRA_To_RGBA_Alias) {
    TestImage bgra_img(width_, height_, 4);
    TestImage rgba_img1(width_, height_, 4);
    TestImage rgba_img2(width_, height_, 4);
    
    bgra_img.fillRandom(123);
    
    // Test original function
    ccap::rgbaToBgra(bgra_img.data(), bgra_img.stride(),
                     rgba_img1.data(), rgba_img1.stride(),
                     width_, height_);
    
    // Test alias function
    ccap::bgraToRgba(bgra_img.data(), bgra_img.stride(),
                     rgba_img2.data(), rgba_img2.stride(),
                     width_, height_);
    
    // Results should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(rgba_img1.data(), rgba_img2.data(), 
                                            width_, height_, 4, 
                                            rgba_img1.stride(), rgba_img2.stride(), 0))
        << "bgraToRgba alias should produce same result as rgbaToBgra";
}

TEST_F(FunctionAliasTest, BGRA_To_RGB_Alias) {
    TestImage bgra_img(width_, height_, 4);
    TestImage rgb_img1(width_, height_, 3);
    TestImage rgb_img2(width_, height_, 3);
    
    bgra_img.fillGradient();
    
    // Test original function
    ccap::rgbaToBgr(bgra_img.data(), bgra_img.stride(),
                    rgb_img1.data(), rgb_img1.stride(),
                    width_, height_);
    
    // Test alias function
    ccap::bgraToRgb(bgra_img.data(), bgra_img.stride(),
                    rgb_img2.data(), rgb_img2.stride(),
                    width_, height_);
    
    // Results should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(rgb_img1.data(), rgb_img2.data(),
                                            width_, height_, 3,
                                            rgb_img1.stride(), rgb_img2.stride(), 0))
        << "bgraToRgb alias should produce same result as rgbaToBgr";
}

TEST_F(FunctionAliasTest, BGRA_To_BGR_Alias) {
    TestImage bgra_img(width_, height_, 4);
    TestImage bgr_img1(width_, height_, 3);
    TestImage bgr_img2(width_, height_, 3);
    
    bgra_img.fillChecker(50, 150);
    
    // Test original function
    ccap::rgbaToRgb(bgra_img.data(), bgra_img.stride(),
                    bgr_img1.data(), bgr_img1.stride(),
                    width_, height_);
    
    // Test alias function
    ccap::bgra2bgr(bgra_img.data(), bgra_img.stride(),
                   bgr_img2.data(), bgr_img2.stride(),
                   width_, height_);
    
    // Results should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(bgr_img1.data(), bgr_img2.data(),
                                            width_, height_, 3,
                                            bgr_img1.stride(), bgr_img2.stride(), 0))
        << "bgra2bgr alias should produce same result as rgbaToRgb";
}

TEST_F(FunctionAliasTest, BGR_To_RGBA_Alias) {
    TestImage bgr_img(width_, height_, 3);
    TestImage rgba_img1(width_, height_, 4);
    TestImage rgba_img2(width_, height_, 4);
    
    bgr_img.fillRandom(456);
    
    // Test original function
    ccap::rgbToBgra(bgr_img.data(), bgr_img.stride(),
                    rgba_img1.data(), rgba_img1.stride(),
                    width_, height_);
    
    // Test alias function
    ccap::bgrToRgba(bgr_img.data(), bgr_img.stride(),
                    rgba_img2.data(), rgba_img2.stride(),
                    width_, height_);
    
    // Results should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(rgba_img1.data(), rgba_img2.data(),
                                            width_, height_, 4,
                                            rgba_img1.stride(), rgba_img2.stride(), 0))
        << "bgrToRgba alias should produce same result as rgbToBgra";
}

TEST_F(FunctionAliasTest, BGR_To_BGRA_Alias) {
    TestImage bgr_img(width_, height_, 3);
    TestImage bgra_img1(width_, height_, 4);
    TestImage bgra_img2(width_, height_, 4);
    
    bgr_img.fillSolid(128);
    
    // Test original function
    ccap::rgbToRgba(bgr_img.data(), bgr_img.stride(),
                    bgra_img1.data(), bgra_img1.stride(),
                    width_, height_);
    
    // Test alias function
    ccap::bgrToBgra(bgr_img.data(), bgr_img.stride(),
                    bgra_img2.data(), bgra_img2.stride(),
                    width_, height_);
    
    // Results should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(bgra_img1.data(), bgra_img2.data(),
                                            width_, height_, 4,
                                            bgra_img1.stride(), bgra_img2.stride(), 0))
        << "bgrToBgra alias should produce same result as rgbToRgba";
}

// ============ Edge Case Tests for Color Conversions ============

class ColorConversionEdgeCaseTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
    }
};

TEST_P(ColorConversionEdgeCaseTest, VerySmallImages_1x1_RGBA_BGR) {
    auto backend = GetParam();
    
    TestImage rgba_img(1, 1, 4);
    TestImage bgr_img(1, 1, 3);
    
    rgba_img.fillSolid(128);
    
    // Should not crash with 1x1 image
    ccap::rgbaToBgr(rgba_img.data(), rgba_img.stride(), 
                    bgr_img.data(), bgr_img.stride(), 
                    1, 1);
    
    const uint8_t* rgba_pixel = rgba_img.data();
    const uint8_t* bgr_pixel = bgr_img.data();
    
    EXPECT_EQ(rgba_pixel[0], bgr_pixel[2]) << "R->B conversion failed, backend: " 
                                           << BackendTestManager::getBackendName(backend);
    EXPECT_EQ(rgba_pixel[1], bgr_pixel[1]) << "G->G conversion failed, backend: " 
                                           << BackendTestManager::getBackendName(backend);
    EXPECT_EQ(rgba_pixel[2], bgr_pixel[0]) << "B->R conversion failed, backend: " 
                                           << BackendTestManager::getBackendName(backend);
}

TEST_P(ColorConversionEdgeCaseTest, Large_Image_Performance_RGB_BGR) {
    auto backend = GetParam();
    
    // Test with larger image to ensure performance is reasonable
    const int large_width = 1920;
    const int large_height = 1080;
    
    TestImage rgb_img(large_width, large_height, 3);
    TestImage bgr_img(large_width, large_height, 3);
    
    rgb_img.fillGradient();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    ccap::rgbToBgr(rgb_img.data(), rgb_img.stride(), 
                   bgr_img.data(), bgr_img.stride(), 
                   large_width, large_height);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete in reasonable time (less than 1 second for 1080p)
    EXPECT_LT(duration.count(), 1000) << "Large image color conversion too slow, backend: " 
                                      << BackendTestManager::getBackendName(backend);
    
    // Verify conversion correctness for first few pixels
    for (int i = 0; i < 5; ++i) {
        const uint8_t* rgb_pixel = rgb_img.data() + i * 3;
        const uint8_t* bgr_pixel = bgr_img.data() + i * 3;
        
        EXPECT_EQ(rgb_pixel[0], bgr_pixel[2]) << "Large image conversion error at pixel " << i;
        EXPECT_EQ(rgb_pixel[1], bgr_pixel[1]) << "Large image conversion error at pixel " << i;
        EXPECT_EQ(rgb_pixel[2], bgr_pixel[0]) << "Large image conversion error at pixel " << i;
    }
}

INSTANTIATE_BACKEND_TEST(ColorConversionEdgeCaseTest);
