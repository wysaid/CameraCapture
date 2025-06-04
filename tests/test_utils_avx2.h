/**
 * @file test_utils_avx2.h
 * @brief Utilities for testing both AVX2 and CPU implementations
 */

#pragma once

#include "ccap_convert.h"
#include "test_utils.h"

#include <functional>
#include <gtest/gtest.h>
#include <string>

namespace ccap_test {

/**
 * @brief Utility class for testing both AVX2 and CPU implementations
 */
class AVX2TestRunner {
public:
    /**
     * @brief Run a test function with both AVX2 enabled and disabled (if AVX2 is supported)
     * @param test_func Function to execute for testing
     * @param test_name Name of the test for error reporting
     */
    static void runWithBothImplementations(std::function<void()> test_func, const std::string& test_name = "Test")
    {
        bool hasAVX2Support = ccap::hasAVX2();

        if (hasAVX2Support) {
            SCOPED_TRACE("Testing with AVX2 enabled");

            // First test with AVX2 enabled
            ccap::disableAVX2(false);
            ASSERT_NO_THROW(test_func()) << test_name << " failed with AVX2 enabled";

            SCOPED_TRACE("Testing with AVX2 disabled (CPU implementation)");

            // Then test with AVX2 disabled (CPU implementation)
            ccap::disableAVX2(true);
            ASSERT_NO_THROW(test_func()) << test_name << " failed with AVX2 disabled (CPU implementation)";

            // Restore AVX2 state
            ccap::disableAVX2(false);

            std::cout << "[INFO] " << test_name << " passed for both AVX2 and CPU implementations" << std::endl;
        }
        else {
            SCOPED_TRACE("Testing with CPU implementation only (AVX2 not supported)");

            // Only test CPU implementation if AVX2 is not supported
            ASSERT_NO_THROW(test_func()) << test_name << " failed with CPU implementation";

            std::cout << "[INFO] " << test_name << " passed for CPU implementation (AVX2 not supported)" << std::endl;
        }
    }

    /**
     * @brief Run a comparison test between AVX2 and CPU implementations
     * @param test_func Function that returns a result for comparison
     * @param test_name Name of the test for error reporting
     */
    template <typename T> static void runComparisonTest(std::function<T()> test_func, const std::string& test_name = "Comparison Test")
    {
        bool hasAVX2Support = ccap::hasAVX2();

        if (hasAVX2Support) {
            // Get result with AVX2 enabled
            ccap::disableAVX2(false);
            T avx2_result = test_func();

            // Get result with AVX2 disabled
            ccap::disableAVX2(true);
            T cpu_result = test_func();

            // Compare results
            EXPECT_EQ(avx2_result, cpu_result) << test_name << ": AVX2 and CPU implementations should produce identical results";

            // Restore AVX2 state
            ccap::disableAVX2(false);

            std::cout << "[INFO] " << test_name << " - AVX2 and CPU implementations produce identical results" << std::endl;
        }
        else {
            // Just run the test once if AVX2 is not supported
            T result = test_func();
            (void)result; // Avoid unused variable warning

            std::cout << "[INFO] " << test_name << " completed (AVX2 not supported, tested CPU implementation only)" << std::endl;
        }
    }

    /**
     * @brief Run a test that compares image data between AVX2 and CPU implementations
     * @param test_func Function that performs conversion and returns result image
     * @param test_name Name of the test for error reporting
     * @param tolerance Allowed pixel difference tolerance
     */
    static void runImageComparisonTest(std::function<TestImage()> test_func, const std::string& test_name = "Image Comparison Test",
                                       int tolerance = 0)
    {
        bool hasAVX2Support = ccap::hasAVX2();

        if (hasAVX2Support) {
            // Get result with AVX2 enabled
            ccap::disableAVX2(false);
            TestImage avx2_result = test_func();

            // Get result with AVX2 disabled
            ccap::disableAVX2(true);
            TestImage cpu_result = test_func();

            // Compare images
            ASSERT_EQ(avx2_result.width(), cpu_result.width()) << test_name << ": Image dimensions should match";
            ASSERT_EQ(avx2_result.height(), cpu_result.height()) << test_name << ": Image dimensions should match";
            ASSERT_EQ(avx2_result.channels(), cpu_result.channels()) << test_name << ": Image channels should match";

            // Compare pixel data
            bool images_match =
                PixelTestUtils::compareImages(avx2_result.data(), cpu_result.data(), avx2_result.width(), avx2_result.height(),
                                              avx2_result.channels(), avx2_result.stride(), cpu_result.stride(), tolerance);

            // If images don't match, save debug images before failing
            if (!images_match) {
                std::cout << "[WARNING] Image mismatch detected in " << test_name << std::endl;
                PixelTestUtils::saveDebugImagesOnFailure(avx2_result, cpu_result, test_name, tolerance);
            }

            EXPECT_TRUE(images_match) << test_name
                                      << ": AVX2 and CPU implementations should produce identical images (tolerance: " << tolerance << ")";

            // Restore AVX2 state
            ccap::disableAVX2(false);

            std::cout << "[INFO] " << test_name << " - AVX2 and CPU implementations produce identical images" << std::endl;
        }
        else {
            // Just run the test once if AVX2 is not supported
            TestImage result = test_func();
            (void)result; // Avoid unused variable warning

            std::cout << "[INFO] " << test_name << " completed (AVX2 not supported, tested CPU implementation only)" << std::endl;
        }
    }

    /**
     * @brief Get a string describing the current implementation being tested
     */
    static std::string getCurrentImplementationName()
    {
        bool hasAVX2Support = ccap::hasAVX2();
        if (hasAVX2Support) {
            return "AVX2";
        }
        else {
            return "CPU";
        }
    }

    /**
     * @brief Check if the current platform supports AVX2
     */
    static bool isAVX2Supported() { return ccap::hasAVX2(); }
};

/**
 * @brief Macro to run a test with both AVX2 and CPU implementations
 */
#define RUN_DUAL_IMPLEMENTATION_TEST(test_func) AVX2TestRunner::runWithBothImplementations([&]() { test_func; }, #test_func)

/**
 * @brief Macro to run an image comparison test between AVX2 and CPU implementations
 */
#define RUN_IMAGE_COMPARISON_TEST(test_func, tolerance) \
    AVX2TestRunner::runImageComparisonTest([&]() -> TestImage { return test_func; }, #test_func, tolerance)

} // namespace ccap_test
