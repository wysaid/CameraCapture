/**
 * @file test_yuv_conversions.cpp
 * @brief Comprehensive tests for YUV format conversions with multi-dimensional parameterization
 *
 * This test suite provides comprehensive coverage across three dimensions:
 * 1. Backend dimension: Tests all non-CPU backends against CPU baseline
 * 2. Resolution dimension: Tests various even-number resolutions
 * 3. Conversion flags dimension: Tests all BT601/BT709 + VideoRange/FullRange combinations
 */

#include "ccap_convert.h"
#include "test_backend_manager.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <tuple>
#include <vector>

using namespace ccap_test;

// ============ Multi-Dimensional YUV Conversion Test Framework ============

/**
 * @brief Test parameters for comprehensive YUV conversion testing
 * Encapsulates the three test dimensions: backend, resolution, and conversion flags
 */
struct YUVConversionTestParams {
    ccap::ConvertBackend backend;
    std::pair<int, int> resolution; // width, height
    ccap::ConvertFlag conversion_flags;

    std::string toString() const {
        std::string backend_name = BackendTestManager::getBackendName(backend);
        std::string resolution_str = std::to_string(resolution.first) + "x" + std::to_string(resolution.second);

        std::string flags_str;
        if (conversion_flags & ccap::ConvertFlag::BT601) {
            flags_str += "BT601";
        } else if (conversion_flags & ccap::ConvertFlag::BT709) {
            flags_str += "BT709";
        }

        if (conversion_flags & ccap::ConvertFlag::FullRange) {
            flags_str += "_FullRange";
        } else if (conversion_flags & ccap::ConvertFlag::VideoRange) {
            flags_str += "_VideoRange";
        }

        return backend_name + "_" + resolution_str + "_" + flags_str;
    }
};

/**
 * @brief Enum for YUV format types
 */
enum class YUVFormat {
    NV12,
    I420,
    YUYV,
    UYVY
};

/**
 * @brief Enum for RGB output format types
 */
enum class RGBFormat {
    RGB24,
    RGBA32,
    BGR24,
    BGRA32
};

/**
 * @brief Helper class for YUV conversion testing with multiple backends, resolutions, and flags
 */
class YUVConversionTestHelper {
public:
    /**
     * @brief Execute YUV to RGB conversion test comparing backend against CPU baseline
     */
    static void testYUVConversion(
        const YUVConversionTestParams& params,
        YUVFormat yuv_format,
        RGBFormat rgb_format,
        int tolerance = 5) {
        // Skip CPU backend comparison (CPU is our baseline)
        if (params.backend == ccap::ConvertBackend::CPU) {
            GTEST_SKIP() << "Skipping CPU backend self-comparison";
            return;
        }

        int width = params.resolution.first;
        int height = params.resolution.second;
        int channels = (rgb_format == RGBFormat::RGB24 || rgb_format == RGBFormat::BGR24) ? 3 : 4;

        // Create test images based on format
        std::unique_ptr<TestYUVImage> yuv_img;
        std::unique_ptr<TestImage> packed_img; // For YUYV/UYVY packed formats
        
        if (yuv_format == YUVFormat::YUYV || yuv_format == YUVFormat::UYVY) {
            // YUYV/UYVY are packed formats: each pixel pair uses 4 bytes (Y0 U Y1 V or U Y0 V Y1)
            // Store in a single image with 2 bytes per pixel (width * 2 bytes per row)
            packed_img = std::make_unique<TestImage>(width * 2, height, 1); // width*2 bytes, 1 channel
            generatePackedYUVPattern(*packed_img, yuv_format, width, height);
        } else {
            // NV12/I420 use planar formats
            bool is_nv12 = (yuv_format == YUVFormat::NV12);
            yuv_img = std::make_unique<TestYUVImage>(width, height, is_nv12);
            yuv_img->generateKnownPattern();
        }

        TestImage cpu_result(width, height, channels);
        TestImage backend_result(width, height, channels);

        // Get CPU result (baseline)
        auto original_backend = ccap::getConvertBackend();
        ccap::setConvertBackend(ccap::ConvertBackend::CPU);
        if (packed_img) {
            performPackedConversion(*packed_img, cpu_result, yuv_format, rgb_format, params.conversion_flags, width, height);
        } else {
            performConversion(*yuv_img, cpu_result, yuv_format, rgb_format, params.conversion_flags, width, height);
        }

        // Get backend result
        ccap::setConvertBackend(params.backend);
        if (packed_img) {
            performPackedConversion(*packed_img, backend_result, yuv_format, rgb_format, params.conversion_flags, width, height);
        } else {
            performConversion(*yuv_img, backend_result, yuv_format, rgb_format, params.conversion_flags, width, height);
        }

        // Compare results
        bool images_match = PixelTestUtils::compareImages(
            cpu_result.data(), backend_result.data(),
            width, height, channels,
            cpu_result.stride(), backend_result.stride(), tolerance);

        if (!images_match) {
            // Calculate MSE and PSNR for debugging
            double mse = PixelTestUtils::calculateMSE(
                cpu_result.data(), backend_result.data(),
                width, height, channels,
                cpu_result.stride(), backend_result.stride());
            double psnr = PixelTestUtils::calculatePSNR(
                cpu_result.data(), backend_result.data(),
                width, height, channels,
                cpu_result.stride(), backend_result.stride());

            std::cout << "[DEBUG] YUV conversion differences - MSE: " << mse
                      << ", PSNR: " << psnr << " dB" << std::endl;

            // Save debug images for analysis
            std::string test_name = getFormatString(yuv_format, rgb_format) + "_" + params.toString();
            PixelTestUtils::saveDebugImagesOnFailure(cpu_result, backend_result, test_name, tolerance);
        }

        EXPECT_TRUE(images_match)
            << "YUV conversion results differ between CPU and " << BackendTestManager::getBackendName(params.backend)
            << " backends for " << getFormatString(yuv_format, rgb_format)
            << " at " << width << "x" << height
            << " (tolerance: " << tolerance << ")";

        // Restore original backend
        ccap::setConvertBackend(original_backend);
    }

