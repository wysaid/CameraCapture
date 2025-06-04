/**
 * @file test_convert_yuv_rgb.cpp
 * @brief Unit tests for YUV to RGB conversion functions in ccap_convert.h
 */

#include <gtest/gtest.h>
#include "test_utils.h"
#include "test_utils_avx2.h"
#include "ccap_convert.h"

using namespace ccap_test;

class YUVToRGBTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test setup
    }
    
    void TearDown() override {
        // Cleanup if needed
    }
};

// ============ Single Pixel Conversion Tests ============

TEST_F(YUVToRGBTest, YUV2RGB601VideoRange_KnownValues) {
    struct TestCase {
        int y, u, v;
        int expected_r, expected_g, expected_b;
        std::string description;
    };
    
    std::vector<TestCase> test_cases = {
        // Video range black (Y=16, U=V=128)
        {16, 128, 128, 0, 0, 0, "Video black"},
        
        // Video range white (Y=235, U=V=128)
        {235, 128, 128, 255, 255, 255, "Video white"},
        
        // Pure red (approximate)
        {81, 90, 240, 255, 0, 0, "Pure red"},
        
        // Pure green (approximate)
        {145, 54, 34, 0, 255, 0, "Pure green"},
        
        // Pure blue (approximate)
        {41, 240, 110, 0, 0, 255, "Pure blue"},
    };
    
    for (const auto& test_case : test_cases) {
        int r, g, b;
        ccap::yuv2rgb601v(test_case.y, test_case.u, test_case.v, r, g, b);
        
        // Allow small tolerance due to rounding
        EXPECT_NEAR(r, test_case.expected_r, 5) 
            << "Red mismatch for " << test_case.description 
            << " YUV(" << test_case.y << "," << test_case.u << "," << test_case.v << ")";
        EXPECT_NEAR(g, test_case.expected_g, 5) 
            << "Green mismatch for " << test_case.description;
        EXPECT_NEAR(b, test_case.expected_b, 5) 
            << "Blue mismatch for " << test_case.description;
            
        // Ensure RGB values are in valid range
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "Invalid RGB values for " << test_case.description;
    }
}

TEST_F(YUVToRGBTest, YUV2RGB709VideoRange_KnownValues) {
    // Test BT.709 conversion with known values
    int r, g, b;
    
    // Video black
    ccap::yuv2rgb709v(16, 128, 128, r, g, b);
    EXPECT_NEAR(r, 0, 3);
    EXPECT_NEAR(g, 0, 3);
    EXPECT_NEAR(b, 0, 3);
    
    // Video white
    ccap::yuv2rgb709v(235, 128, 128, r, g, b);
    EXPECT_NEAR(r, 255, 3);
    EXPECT_NEAR(g, 255, 3);
    EXPECT_NEAR(b, 255, 3);
}

TEST_F(YUVToRGBTest, YUV2RGB601FullRange_KnownValues) {
    int r, g, b;
    
    // Full range black
    ccap::yuv2rgb601f(0, 128, 128, r, g, b);
    EXPECT_NEAR(r, 0, 3);
    EXPECT_NEAR(g, 0, 3);
    EXPECT_NEAR(b, 0, 3);
    
    // Full range white
    ccap::yuv2rgb601f(255, 128, 128, r, g, b);
    EXPECT_NEAR(r, 255, 3);
    EXPECT_NEAR(g, 255, 3);
    EXPECT_NEAR(b, 255, 3);
}

TEST_F(YUVToRGBTest, YUV2RGB709FullRange_KnownValues) {
    int r, g, b;
    
    // Full range black
    ccap::yuv2rgb709f(0, 128, 128, r, g, b);
    EXPECT_NEAR(r, 0, 3);
    EXPECT_NEAR(g, 0, 3);
    EXPECT_NEAR(b, 0, 3);
    
    // Full range white
    ccap::yuv2rgb709f(255, 128, 128, r, g, b);
    EXPECT_NEAR(r, 255, 3);
    EXPECT_NEAR(g, 255, 3);
    EXPECT_NEAR(b, 255, 3);
}

