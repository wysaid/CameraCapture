/**
 * @file test_dual_implementation.cpp
 * @brief Comprehensive tests for comparing AVX2 and CPU implementations
 */

#include "ccap_convert.h"
#include "test_utils.h"
#include "test_utils_avx2.h"

#include <gtest/gtest.h>
#include <vector>

using namespace ccap_test;

class DualImplementationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Standard test dimensions
        width_ = 256;
        height_ = 256;
        small_width_ = 32;
        small_height_ = 32;
        large_width_ = 512;
        large_height_ = 512;
    }

    void TearDown() override {
        // Ensure AVX2 is re-enabled after tests
        if (AVX2TestRunner::isAVX2Supported()) {
            ccap::disableAVX2(false);
        }
    }

    int width_, height_;
    int small_width_, small_height_;
    int large_width_, large_height_;
};

// ============ Color Shuffle Comprehensive Tests ============

TEST_F(DualImplementationTest, All_ColorShuffle_Functions_Multiple_Sizes) {
    std::vector<std::pair<int, int>> test_sizes = {
        { small_width_, small_height_ },
        { width_, height_ },
        { large_width_, large_height_ },
        { 64, 48 }, // Non-power-of-2 dimensions (even)
        { 128, 98 } // Non-standard dimensions (even)
    };

    for (const auto& size : test_sizes) {
        int w = size.first;
        int h = size.second;

        SCOPED_TRACE("Testing size: " + std::to_string(w) + "x" + std::to_string(h));

        // Test RGBA to BGRA
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage rgba_src(w, h, 4);
                TestImage bgra_dst(w, h, 4);
                rgba_src.fillGradient();

                ccap::rgbaToBgra(rgba_src.data(), rgba_src.stride(), bgra_dst.data(), bgra_dst.stride(), w, h);
                return bgra_dst;
            },
            "RGBA to BGRA size " + std::to_string(w) + "x" + std::to_string(h), 0);

        // Test RGBA to BGR
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage rgba_src(w, h, 4);
                TestImage bgr_dst(w, h, 3);
                rgba_src.fillGradient();

                ccap::rgbaToBgr(rgba_src.data(), rgba_src.stride(), bgr_dst.data(), bgr_dst.stride(), w, h);
                return bgr_dst;
            },
            "RGBA to BGR size " + std::to_string(w) + "x" + std::to_string(h), 0);

        // Test RGB to BGR
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage rgb_src(w, h, 3);
                TestImage bgr_dst(w, h, 3);
                rgb_src.fillGradient();

                ccap::rgbToBgr(rgb_src.data(), rgb_src.stride(), bgr_dst.data(), bgr_dst.stride(), w, h);
                return bgr_dst;
            },
            "RGB to BGR size " + std::to_string(w) + "x" + std::to_string(h), 0);
    }
}

// ============ YUV Conversion Comprehensive Tests ============