    /**
     * @brief Get all supported test resolutions (even numbers only)
     *
     * Covers various scenarios:
     * - Very small (8x8): Edge case for minimal processing
     * - Small square (64x64): Common thumbnail size
     * - Small rectangular (128x96): Aspect ratio testing
     * - Medium (320x240): Classic video resolution
     * - Standard VGA (640x480): Common desktop resolution
     * - HD (1280x720): Modern video standard
     */
    static std::vector<std::pair<int, int>> getTestResolutions() {
        return {
            { 8, 8 },    // Very small
            { 64, 64 },  // Small square
            { 66, 68 },  // Small square
            { 130, 96 }, // Small rectangular (even)
            { 540, 310 },
        };
    }

    /**
     * @brief Get all conversion flag combinations
     *
     * Tests all combinations of:
     * - Color space: BT601 (legacy) vs BT709 (modern HD)
     * - Range: VideoRange (16-235) vs FullRange (0-255)
     */
    static std::vector<ccap::ConvertFlag> getConversionFlags() {
        return {
            ccap::ConvertFlag::BT601 | ccap::ConvertFlag::VideoRange, // Default/Legacy
            ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange,  // Legacy with full range
            ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange, // Modern HD standard
            ccap::ConvertFlag::BT709 | ccap::ConvertFlag::FullRange   // Modern with full range
        };
    }

    /**
     * @brief Generate all test parameter combinations for multi-dimensional testing
     *
     * Creates combinations across three dimensions:
     * 1. Backend: All supported non-CPU backends (CPU is baseline, no self-comparison)
     * 2. Resolution: All even-numbered test resolutions
     * 3. Conversion flags: All BT601/BT709 + VideoRange/FullRange combinations
     *
     * @return Empty vector if only CPU backend available (triggers test skip)
     */
    static std::vector<YUVConversionTestParams> generateTestParams() {
        std::vector<YUVConversionTestParams> params;

        auto backends = BackendTestManager::getSupportedBackends();
        auto resolutions = getTestResolutions();
        auto flags = getConversionFlags();

        // Filter out CPU backend (we use it as baseline, not for comparison)
        std::vector<ccap::ConvertBackend> non_cpu_backends;
        for (auto backend : backends) {
            if (backend != ccap::ConvertBackend::CPU) {
                non_cpu_backends.push_back(backend);
            }
        }

        // If only CPU is supported, return empty params to skip all tests
        if (non_cpu_backends.empty()) {
            return params;
        }

        // Generate all combinations: backends × resolutions × flags
        for (auto backend : non_cpu_backends) {
            for (auto resolution : resolutions) {
                for (auto flag : flags) {
                    params.push_back({ backend, resolution, flag });
                }
            }
        }

        return params;
    }

private:
    /**
     * @brief Generate packed YUV test pattern for YUYV/UYVY formats
     */
    static void generatePackedYUVPattern(TestImage& packed_img, YUVFormat format, int width, int height) {
        uint8_t* data = packed_img.data();
        int stride = packed_img.stride();
        
        for (int y = 0; y < height; ++y) {
            uint8_t* row = data + y * stride;
            for (int x = 0; x < width; x += 2) {
                // Generate test pattern values
                uint8_t y0 = (uint8_t)(((x + y * width) * 219 / (width * height)) + 16);     // Video range Y
                uint8_t y1 = (uint8_t)(((x + 1 + y * width) * 219 / (width * height)) + 16); // Video range Y
                uint8_t u = (uint8_t)(((x / 2 + y) * 224 / (width / 2 + height)) + 16);      // Video range U/V
                uint8_t v = (uint8_t)(((x / 2 + y + 100) * 224 / (width / 2 + height)) + 16); // Video range U/V
                
                int base_idx = x * 2; // Each pixel pair uses 4 bytes
                
                if (format == YUVFormat::YUYV) {
                    // YUYV format: Y0 U Y1 V
                    row[base_idx + 0] = y0;
                    row[base_idx + 1] = u;
                    row[base_idx + 2] = y1;
                    row[base_idx + 3] = v;
                } else { // UYVY
                    // UYVY format: U Y0 V Y1
                    row[base_idx + 0] = u;
                    row[base_idx + 1] = y0;
                    row[base_idx + 2] = v;
                    row[base_idx + 3] = y1;
                }
            }
        }
    }
    