// ============ Comprehensive Range Tests ============

class YUVConversionParameterizedTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {
protected:
    void TestYUVConversion(void (*conv_func)(int, int, int, int&, int&, int&)) {
        auto [y, u, v] = GetParam();
        int r, g, b;
        
        conv_func(y, u, v, r, g, b);
        
        // Verify output is in valid range
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "Invalid RGB(" << r << "," << g << "," << b 
            << ") for YUV(" << y << "," << u << "," << v << ")";
    }
};

TEST_P(YUVConversionParameterizedTest, YUV2RGB601v_ValidRange) {
    TestYUVConversion(ccap::yuv2rgb601v);
}

TEST_P(YUVConversionParameterizedTest, YUV2RGB709v_ValidRange) {
    TestYUVConversion(ccap::yuv2rgb709v);
}

TEST_P(YUVConversionParameterizedTest, YUV2RGB601f_ValidRange) {
    TestYUVConversion(ccap::yuv2rgb601f);
}

TEST_P(YUVConversionParameterizedTest, YUV2RGB709f_ValidRange) {
    TestYUVConversion(ccap::yuv2rgb709f);
}

INSTANTIATE_TEST_SUITE_P(
    YUVToRGBRangeTest,
    YUVConversionParameterizedTest,
    ::testing::ValuesIn(TestDataGenerator::getTestYUVValues())
);

// ============ Consistency Tests ============

TEST_F(YUVToRGBTest, ConsistencyBetweenVideoAndFullRange) {
    // Test that converting the same YUV values gives different but reasonable results
    // between video and full range
    
    std::vector<std::tuple<int, int, int>> test_yuv = {
        {128, 128, 128}, // Mid gray
        {100, 150, 200}, // Some color
        {200, 80, 180}   // Another color
    };
    
    for (const auto& [y, u, v] : test_yuv) {
        int r1, g1, b1, r2, g2, b2;
        
        // BT.601
        ccap::yuv2rgb601v(y, u, v, r1, g1, b1); // Video range
        ccap::yuv2rgb601f(y, u, v, r2, g2, b2); // Full range
        
        // Results should be different for most cases (except neutral gray)
        if (y != 128 || u != 128 || v != 128) {
            EXPECT_TRUE(r1 != r2 || g1 != g2 || b1 != b2)
                << "Video and full range should give different results for YUV("
                << y << "," << u << "," << v << ")";
        }
        
        // Both should be valid
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r1, g1, b1));
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r2, g2, b2));
    }
}

TEST_F(YUVToRGBTest, ConsistencyBetweenBT601AndBT709) {
    // Test that BT.601 and BT.709 give different results for the same input
    
    std::vector<std::tuple<int, int, int>> test_yuv = {
        {100, 150, 200},
        {150, 100, 180},
        {200, 50, 100}
    };
    
    for (const auto& [y, u, v] : test_yuv) {
        int r601, g601, b601, r709, g709, b709;
        
        ccap::yuv2rgb601v(y, u, v, r601, g601, b601);
        ccap::yuv2rgb709v(y, u, v, r709, g709, b709);
        
        // Results should be different
        EXPECT_TRUE(r601 != r709 || g601 != g709 || b601 != b709)
            << "BT.601 and BT.709 should give different results for YUV("
            << y << "," << u << "," << v << ")";
        
        // Both should be valid
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r601, g601, b601));
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r709, g709, b709));
    }
}

// ============ Edge Cases Tests ============