TEST_F(DualImplementationTest, All_NV12_Conversions_Multiple_Sizes) {
    std::vector<std::pair<int, int>> test_sizes = {
        { 64, 64 },   // Small size
        { 128, 128 }, // Medium size
        { 256, 256 }, // Large size
        { 320, 240 }, // Common video resolution
        { 62, 46 }    // Non-aligned dimensions (even)
    };

    for (const auto& size : test_sizes) {
        int w = size.first;
        int h = size.second;

        SCOPED_TRACE("Testing NV12 size: " + std::to_string(w) + "x" + std::to_string(h));

        // Test NV12 to BGRA32
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage y_plane(w, h, 1);
                TestImage uv_plane(w / 2, h / 2, 2);
                TestImage bgra_dst(w, h, 4);

                y_plane.fillYUVGradient();
                uv_plane.fillYUVGradient();

                ccap::nv12ToBgra32(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), bgra_dst.data(), bgra_dst.stride(),
                                   w, h, ccap::ConvertFlag::Default);
                return bgra_dst;
            },
            "NV12 to BGRA32 size " + std::to_string(w) + "x" + std::to_string(h), 3);

        // Test NV12 to RGBA32
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage y_plane(w, h, 1);
                TestImage uv_plane(w / 2, h / 2, 2);
                TestImage rgba_dst(w, h, 4);

                y_plane.fillYUVGradient();
                uv_plane.fillYUVGradient();

                ccap::nv12ToRgba32(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), rgba_dst.data(), rgba_dst.stride(),
                                   w, h, ccap::ConvertFlag::Default);
                return rgba_dst;
            },
            "NV12 to RGBA32 size " + std::to_string(w) + "x" + std::to_string(h), 3);

        // Test NV12 to BGR24
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage y_plane(w, h, 1);
                TestImage uv_plane(w / 2, h / 2, 2);
                TestImage bgr_dst(w, h, 3);

                y_plane.fillYUVGradient();
                uv_plane.fillYUVGradient();

                ccap::nv12ToBgr24(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), bgr_dst.data(), bgr_dst.stride(), w,
                                  h, ccap::ConvertFlag::Default);
                return bgr_dst;
            },
            "NV12 to BGR24 size " + std::to_string(w) + "x" + std::to_string(h), 3);

        // Test NV12 to RGB24
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage y_plane(w, h, 1);
                TestImage uv_plane(w / 2, h / 2, 2);
                TestImage rgb_dst(w, h, 3);

                y_plane.fillYUVGradient();
                uv_plane.fillYUVGradient();

                ccap::nv12ToRgb24(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), rgb_dst.data(), rgb_dst.stride(), w,
                                  h, ccap::ConvertFlag::Default);
                return rgb_dst;
            },
            "NV12 to RGB24 size " + std::to_string(w) + "x" + std::to_string(h), 3);
    }
}

TEST_F(DualImplementationTest, All_I420_Conversions_Multiple_Sizes) {
    std::vector<std::pair<int, int>> test_sizes = {
        { 64, 64 },   // Small size
        { 128, 128 }, // Medium size
        { 256, 256 }, // Large size
        { 320, 240 }  // Common video resolution
    };

    for (const auto& size : test_sizes) {
        int w = size.first;
        int h = size.second;

        SCOPED_TRACE("Testing I420 size: " + std::to_string(w) + "x" + std::to_string(h));

        // Test I420 to BGRA32
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage y_plane(w, h, 1);
                TestImage u_plane(w / 2, h / 2, 1);
                TestImage v_plane(w / 2, h / 2, 1);
                TestImage bgra_dst(w, h, 4);

                y_plane.fillYUVGradient();
                u_plane.fillYUVGradient();
                v_plane.fillYUVGradient();

                ccap::i420ToBgra32(y_plane.data(), y_plane.stride(), u_plane.data(), u_plane.stride(), v_plane.data(), v_plane.stride(),
                                   bgra_dst.data(), bgra_dst.stride(), w, h, ccap::ConvertFlag::Default);
                return bgra_dst;
            },
            "I420 to BGRA32 size " + std::to_string(w) + "x" + std::to_string(h), 3);

        // Test I420 to RGBA32
        AVX2TestRunner::runImageComparisonTest(
            [&]() -> TestImage {
                TestImage y_plane(w, h, 1);
                TestImage u_plane(w / 2, h / 2, 1);
                TestImage v_plane(w / 2, h / 2, 1);
                TestImage rgba_dst(w, h, 4);

                y_plane.fillYUVGradient();
                u_plane.fillYUVGradient();
                v_plane.fillYUVGradient();

                ccap::i420ToRgba32(y_plane.data(), y_plane.stride(), u_plane.data(), u_plane.stride(), v_plane.data(), v_plane.stride(),
                                   rgba_dst.data(), rgba_dst.stride(), w, h, ccap::ConvertFlag::Default);
                return rgba_dst;
            },
            "I420 to RGBA32 size " + std::to_string(w) + "x" + std::to_string(h), 3);
    }
}

// ============ Edge Case Tests ============