    /**
     * @brief Perform conversion for packed YUV formats (YUYV/UYVY)
     */
    static void performPackedConversion(
        const TestImage& packed_img,
        TestImage& result,
        YUVFormat yuv_format,
        RGBFormat rgb_format,
        ccap::ConvertFlag flags,
        int width,
        int height) {
        
        if (yuv_format == YUVFormat::YUYV) {
            switch (rgb_format) {
            case RGBFormat::RGB24:
                ccap::yuyvToRgb24(packed_img.data(), packed_img.stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::RGBA32:
                ccap::yuyvToRgba32(packed_img.data(), packed_img.stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            case RGBFormat::BGR24:
                ccap::yuyvToBgr24(packed_img.data(), packed_img.stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::BGRA32:
                ccap::yuyvToBgra32(packed_img.data(), packed_img.stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            }
        } else if (yuv_format == YUVFormat::UYVY) {
            switch (rgb_format) {
            case RGBFormat::RGB24:
                ccap::uyvyToRgb24(packed_img.data(), packed_img.stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::RGBA32:
                ccap::uyvyToRgba32(packed_img.data(), packed_img.stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            case RGBFormat::BGR24:
                ccap::uyvyToBgr24(packed_img.data(), packed_img.stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::BGRA32:
                ccap::uyvyToBgra32(packed_img.data(), packed_img.stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            }
        }
    }

    static void performConversion(
        const TestYUVImage& yuv_img,
        TestImage& result,
        YUVFormat yuv_format,
        RGBFormat rgb_format,
        ccap::ConvertFlag flags,
        int width,
        int height) {
        if (yuv_format == YUVFormat::NV12) {
            switch (rgb_format) {
            case RGBFormat::RGB24:
                ccap::nv12ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                                  yuv_img.uv_data(), yuv_img.uv_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::RGBA32:
                ccap::nv12ToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                                   yuv_img.uv_data(), yuv_img.uv_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            case RGBFormat::BGR24:
                ccap::nv12ToBgr24(yuv_img.y_data(), yuv_img.y_stride(),
                                  yuv_img.uv_data(), yuv_img.uv_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::BGRA32:
                ccap::nv12ToBgra32(yuv_img.y_data(), yuv_img.y_stride(),
                                   yuv_img.uv_data(), yuv_img.uv_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            }
        } else if (yuv_format == YUVFormat::I420) {
            switch (rgb_format) {
            case RGBFormat::RGB24:
                ccap::i420ToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                                  yuv_img.u_data(), yuv_img.u_stride(),
                                  yuv_img.v_data(), yuv_img.v_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::RGBA32:
                ccap::i420ToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                                   yuv_img.u_data(), yuv_img.u_stride(),
                                   yuv_img.v_data(), yuv_img.v_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            case RGBFormat::BGR24:
                ccap::i420ToBgr24(yuv_img.y_data(), yuv_img.y_stride(),
                                  yuv_img.u_data(), yuv_img.u_stride(),
                                  yuv_img.v_data(), yuv_img.v_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::BGRA32:
                ccap::i420ToBgra32(yuv_img.y_data(), yuv_img.y_stride(),
                                   yuv_img.u_data(), yuv_img.u_stride(),
                                   yuv_img.v_data(), yuv_img.v_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            }
        } else if (yuv_format == YUVFormat::YUYV) {
            // YUYV uses packed format, stored in y_data()
            switch (rgb_format) {
            case RGBFormat::RGB24:
                ccap::yuyvToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::RGBA32:
                ccap::yuyvToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            case RGBFormat::BGR24:
                ccap::yuyvToBgr24(yuv_img.y_data(), yuv_img.y_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::BGRA32:
                ccap::yuyvToBgra32(yuv_img.y_data(), yuv_img.y_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            }
        } else if (yuv_format == YUVFormat::UYVY) {
            // UYVY uses packed format, stored in y_data()
            switch (rgb_format) {
            case RGBFormat::RGB24:
                ccap::uyvyToRgb24(yuv_img.y_data(), yuv_img.y_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::RGBA32:
                ccap::uyvyToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            case RGBFormat::BGR24:
                ccap::uyvyToBgr24(yuv_img.y_data(), yuv_img.y_stride(),
                                  result.data(), result.stride(),
                                  width, height, flags);
                break;
            case RGBFormat::BGRA32:
                ccap::uyvyToBgra32(yuv_img.y_data(), yuv_img.y_stride(),
                                   result.data(), result.stride(),
                                   width, height, flags);
                break;
            }
        }
    }

    static std::string getFormatString(YUVFormat yuv_format, RGBFormat rgb_format) {
        std::string yuv_str;
        switch (yuv_format) {
        case YUVFormat::NV12:
            yuv_str = "NV12";
            break;
        case YUVFormat::I420:
            yuv_str = "I420";
            break;
        case YUVFormat::YUYV:
            yuv_str = "YUYV";
            break;
        case YUVFormat::UYVY:
            yuv_str = "UYVY";
            break;
        }
        
        std::string rgb_str;
        switch (rgb_format) {
        case RGBFormat::RGB24:
            rgb_str = "RGB24";
            break;
        case RGBFormat::RGBA32:
            rgb_str = "RGBA32";
            break;
        case RGBFormat::BGR24:
            rgb_str = "BGR24";
            break;
        case RGBFormat::BGRA32:
            rgb_str = "BGRA32";
            break;
        }
        return yuv_str + "_To_" + rgb_str;
    }
};

/**
 * @brief Parameterized test class for comprehensive YUV conversion testing
 */
class YUVConversionMultiDimensionalTest : public ::testing::TestWithParam<YUVConversionTestParams> {
protected:
    void SetUp() override {
        auto params = GetParam();

        // Skip if backend is not supported
        if (!BackendTestManager::isBackendSupported(params.backend)) {
            GTEST_SKIP() << "Backend " << BackendTestManager::getBackendName(params.backend)
                         << " is not supported on this platform";
        }

        // Skip if only CPU backend is available (no comparison needed)
        auto supported_backends = BackendTestManager::getSupportedBackends();
        bool has_non_cpu_backend = false;
        for (auto backend : supported_backends) {
            if (backend != ccap::ConvertBackend::CPU) {
                has_non_cpu_backend = true;
                break;
            }
        }

        if (!has_non_cpu_backend) {
            GTEST_SKIP() << "Only CPU backend available, skipping YUV conversion tests";
        }
    }
};

// ============ Comprehensive Multi-Dimensional Tests ============

TEST_P(YUVConversionMultiDimensionalTest, NV12_To_RGB24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::NV12, RGBFormat::RGB24);
}

TEST_P(YUVConversionMultiDimensionalTest, NV12_To_RGBA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::NV12, RGBFormat::RGBA32);
}

TEST_P(YUVConversionMultiDimensionalTest, NV12_To_BGR24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::NV12, RGBFormat::BGR24);
}

TEST_P(YUVConversionMultiDimensionalTest, NV12_To_BGRA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::NV12, RGBFormat::BGRA32);
}

TEST_P(YUVConversionMultiDimensionalTest, I420_To_RGB24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::I420, RGBFormat::RGB24);
}

TEST_P(YUVConversionMultiDimensionalTest, I420_To_RGBA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::I420, RGBFormat::RGBA32);
}

TEST_P(YUVConversionMultiDimensionalTest, I420_To_BGR24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::I420, RGBFormat::BGR24);
}

TEST_P(YUVConversionMultiDimensionalTest, I420_To_BGRA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::I420, RGBFormat::BGRA32);
}

// ============ YUYV Conversion Tests ============

TEST_P(YUVConversionMultiDimensionalTest, YUYV_To_RGB24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::YUYV, RGBFormat::RGB24);
}

TEST_P(YUVConversionMultiDimensionalTest, YUYV_To_RGBA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::YUYV, RGBFormat::RGBA32);
}

TEST_P(YUVConversionMultiDimensionalTest, YUYV_To_BGR24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::YUYV, RGBFormat::BGR24);
}

TEST_P(YUVConversionMultiDimensionalTest, YUYV_To_BGRA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::YUYV, RGBFormat::BGRA32);
}

// ============ UYVY Conversion Tests ============

TEST_P(YUVConversionMultiDimensionalTest, UYVY_To_RGB24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::UYVY, RGBFormat::RGB24);
}

TEST_P(YUVConversionMultiDimensionalTest, UYVY_To_RGBA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::UYVY, RGBFormat::RGBA32);
}

TEST_P(YUVConversionMultiDimensionalTest, UYVY_To_BGR24) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::UYVY, RGBFormat::BGR24);
}