TEST_F(YUVToRGBTest, EdgeCases) {
    int r, g, b;
    
    // Test extreme values
    struct EdgeCase {
        int y, u, v;
        std::string description;
    };
    
    std::vector<EdgeCase> edge_cases = {
        {0, 0, 0, "All zeros"},
        {255, 255, 255, "All max"},
        {0, 128, 128, "Min Y, neutral UV"},
        {255, 128, 128, "Max Y, neutral UV"},
        {128, 0, 0, "Neutral Y, min UV"},
        {128, 255, 255, "Neutral Y, max UV"},
    };
    
    for (const auto& edge_case : edge_cases) {
        // Test all conversion functions
        ccap::yuv2rgb601v(edge_case.y, edge_case.u, edge_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT.601 video: " << edge_case.description;
        
        ccap::yuv2rgb601f(edge_case.y, edge_case.u, edge_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT.601 full: " << edge_case.description;
        
        ccap::yuv2rgb709v(edge_case.y, edge_case.u, edge_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT.709 video: " << edge_case.description;
        
        ccap::yuv2rgb709f(edge_case.y, edge_case.u, edge_case.v, r, g, b);
        EXPECT_TRUE(PixelTestUtils::isValidRGB(r, g, b))
            << "BT.709 full: " << edge_case.description;
    }
}

// ============ Reference Implementation Comparison ============

TEST_F(YUVToRGBTest, CompareWithReferenceImplementation) {
    auto test_values = TestDataGenerator::getTestYUVValues();
    
    for (const auto& [y, u, v] : test_values) {
        int ccap_r, ccap_g, ccap_b;
        int ref_r, ref_g, ref_b;
        
        // Test BT.601 video range
        ccap::yuv2rgb601v(y, u, v, ccap_r, ccap_g, ccap_b);
        PixelTestUtils::yuv2rgbReference(y, u, v, ref_r, ref_g, ref_b, false, false);
        
        EXPECT_NEAR(ccap_r, ref_r, 2) << "BT.601v Red mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_g, ref_g, 2) << "BT.601v Green mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_b, ref_b, 2) << "BT.601v Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
        
        // Test BT.709 video range
        ccap::yuv2rgb709v(y, u, v, ccap_r, ccap_g, ccap_b);
        PixelTestUtils::yuv2rgbReference(y, u, v, ref_r, ref_g, ref_b, true, false);
        
        EXPECT_NEAR(ccap_r, ref_r, 2) << "BT.709v Red mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_g, ref_g, 2) << "BT.709v Green mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_b, ref_b, 2) << "BT.709v Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
        
        // Test full range variants
        ccap::yuv2rgb601f(y, u, v, ccap_r, ccap_g, ccap_b);
        PixelTestUtils::yuv2rgbReference(y, u, v, ref_r, ref_g, ref_b, false, true);
        
        EXPECT_NEAR(ccap_r, ref_r, 2) << "BT.601f Red mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_g, ref_g, 2) << "BT.601f Green mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_b, ref_b, 2) << "BT.601f Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
        
        ccap::yuv2rgb709f(y, u, v, ccap_r, ccap_g, ccap_b);
        PixelTestUtils::yuv2rgbReference(y, u, v, ref_r, ref_g, ref_b, true, true);
        
        EXPECT_NEAR(ccap_r, ref_r, 2) << "BT.709f Red mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_g, ref_g, 2) << "BT.709f Green mismatch for YUV(" << y << "," << u << "," << v << ")";
        EXPECT_NEAR(ccap_b, ref_b, 2) << "BT.709f Blue mismatch for YUV(" << y << "," << u << "," << v << ")";
    }
}

// ============ Dual Implementation Tests (AVX2 vs CPU) ============

TEST_F(YUVToRGBTest, Dual_Implementation_NV12_To_BGRA32)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, true); // NV12
        TestImage bgra_dst(width, height, 4);
        
        yuv_src.generateGradient();
        
        ccap::nv12ToBgra32(yuv_src.y_data(), yuv_src.y_stride(),
                           yuv_src.uv_data(), yuv_src.uv_stride(),
                           bgra_dst.data(), bgra_dst.stride(),
                           width, height);
        
        return bgra_dst;
    }, "NV12 to BGRA32 dual implementation test", 3);
}