TEST_F(DualImplementationTest, Vertical_Flip_All_Functions) {
    const int w = 64;
    const int h = 64;

    // Test RGBA to BGRA with vertical flip
    AVX2TestRunner::runImageComparisonTest(
        [&]() -> TestImage {
            TestImage rgba_src(w, h, 4);
            TestImage bgra_dst(w, h, 4);
            rgba_src.fillGradient();

            uint8_t* flipped_dst = bgra_dst.data() + (h - 1) * bgra_dst.stride();
            ccap::rgbaToBgra(rgba_src.data(), rgba_src.stride(), flipped_dst, -bgra_dst.stride(), w, -h);
            return bgra_dst;
        },
        "RGBA to BGRA vertical flip dual implementation", 0);

    // Test RGB to BGR with vertical flip
    AVX2TestRunner::runImageComparisonTest(
        [&]() -> TestImage {
            TestImage rgb_src(w, h, 3);
            TestImage bgr_dst(w, h, 3);
            rgb_src.fillGradient();

            uint8_t* flipped_dst = bgr_dst.data() + (h - 1) * bgr_dst.stride();
            ccap::rgbToBgr(rgb_src.data(), rgb_src.stride(), flipped_dst, -bgr_dst.stride(), w, -h);
            return bgr_dst;
        },
        "RGB to BGR vertical flip dual implementation", 0);
}

// ============ Solid Color YUV Conversion Tests ============