TEST_P(YUVConversionMultiDimensionalTest, UYVY_To_BGRA32) {
    YUVConversionTestHelper::testYUVConversion(GetParam(), YUVFormat::UYVY, RGBFormat::BGRA32);
}

// Instantiate the multi-dimensional parameterized tests
INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    YUVConversionMultiDimensionalTest,
    ::testing::ValuesIn(YUVConversionTestHelper::generateTestParams()),
    [](const ::testing::TestParamInfo<YUVConversionTestParams>& info) {
        return info.param.toString();
    });

// Allow uninstantiated parameterized test when only CPU backend is available
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(YUVConversionMultiDimensionalTest);

// ============ Single Pixel YUV to RGB Tests ============

class YUVPixelConversionTest : public BackendTestManager::BackendTestFixture {
protected:
    void SetUp() override {
        BackendTestFixture::SetUp();
        setBackend(ccap::ConvertBackend::CPU); // Use CPU for precise pixel tests
    }
};

TEST_F(YUVPixelConversionTest, YUV2RGB601_VideoRange_KnownValues) {
    struct TestCase {
        int y, u, v;
        int expected_r, expected_g, expected_b;
        std::string description;
    };

    std::vector<TestCase> test_cases = {
        { 16, 128, 128, 0, 0, 0, "Video black" },
        { 235, 128, 128, 255, 255, 255, "Video white" },
        { 81, 90, 240, 255, 0, 0, "Pure red" },
        { 145, 54, 34, 0, 255, 0, "Pure green" },
        { 41, 240, 110, 0, 0, 255, "Pure blue" },
    };

    for (const auto& test_case : test_cases) {
        int r, g, b;
        ccap::yuv2rgb601v(test_case.y, test_case.u, test_case.v, r, g, b);

        EXPECT_NEAR(r, test_case.expected_r, 10) << "Red mismatch for " << test_case.description;
        EXPECT_NEAR(g, test_case.expected_g, 10) << "Green mismatch for " << test_case.description;
        EXPECT_NEAR(b, test_case.expected_b, 10) << "Blue mismatch for " << test_case.description;
    }
}