TEST_F(YUVToRGBTest, Dual_Implementation_NV12_To_RGBA32)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, true); // NV12
        TestImage rgba_dst(width, height, 4);
        
        yuv_src.generateGradient();
        
        ccap::nv12ToRgba32(yuv_src.y_data(), yuv_src.y_stride(),
                           yuv_src.uv_data(), yuv_src.uv_stride(),
                           rgba_dst.data(), rgba_dst.stride(),
                           width, height);
        
        return rgba_dst;
    }, "NV12 to RGBA32 dual implementation test", 3);
}

TEST_F(YUVToRGBTest, Dual_Implementation_NV12_To_BGR24)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, true); // NV12
        TestImage bgr_dst(width, height, 3);
        
        yuv_src.generateGradient();
        
        ccap::nv12ToBgr24(yuv_src.y_data(), yuv_src.y_stride(),
                          yuv_src.uv_data(), yuv_src.uv_stride(),
                          bgr_dst.data(), bgr_dst.stride(),
                          width, height);
        
        return bgr_dst;
    }, "NV12 to BGR24 dual implementation test", 3);
}

TEST_F(YUVToRGBTest, Dual_Implementation_NV12_To_RGB24)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, true); // NV12
        TestImage rgb_dst(width, height, 3);
        
        yuv_src.generateGradient();
        
        ccap::nv12ToRgb24(yuv_src.y_data(), yuv_src.y_stride(),
                          yuv_src.uv_data(), yuv_src.uv_stride(),
                          rgb_dst.data(), rgb_dst.stride(),
                          width, height);
        
        return rgb_dst;
    }, "NV12 to RGB24 dual implementation test", 3);
}

TEST_F(YUVToRGBTest, Dual_Implementation_I420_To_BGRA32)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, false); // I420
        TestImage bgra_dst(width, height, 4);
        
        yuv_src.generateGradient();
        
        ccap::i420ToBgra32(yuv_src.y_data(), yuv_src.y_stride(),
                           yuv_src.u_data(), yuv_src.uv_stride(),
                           yuv_src.v_data(), yuv_src.uv_stride(),
                           bgra_dst.data(), bgra_dst.stride(),
                           width, height);
        
        return bgra_dst;
    }, "I420 to BGRA32 dual implementation test", 3);
}

TEST_F(YUVToRGBTest, Dual_Implementation_I420_To_RGBA32)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, false); // I420
        TestImage rgba_dst(width, height, 4);
        
        yuv_src.generateGradient();
        
        ccap::i420ToRgba32(yuv_src.y_data(), yuv_src.y_stride(),
                           yuv_src.u_data(), yuv_src.uv_stride(),
                           yuv_src.v_data(), yuv_src.uv_stride(),
                           rgba_dst.data(), rgba_dst.stride(),
                           width, height);
        
        return rgba_dst;
    }, "I420 to RGBA32 dual implementation test", 3);
}

TEST_F(YUVToRGBTest, Dual_Implementation_I420_To_BGR24)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, false); // I420
        TestImage bgr_dst(width, height, 3);
        
        yuv_src.generateGradient();
        
        ccap::i420ToBgr24(yuv_src.y_data(), yuv_src.y_stride(),
                          yuv_src.u_data(), yuv_src.uv_stride(),
                          yuv_src.v_data(), yuv_src.uv_stride(),
                          bgr_dst.data(), bgr_dst.stride(),
                          width, height);
        
        return bgr_dst;
    }, "I420 to BGR24 dual implementation test", 3);
}

TEST_F(YUVToRGBTest, Dual_Implementation_I420_To_RGB24)
{
    const int width = 128;
    const int height = 128;
    
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage {
        TestYUVImage yuv_src(width, height, false); // I420
        TestImage rgb_dst(width, height, 3);
        
        yuv_src.generateGradient();
        
        ccap::i420ToRgb24(yuv_src.y_data(), yuv_src.y_stride(),
                          yuv_src.u_data(), yuv_src.uv_stride(),
                          yuv_src.v_data(), yuv_src.uv_stride(),
                          rgb_dst.data(), rgb_dst.stride(),
                          width, height);
        
        return rgb_dst;
    }, "I420 to RGB24 dual implementation test", 3);
}