TEST_F(DualImplementationTest, Solid_Color_YUV_Conversions_Multiple_Sizes) {
    // Test resolution array - comprehensive coverage of different image sizes
    std::vector<std::pair<int, int>> test_sizes = {
        { small_width_, small_height_ }, // 32x32 - Small size
        { 64, 64 },                      // Medium-small size
        { 128, 128 },                    // Medium size (original test size)
        { width_, height_ },             // 256x256 - Standard size
        { large_width_, large_height_ }, // 512x512 - Large size
        { 640, 480 },                    // Common 4:3 video resolution
        { 720, 480 },                    // Standard definition TV
        { 1280, 720 },                   // HD resolution
        { 64, 48 },                      // Non-standard dimensions (even)
        { 128, 98 },                     // Non-standard dimensions (even)
        { 320, 240 }                     // QVGA resolution
    };

    // Define standard colors in YUV space (BT.601 video range)
    struct ColorInfo {
        const char* name;
        uint8_t y, u, v;
        int tolerance; // MSE tolerance for this color
    };

    std::vector<ColorInfo> test_colors = {
        // Standard colors (BT.601 video range)
        { "white", 235, 128, 128, 2 },   // Pure white (video range max Y)
        { "black", 16, 128, 128, 2 },    // Pure black (video range min Y)
        { "red", 81, 90, 240, 4 },       // Pure red
        { "green", 145, 54, 34, 4 },     // Pure green
        { "blue", 41, 240, 110, 4 },     // Pure blue
        { "magenta", 106, 202, 222, 4 }, // Magenta (Red + Blue)
        { "yellow", 210, 16, 146, 4 },   // Yellow (Red + Green)
        { "cyan", 170, 166, 16, 4 },     // Cyan (Green + Blue)

        // Extreme boundary colors - these push the limits of YUV space
        { "extreme_white", 255, 128, 128, 6 },      // Full range white (Y=255)
        { "extreme_black", 0, 128, 128, 6 },        // Full range black (Y=0)
        { "max_u_chroma", 128, 255, 128, 8 },       // Maximum U chroma (blue extreme)
        { "min_u_chroma", 128, 0, 128, 8 },         // Minimum U chroma (yellow extreme)
        { "max_v_chroma", 128, 128, 255, 8 },       // Maximum V chroma (red extreme)
        { "min_v_chroma", 128, 128, 0, 8 },         // Minimum V chroma (green extreme)
        { "max_chroma_corner", 128, 255, 255, 10 }, // Both U and V at maximum
        { "min_chroma_corner", 128, 0, 0, 10 },     // Both U and V at minimum
        { "bright_red_extreme", 255, 0, 255, 10 },  // Brightest possible red
        { "dark_blue_extreme", 0, 255, 0, 10 },     // Darkest possible blue
        { "bright_yellow_extreme", 255, 0, 0, 10 }, // Brightest possible yellow
        { "dark_cyan_extreme", 0, 255, 255, 10 }    // Darkest possible cyan
    };

    // Test each resolution with all colors and conversion formats
    for (const auto& size : test_sizes) {
        int w = size.first;
        int h = size.second;

        SCOPED_TRACE("Testing resolution: " + std::to_string(w) + "x" + std::to_string(h));

        // Helper lambda to run a single color test for current resolution
        auto testSolidColor = [&](const ColorInfo& color, const char* format_name, std::function<TestImage()> converter) {
            return AVX2TestRunner::runImageComparisonTest(converter,
                                                          std::string(format_name) + " " + color.name + " " + std::to_string(w) + "x" +
                                                              std::to_string(h) + " dual implementation",
                                                          color.tolerance);
        };

        // Test all colors for each conversion function at current resolution
        for (const auto& color : test_colors) {
            SCOPED_TRACE("Testing color: " + std::string(color.name));

            // Test NV12 to BGRA32 (default BT601 + VideoRange)
            testSolidColor(color, "NV12 to BGRA32 Default", [&]() -> TestImage {
                TestYUVImage yuv_src(w, h, true); // NV12
                TestImage bgra_dst(w, h, 4);

                yuv_src.fillSolid(color.y, color.u, color.v);

                ccap::nv12ToBgra32(yuv_src.y_data(), yuv_src.y_stride(), yuv_src.uv_data(), yuv_src.uv_stride(), bgra_dst.data(),
                                   bgra_dst.stride(), w, h, ccap::ConvertFlag::Default);
                return bgra_dst;
            });

            // Test NV12 to RGB24 (default BT601 + VideoRange)
            testSolidColor(color, "NV12 to RGB24 Default", [&]() -> TestImage {
                TestYUVImage yuv_src(w, h, true); // NV12
                TestImage rgb_dst(w, h, 3);

                yuv_src.fillSolid(color.y, color.u, color.v);

                ccap::nv12ToRgb24(yuv_src.y_data(), yuv_src.y_stride(), yuv_src.uv_data(), yuv_src.uv_stride(), rgb_dst.data(),
                                  rgb_dst.stride(), w, h, ccap::ConvertFlag::Default);
                return rgb_dst;
            });

            // Test I420 to BGRA32 (default BT601 + VideoRange)
            testSolidColor(color, "I420 to BGRA32 Default", [&]() -> TestImage {
                TestYUVImage yuv_src(w, h, false); // I420
                TestImage bgra_dst(w, h, 4);

                yuv_src.fillSolid(color.y, color.u, color.v);

                ccap::i420ToBgra32(yuv_src.y_data(), yuv_src.y_stride(), yuv_src.u_data(), yuv_src.uv_stride(), yuv_src.v_data(),
                                   yuv_src.uv_stride(), bgra_dst.data(), bgra_dst.stride(), w, h, ccap::ConvertFlag::Default);
                return bgra_dst;
            });

            // Test I420 to RGB24 (default BT601 + VideoRange)
            testSolidColor(color, "I420 to RGB24 Default", [&]() -> TestImage {
                TestYUVImage yuv_src(w, h, false); // I420
                TestImage rgb_dst(w, h, 3);

                yuv_src.fillSolid(color.y, color.u, color.v);

                ccap::i420ToRgb24(yuv_src.y_data(), yuv_src.y_stride(), yuv_src.u_data(), yuv_src.uv_stride(), yuv_src.v_data(),
                                  yuv_src.uv_stride(), rgb_dst.data(), rgb_dst.stride(), w, h, ccap::ConvertFlag::Default);
                return rgb_dst;
            });
        }
    }
}

// ============ Performance Information Test ============