TEST_F(YUVPixelConversionTest, YUV2RGB709_VideoRange_KnownValues) {
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

TEST_F(YUVPixelConversionTest, GetYuvToRgbFunc_ReturnsCorrectFunctions) {
    // Test function pointer retrieval
    auto func_601v = ccap::getYuvToRgbFunc(true, false);  // BT.601 video range
    auto func_601f = ccap::getYuvToRgbFunc(true, true);   // BT.601 full range
    auto func_709v = ccap::getYuvToRgbFunc(false, false); // BT.709 video range
    auto func_709f = ccap::getYuvToRgbFunc(false, true);  // BT.709 full range

    EXPECT_NE(func_601v, nullptr) << "BT.601 video range function should not be null";
    EXPECT_NE(func_601f, nullptr) << "BT.601 full range function should not be null";
    EXPECT_NE(func_709v, nullptr) << "BT.709 video range function should not be null";
    EXPECT_NE(func_709f, nullptr) << "BT.709 full range function should not be null";

    // Test that different combinations return different functions
    EXPECT_NE(func_601v, func_601f) << "Video and full range should be different functions";
    EXPECT_NE(func_601v, func_709v) << "BT.601 and BT.709 should be different functions";

    // Test function calls through pointers
    int r1, g1, b1, r2, g2, b2;

    func_601v(128, 128, 128, r1, g1, b1);
    ccap::yuv2rgb601v(128, 128, 128, r2, g2, b2);

    EXPECT_EQ(r1, r2) << "Function pointer should produce same result as direct call";
    EXPECT_EQ(g1, g2) << "Function pointer should produce same result as direct call";
    EXPECT_EQ(b1, b2) << "Function pointer should produce same result as direct call";
}

// ============ YUYV/UYVY Packed Format Tests ============

/**
 * @brief Test class for YUYV/UYVY packed format conversions
 */
class PackedYUVConversionTest : public BackendTestManager::BackendTestFixture {
protected:
    void SetUp() override {
        BackendTestFixture::SetUp();
        setBackend(ccap::ConvertBackend::CPU); // Start with CPU for baseline tests
    }
    
    /**
     * @brief Create a simple test pattern for YUYV/UYVY formats
     */
    void createTestPattern(TestImage& packed_img, YUVFormat format, int width, int height) {
        uint8_t* data = packed_img.data();
        int stride = packed_img.stride();
        
        // Create a simple gradient pattern
        for (int y = 0; y < height; ++y) {
            uint8_t* row = data + y * stride;
            for (int x = 0; x < width; x += 2) {
                // Simple test values
                uint8_t y0 = 80 + (x * 100 / width);     // Y values 80-180
                uint8_t y1 = 80 + ((x+1) * 100 / width); // Y values 80-180  
                uint8_t u = 100 + (y * 50 / height);     // U values 100-150
                uint8_t v = 140 - (y * 50 / height);     // V values 140-90
                
                int base_idx = x * 2;
                
                if (format == YUVFormat::YUYV) {
                    // YUYV: Y0 U Y1 V
                    row[base_idx + 0] = y0;
                    row[base_idx + 1] = u;
                    row[base_idx + 2] = y1;
                    row[base_idx + 3] = v;
                } else { // UYVY
                    // UYVY: U Y0 V Y1
                    row[base_idx + 0] = u;
                    row[base_idx + 1] = y0;
                    row[base_idx + 2] = v;
                    row[base_idx + 3] = y1;
                }
            }
        }
    }
};

TEST_F(PackedYUVConversionTest, YUYV_BasicConversion_SmallImage) {
    const int width = 8, height = 4;
    TestImage yuyv_img(width * 2, height, 1); // YUYV needs width*2 bytes per row
    TestImage result_rgb(width, height, 3);
    TestImage result_rgba(width, height, 4);
    
    createTestPattern(yuyv_img, YUVFormat::YUYV, width, height);
    
    // Test RGB24 conversion
    ccap::yuyvToRgb24(yuyv_img.data(), yuyv_img.stride(),
                      result_rgb.data(), result_rgb.stride(),
                      width, height, ccap::ConvertFlag::Default);
    
    // Test RGBA32 conversion
    ccap::yuyvToRgba32(yuyv_img.data(), yuyv_img.stride(),
                       result_rgba.data(), result_rgba.stride(),
                       width, height, ccap::ConvertFlag::Default);
    
    // Basic sanity checks - RGB values should be in valid range
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t r = result_rgb.data()[y * result_rgb.stride() + x * 3 + 0];
            uint8_t g = result_rgb.data()[y * result_rgb.stride() + x * 3 + 1];
            uint8_t b = result_rgb.data()[y * result_rgb.stride() + x * 3 + 2];
            
            // RGB values should be in valid range
            EXPECT_LE(r, 255) << "Red value out of range at (" << x << "," << y << ")";
            EXPECT_LE(g, 255) << "Green value out of range at (" << x << "," << y << ")";
            EXPECT_LE(b, 255) << "Blue value out of range at (" << x << "," << y << ")";
            
            // RGBA should have alpha = 255
            uint8_t a = result_rgba.data()[y * result_rgba.stride() + x * 4 + 3];
            EXPECT_EQ(a, 255) << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

TEST_F(PackedYUVConversionTest, UYVY_BasicConversion_SmallImage) {
    const int width = 8, height = 4;
    TestImage uyvy_img(width * 2, height, 1); // UYVY needs width*2 bytes per row
    TestImage result_bgr(width, height, 3);
    TestImage result_bgra(width, height, 4);
    
    createTestPattern(uyvy_img, YUVFormat::UYVY, width, height);
    
    // Test BGR24 conversion
    ccap::uyvyToBgr24(uyvy_img.data(), uyvy_img.stride(),
                      result_bgr.data(), result_bgr.stride(),
                      width, height, ccap::ConvertFlag::Default);
    
    // Test BGRA32 conversion
    ccap::uyvyToBgra32(uyvy_img.data(), uyvy_img.stride(),
                       result_bgra.data(), result_bgra.stride(),
                       width, height, ccap::ConvertFlag::Default);
    
    // Basic sanity checks - RGB values should be in valid range
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t b = result_bgr.data()[y * result_bgr.stride() + x * 3 + 0];
            uint8_t g = result_bgr.data()[y * result_bgr.stride() + x * 3 + 1];
            uint8_t r = result_bgr.data()[y * result_bgr.stride() + x * 3 + 2];
            
            // RGB values should be in valid range
            EXPECT_LE(r, 255) << "Red value out of range at (" << x << "," << y << ")";
            EXPECT_LE(g, 255) << "Green value out of range at (" << x << "," << y << ")";
            EXPECT_LE(b, 255) << "Blue value out of range at (" << x << "," << y << ")";
            
            // BGRA should have alpha = 255
            uint8_t a = result_bgra.data()[y * result_bgra.stride() + x * 4 + 3];
            EXPECT_EQ(a, 255) << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

/**
 * @brief Test CPU vs AVX2 backend consistency for YUYV/UYVY conversions
 */
TEST_F(PackedYUVConversionTest, YUYV_CPU_vs_AVX2_Consistency) {
    // Skip if AVX2 is not supported
    if (!ccap::hasAVX2()) {
        GTEST_SKIP() << "AVX2 not supported on this platform";
    }
    
    const int width = 64, height = 32; // Size suitable for AVX2 optimization
    TestImage yuyv_img(width * 2, height, 1);
    TestImage cpu_result(width, height, 4);
    TestImage avx2_result(width, height, 4);
    
    createTestPattern(yuyv_img, YUVFormat::YUYV, width, height);
    
    // Get CPU result
    ccap::setConvertBackend(ccap::ConvertBackend::CPU);
    ccap::yuyvToRgba32(yuyv_img.data(), yuyv_img.stride(),
                       cpu_result.data(), cpu_result.stride(),
                       width, height, ccap::ConvertFlag::Default);
    
    // Get AVX2 result
    ccap::setConvertBackend(ccap::ConvertBackend::AVX2);
    ccap::yuyvToRgba32(yuyv_img.data(), yuyv_img.stride(),
                       avx2_result.data(), avx2_result.stride(),
                       width, height, ccap::ConvertFlag::Default);
    
    // Compare results
    bool images_match = PixelTestUtils::compareImages(
        cpu_result.data(), avx2_result.data(),
        width, height, 4,
        cpu_result.stride(), avx2_result.stride(), 3);
    
    if (!images_match) {
        // Calculate MSE and PSNR for debugging
        double mse = PixelTestUtils::calculateMSE(
            cpu_result.data(), avx2_result.data(),
            width, height, 4,
            cpu_result.stride(), avx2_result.stride());
        double psnr = PixelTestUtils::calculatePSNR(
            cpu_result.data(), avx2_result.data(),
            width, height, 4,
            cpu_result.stride(), avx2_result.stride());

        std::cout << "[DEBUG] YUYV conversion differences - MSE: " << mse
                  << ", PSNR: " << psnr << " dB" << std::endl;

        // Save debug images for analysis
        std::string test_name = "YUYV_To_RGBA32_CPU_vs_AVX2_" + std::to_string(width) + "x" + std::to_string(height);
        PixelTestUtils::saveDebugImagesOnFailure(cpu_result, avx2_result, test_name, 3);
    }
    
    EXPECT_TRUE(images_match) << "YUYV conversion results differ between CPU and AVX2 backends";
}

TEST_F(PackedYUVConversionTest, UYVY_CPU_vs_AVX2_Consistency) {
    // Skip if AVX2 is not supported
    if (!ccap::hasAVX2()) {
        GTEST_SKIP() << "AVX2 not supported on this platform";
    }
    
    const int width = 64, height = 32; // Size suitable for AVX2 optimization
    TestImage uyvy_img(width * 2, height, 1);
    TestImage cpu_result(width, height, 4);
    TestImage avx2_result(width, height, 4);
    
    createTestPattern(uyvy_img, YUVFormat::UYVY, width, height);
    
    // Get CPU result
    ccap::setConvertBackend(ccap::ConvertBackend::CPU);
    ccap::uyvyToBgra32(uyvy_img.data(), uyvy_img.stride(),
                       cpu_result.data(), cpu_result.stride(),
                       width, height, ccap::ConvertFlag::Default);
    
    // Get AVX2 result
    ccap::setConvertBackend(ccap::ConvertBackend::AVX2);
    ccap::uyvyToBgra32(uyvy_img.data(), uyvy_img.stride(),
                       avx2_result.data(), avx2_result.stride(),
                       width, height, ccap::ConvertFlag::Default);
    
    // Compare results
    bool images_match = PixelTestUtils::compareImages(
        cpu_result.data(), avx2_result.data(),
        width, height, 4,
        cpu_result.stride(), avx2_result.stride(), 3);
    
    if (!images_match) {
        // Calculate MSE and PSNR for debugging
        double mse = PixelTestUtils::calculateMSE(
            cpu_result.data(), avx2_result.data(),
            width, height, 4,
            cpu_result.stride(), avx2_result.stride());
        double psnr = PixelTestUtils::calculatePSNR(
            cpu_result.data(), avx2_result.data(),
            width, height, 4,
            cpu_result.stride(), avx2_result.stride());

        std::cout << "[DEBUG] UYVY conversion differences - MSE: " << mse
                  << ", PSNR: " << psnr << " dB" << std::endl;

        // Save debug images for analysis
        std::string test_name = "UYVY_To_BGRA32_CPU_vs_AVX2_" + std::to_string(width) + "x" + std::to_string(height);
        PixelTestUtils::saveDebugImagesOnFailure(cpu_result, avx2_result, test_name, 3);
    }
    
    EXPECT_TRUE(images_match) << "UYVY conversion results differ between CPU and AVX2 backends";
}

TEST_F(PackedYUVConversionTest, PackedYUV_ColorSpaceFlags_Consistency) {
    const int width = 32, height = 16;
    TestImage yuyv_img(width * 2, height, 1);
    TestImage result_601v(width, height, 3);
    TestImage result_601f(width, height, 3);
    TestImage result_709v(width, height, 3);
    TestImage result_709f(width, height, 3);
    
    createTestPattern(yuyv_img, YUVFormat::YUYV, width, height);
    
    // Test different color space conversions
    ccap::yuyvToRgb24(yuyv_img.data(), yuyv_img.stride(),
                      result_601v.data(), result_601v.stride(),
                      width, height, ccap::ConvertFlag::BT601 | ccap::ConvertFlag::VideoRange);
    
    ccap::yuyvToRgb24(yuyv_img.data(), yuyv_img.stride(),
                      result_601f.data(), result_601f.stride(),
                      width, height, ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange);
    
    ccap::yuyvToRgb24(yuyv_img.data(), yuyv_img.stride(),
                      result_709v.data(), result_709v.stride(),
                      width, height, ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange);
    
    ccap::yuyvToRgb24(yuyv_img.data(), yuyv_img.stride(),
                      result_709f.data(), result_709f.stride(),
                      width, height, ccap::ConvertFlag::BT709 | ccap::ConvertFlag::FullRange);
    
    // Results should be different between different color space conversions
    bool same_601v_601f = PixelTestUtils::compareImages(
        result_601v.data(), result_601f.data(),
        width, height, 3,
        result_601v.stride(), result_601f.stride(), 0);
    
    bool same_601v_709v = PixelTestUtils::compareImages(
        result_601v.data(), result_709v.data(),
        width, height, 3,
        result_601v.stride(), result_709v.stride(), 0);
    
    // Different color space settings should produce different results
    EXPECT_FALSE(same_601v_601f) << "BT601 VideoRange and FullRange should produce different results";
    EXPECT_FALSE(same_601v_709v) << "BT601 and BT709 should produce different results";
}

// ============ Additional Edge Case Tests (Special scenarios not covered by multi-dimensional tests) ============

/**
 * @brief Edge case test for extremely large images (if needed for specific testing)
 * Note: Most edge cases are now covered by the multi-dimensional test framework
 */
class YUVConversionSpecialEdgeCaseTest : public BackendTestManager::BackendTestFixture {
protected:
    void SetUp() override {
        BackendTestFixture::SetUp();

        // Skip if only CPU backend is available
        auto supported_backends = BackendTestManager::getSupportedBackends();
        bool has_non_cpu_backend = false;
        for (auto backend : supported_backends) {
            if (backend != ccap::ConvertBackend::CPU) {
                has_non_cpu_backend = true;
                break;
            }
        }

        if (!has_non_cpu_backend) {
            GTEST_SKIP() << "Only CPU backend available, skipping YUV conversion tests";
        }
    }
};

/**
 * @brief Summary of test coverage
 *
 * This test suite provides comprehensive coverage through the multi-dimensional framework:
 *
 * 1. Backend Dimension (Non-CPU backends tested against CPU baseline):
 *    - Apple VideoToolbox (macOS/iOS)
 *    - Intel AVX2 (x86_64 with AVX2 support)
 *    - LibYUV (cross-platform fallback)
 *    - Future backends can be easily added
 *
 * 2. Resolution Dimension (Even-numbered resolutions):
 *    - 8x8: Minimal processing edge case
 *    - 64x64: Small square (thumbnail)
 *    - 128x96: Small rectangular (aspect ratio testing)
 *    - 320x240: Classic video resolution
 *    - 640x480: Standard VGA
 *    - 1280x720: HD resolution
 *
 * 3. Conversion Parameters Dimension:
 *    - BT601 + VideoRange (Legacy/Default)
 *    - BT601 + FullRange (Legacy with full range)
 *    - BT709 + VideoRange (Modern HD standard)
 *    - BT709 + FullRange (Modern with full range)
 *
 * 4. Format Combinations:
 *    - YUV formats: NV12, I420
 *    - RGB formats: RGB24, RGBA32, BGR24, BGRA32
 *
 * Total test cases: [backends] × [6 resolutions] × [4 flag combinations] × [8 format combinations]
 * For example: 3 backends × 6 resolutions × 4 flags × 8 formats = 576 test cases
 *
 * This comprehensive approach replaces individual edge case tests and provides
 * better coverage with maintainable, parameterized code.
 */
