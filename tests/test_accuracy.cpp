/**
 * @file test_accuracy.cpp
 * @brief Accuracy and precision tests for ccap_convert functions with explicit backend specification
 */

#include "ccap_convert.h"
#include "test_utils.h"
#include "test_backend_manager.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace ccap_test;

// ============ Round-trip Accuracy Tests ============

class AccuracyBackendTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
        width_ = 64;
        height_ = 64;
        strict_tolerance_ = 0;    // For color shuffles
        yuv_tolerance_ = 5;       // For YUV conversions
    }

    int width_;
    int height_;
    int strict_tolerance_;
    int yuv_tolerance_;
};

TEST_P(AccuracyBackendTest, ColorShuffle_Perfect_Roundtrip) {
    auto backend = GetParam();
    
    TestImage original(width_, height_, 4);
    TestImage intermediate(width_, height_, 4);
    TestImage result(width_, height_, 4);

    original.fillGradient();

    // RGBA -> BGRA -> RGBA (should be perfect)
    ccap::rgbaToBgra(original.data(), original.stride(), 
                     intermediate.data(), intermediate.stride(), 
                     width_, height_);

    ccap::bgraToRgba(intermediate.data(), intermediate.stride(), 
                     result.data(), result.stride(), 
                     width_, height_);

    // Should be identical to original
    EXPECT_TRUE(PixelTestUtils::compareImages(original.data(), result.data(),
                                            width_, height_, 4, 
                                            original.stride(), result.stride(), 
                                            strict_tolerance_))
        << "Perfect roundtrip failed for backend: " << BackendTestManager::getBackendName(backend);
}

TEST_P(AccuracyBackendTest, RGB_RGBA_Roundtrip) {
    auto backend = GetParam();
    
    TestImage rgb_original(width_, height_, 3);
    TestImage rgba_intermediate(width_, height_, 4);
    TestImage rgb_result(width_, height_, 3);

    rgb_original.fillRandom(42);

    // RGB -> RGBA -> RGB
    ccap::rgbToRgba(rgb_original.data(), rgb_original.stride(), 
                    rgba_intermediate.data(), rgba_intermediate.stride(), 
                    width_, height_);

    ccap::rgbaToRgb(rgba_intermediate.data(), rgba_intermediate.stride(), 
                    rgb_result.data(), rgb_result.stride(), 
                    width_, height_);

    // Should be identical
    EXPECT_TRUE(PixelTestUtils::compareImages(rgb_original.data(), rgb_result.data(), 
                                            width_, height_, 3, 
                                            rgb_original.stride(), rgb_result.stride(), 
                                            strict_tolerance_))
        << "RGB roundtrip failed for backend: " << BackendTestManager::getBackendName(backend);
}

INSTANTIATE_BACKEND_TEST(AccuracyBackendTest);