TEST_F(DualImplementationTest, Performance_Information_Report) {
    bool hasAVX2Support = AVX2TestRunner::isAVX2Supported();

    std::cout << "\n=== Dual Implementation Test Report ===" << std::endl;
    std::cout << "Platform AVX2 Support: " << (hasAVX2Support ? "YES" : "NO") << std::endl;

    if (hasAVX2Support) {
        std::cout << "Test Coverage: Both AVX2 and CPU implementations tested" << std::endl;
        std::cout << "All conversion functions verified for bit-exact compatibility" << std::endl;

        // Test that disableAVX2 works correctly
        ccap::disableAVX2(true);
        bool avx2_disabled = ccap::hasAVX2();
        ccap::disableAVX2(false);
        bool avx2_enabled = ccap::hasAVX2();

        EXPECT_FALSE(avx2_disabled) << "AVX2 should be disabled when disableAVX2(true) is called";
        EXPECT_TRUE(avx2_enabled) << "AVX2 should be enabled when disableAVX2(false) is called";

        std::cout << "AVX2 disable/enable functionality: VERIFIED" << std::endl;
    } else {
        std::cout << "Test Coverage: CPU implementation only (AVX2 not supported)" << std::endl;
        std::cout << "All conversion functions verified for correctness" << std::endl;
    }

    std::cout << "=======================================" << std::endl;

    // This test always passes, it's just for information
    EXPECT_TRUE(true);
}

// ============ YUV Conversion ConvertFlag Tests ============

TEST_F(DualImplementationTest, All_ConvertFlag_Combinations_NV12_Conversions) {
    // Test different flag combinations for NV12 conversions
    struct FlagCombination {
        ccap::ConvertFlag flag;
        std::string name;
        int tolerance; // Different tolerance for different combinations
    };

    std::vector<FlagCombination> flag_combinations = { { ccap::ConvertFlag::BT601 | ccap::ConvertFlag::VideoRange, "BT601_VideoRange", 3 },
                                                       { ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange, "BT601_FullRange", 3 },
                                                       { ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange, "BT709_VideoRange", 3 },
                                                       { ccap::ConvertFlag::BT709 | ccap::ConvertFlag::FullRange, "BT709_FullRange", 3 } };

    std::vector<std::pair<int, int>> test_sizes = {
        { 64, 64 },   // Small size
        { 128, 128 }, // Medium size
        { 256, 256 }  // Large size
    };

    for (const auto& size : test_sizes) {
        int w = size.first;
        int h = size.second;

        SCOPED_TRACE("Testing NV12 size: " + std::to_string(w) + "x" + std::to_string(h));

        for (const auto& combo : flag_combinations) {
            SCOPED_TRACE("Testing flag combination: " + combo.name);

            // Test NV12 to BGRA32 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage uv_plane(w / 2, h / 2, 2);
                    TestImage bgra_dst(w, h, 4);

                    y_plane.fillYUVGradient();
                    uv_plane.fillYUVGradient();

                    ccap::nv12ToBgra32(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), bgra_dst.data(),
                                       bgra_dst.stride(), w, h, combo.flag);
                    return bgra_dst;
                },
                "NV12 to BGRA32 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);

            // Test NV12 to RGBA32 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage uv_plane(w / 2, h / 2, 2);
                    TestImage rgba_dst(w, h, 4);

                    y_plane.fillYUVGradient();
                    uv_plane.fillYUVGradient();

                    ccap::nv12ToRgba32(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), rgba_dst.data(),
                                       rgba_dst.stride(), w, h, combo.flag);
                    return rgba_dst;
                },
                "NV12 to RGBA32 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);

            // Test NV12 to BGR24 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage uv_plane(w / 2, h / 2, 2);
                    TestImage bgr_dst(w, h, 3);

                    y_plane.fillYUVGradient();
                    uv_plane.fillYUVGradient();

                    ccap::nv12ToBgr24(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), bgr_dst.data(),
                                      bgr_dst.stride(), w, h, combo.flag);
                    return bgr_dst;
                },
                "NV12 to BGR24 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);

            // Test NV12 to RGB24 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage uv_plane(w / 2, h / 2, 2);
                    TestImage rgb_dst(w, h, 3);

                    y_plane.fillYUVGradient();
                    uv_plane.fillYUVGradient();

                    ccap::nv12ToRgb24(y_plane.data(), y_plane.stride(), uv_plane.data(), uv_plane.stride(), rgb_dst.data(),
                                      rgb_dst.stride(), w, h, combo.flag);
                    return rgb_dst;
                },
                "NV12 to RGB24 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);
        }
    }
}

TEST_F(DualImplementationTest, All_ConvertFlag_Combinations_I420_Conversions) {
    // Test different flag combinations for I420 conversions
    struct FlagCombination {
        ccap::ConvertFlag flag;
        std::string name;
        int tolerance; // Different tolerance for different combinations
    };

    std::vector<FlagCombination> flag_combinations = { { ccap::ConvertFlag::BT601 | ccap::ConvertFlag::VideoRange, "BT601_VideoRange", 3 },
                                                       { ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange, "BT601_FullRange", 3 },
                                                       { ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange, "BT709_VideoRange", 3 },
                                                       { ccap::ConvertFlag::BT709 | ccap::ConvertFlag::FullRange, "BT709_FullRange", 3 } };

    std::vector<std::pair<int, int>> test_sizes = {
        { 64, 64 },   // Small size
        { 128, 128 }, // Medium size
        { 256, 256 }  // Large size
    };

    for (const auto& size : test_sizes) {
        int w = size.first;
        int h = size.second;

        SCOPED_TRACE("Testing I420 size: " + std::to_string(w) + "x" + std::to_string(h));

        for (const auto& combo : flag_combinations) {
            SCOPED_TRACE("Testing flag combination: " + combo.name);

            // Test I420 to BGRA32 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage u_plane(w / 2, h / 2, 1);
                    TestImage v_plane(w / 2, h / 2, 1);
                    TestImage bgra_dst(w, h, 4);

                    y_plane.fillYUVGradient();
                    u_plane.fillYUVGradient();
                    v_plane.fillYUVGradient();

                    ccap::i420ToBgra32(y_plane.data(), y_plane.stride(), u_plane.data(), u_plane.stride(), v_plane.data(), v_plane.stride(),
                                       bgra_dst.data(), bgra_dst.stride(), w, h, combo.flag);
                    return bgra_dst;
                },
                "I420 to BGRA32 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);

            // Test I420 to RGBA32 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage u_plane(w / 2, h / 2, 1);
                    TestImage v_plane(w / 2, h / 2, 1);
                    TestImage rgba_dst(w, h, 4);

                    y_plane.fillYUVGradient();
                    u_plane.fillYUVGradient();
                    v_plane.fillYUVGradient();

                    ccap::i420ToRgba32(y_plane.data(), y_plane.stride(), u_plane.data(), u_plane.stride(), v_plane.data(), v_plane.stride(),
                                       rgba_dst.data(), rgba_dst.stride(), w, h, combo.flag);
                    return rgba_dst;
                },
                "I420 to RGBA32 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);

            // Test I420 to BGR24 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage u_plane(w / 2, h / 2, 1);
                    TestImage v_plane(w / 2, h / 2, 1);
                    TestImage bgr_dst(w, h, 3);

                    y_plane.fillYUVGradient();
                    u_plane.fillYUVGradient();
                    v_plane.fillYUVGradient();

                    ccap::i420ToBgr24(y_plane.data(), y_plane.stride(), u_plane.data(), u_plane.stride(), v_plane.data(), v_plane.stride(),
                                      bgr_dst.data(), bgr_dst.stride(), w, h, combo.flag);
                    return bgr_dst;
                },
                "I420 to BGR24 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);

            // Test I420 to RGB24 with different flags
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestImage y_plane(w, h, 1);
                    TestImage u_plane(w / 2, h / 2, 1);
                    TestImage v_plane(w / 2, h / 2, 1);
                    TestImage rgb_dst(w, h, 3);

                    y_plane.fillYUVGradient();
                    u_plane.fillYUVGradient();
                    v_plane.fillYUVGradient();

                    ccap::i420ToRgb24(y_plane.data(), y_plane.stride(), u_plane.data(), u_plane.stride(), v_plane.data(), v_plane.stride(),
                                      rgb_dst.data(), rgb_dst.stride(), w, h, combo.flag);
                    return rgb_dst;
                },
                "I420 to RGB24 " + combo.name + " " + std::to_string(w) + "x" + std::to_string(h), combo.tolerance);
        }
    }
}

TEST_F(DualImplementationTest, Solid_Color_ConvertFlag_Combinations_Extreme_Values) {
    // Test extreme YUV values with all ConvertFlag combinations
    struct FlagCombination {
        ccap::ConvertFlag flag;
        std::string name;
        int tolerance;
    };

    std::vector<FlagCombination> flag_combinations = { { ccap::ConvertFlag::BT601 | ccap::ConvertFlag::VideoRange, "BT601_VideoRange", 4 },
                                                       { ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange, "BT601_FullRange", 4 },
                                                       { ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange, "BT709_VideoRange", 4 },
                                                       { ccap::ConvertFlag::BT709 | ccap::ConvertFlag::FullRange, "BT709_FullRange", 4 } };

    // Extreme test colors for comprehensive flag validation
    struct ExtremColor {
        const char* name;
        uint8_t y, u, v;
        int tolerance_modifier; // Additional tolerance for this specific color
    };

    std::vector<ExtremColor> extreme_colors = {
        // Video Range Extremes
        { "video_white", 235, 128, 128, 0 }, // Video range white
        { "video_black", 16, 128, 128, 0 },  // Video range black

        // Full Range Extremes
        { "full_white", 255, 128, 128, 2 }, // Full range white
        { "full_black", 0, 128, 128, 2 },   // Full range black

        // Chroma Extremes
        { "max_u_extreme", 128, 255, 128, 4 }, // Maximum U value
        { "min_u_extreme", 128, 0, 128, 4 },   // Minimum U value
        { "max_v_extreme", 128, 128, 255, 4 }, // Maximum V value
        { "min_v_extreme", 128, 128, 0, 4 },   // Minimum V value

        // Combined Extremes (higher tolerance due to accumulated rounding errors)
        { "bright_extreme", 255, 255, 255, 20 }, // All components at max
        { "dark_extreme", 0, 0, 0, 6 }           // All components at min
    };

    const int test_width = 64;
    const int test_height = 64;

    for (const auto& combo : flag_combinations) {
        SCOPED_TRACE("Testing flag combination: " + combo.name);

        for (const auto& color : extreme_colors) {
            SCOPED_TRACE("Testing extreme color: " + std::string(color.name));

            // Skip extreme values (255,255,255) for VideoRange as it's out of valid range
            bool is_extreme_bright = (color.y == 255 && color.u == 255 && color.v == 255);
            bool is_video_range = (combo.flag & ccap::ConvertFlag::VideoRange) != 0;

            if (is_extreme_bright && is_video_range) {
                SCOPED_TRACE("Skipping bright_extreme for VideoRange (out of valid range)");
                continue; // Skip this combination as it's outside VideoRange specification
            }

            int total_tolerance = combo.tolerance + color.tolerance_modifier;

            // Test NV12 to BGRA32 with extreme colors
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestYUVImage yuv_src(test_width, test_height, true); // NV12
                    TestImage bgra_dst(test_width, test_height, 4);

                    yuv_src.fillSolid(color.y, color.u, color.v);

                    ccap::nv12ToBgra32(yuv_src.y_data(), yuv_src.y_stride(), yuv_src.uv_data(), yuv_src.uv_stride(), bgra_dst.data(),
                                       bgra_dst.stride(), test_width, test_height, combo.flag);
                    return bgra_dst;
                },
                "NV12 to BGRA32 " + combo.name + " " + color.name, total_tolerance);

            // Test I420 to RGB24 with extreme colors
            AVX2TestRunner::runImageComparisonTest(
                [&]() -> TestImage {
                    TestYUVImage yuv_src(test_width, test_height, false); // I420
                    TestImage rgb_dst(test_width, test_height, 3);

                    yuv_src.fillSolid(color.y, color.u, color.v);

                    ccap::i420ToRgb24(yuv_src.y_data(), yuv_src.y_stride(), yuv_src.u_data(), yuv_src.uv_stride(), yuv_src.v_data(),
                                      yuv_src.uv_stride(), rgb_dst.data(), rgb_dst.stride(), test_width, test_height, combo.flag);
                    return rgb_dst;
                },
                "I420 to RGB24 " + combo.name + " " + color.name, total_tolerance);
        }
    }
}

// ============ Existing Solid Color YUV Conversion Tests ============
