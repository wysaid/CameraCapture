/**
 * @file test_utils.h
 * @brief Utility functions and classes for ccap unit tests
 */

#pragma once

#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <vector>

namespace ccap_test {

/**
 * @brief Test image data container with automatic memory management
 */
class TestImage {
public:
    TestImage(int width, int height, int channels, int alignment = 32);
    ~TestImage() = default;

    // Non-copyable but movable
    TestImage(const TestImage&) = delete;
    TestImage& operator=(const TestImage&) = delete;
    TestImage(TestImage&&) = default;
    TestImage& operator=(TestImage&&) = default;

    uint8_t* data() { return data_.get(); }
    const uint8_t* data() const { return data_.get(); }

    int width() const { return width_; }
    int height() const { return height_; }
    int channels() const { return channels_; }
    int stride() const { return stride_; }
    size_t size() const { return size_; }

    // Fill with test pattern
    void fillGradient();
    void fillYUVGradient(); // For YUV video range gradient (16-235 for Y, 16-240 for UV)
    void fillRandom(uint32_t seed = 42);
    void fillSolid(uint8_t value);
    void fillChecker(uint8_t color1 = 0, uint8_t color2 = 255, int blockSize = 8);

private:
    int width_;
    int height_;
    int channels_;
    int stride_;
    size_t size_;
    std::unique_ptr<uint8_t, void (*)(void*)> data_;
};

/**
 * @brief YUV test image with separate Y, U, V planes
 */
class TestYUVImage {
public:
    TestYUVImage(int width, int height, bool isNV12 = true);

    uint8_t* y_data() { return y_plane_.data(); }
    uint8_t* u_data() { return u_plane_.data(); }
    uint8_t* v_data() { return v_plane_.data(); }
    uint8_t* uv_data() { return isNV12_ ? uv_plane_.data() : nullptr; }

    const uint8_t* y_data() const { return y_plane_.data(); }
    const uint8_t* u_data() const { return u_plane_.data(); }
    const uint8_t* v_data() const { return v_plane_.data(); }
    const uint8_t* uv_data() const { return isNV12_ ? uv_plane_.data() : nullptr; }

    int width() const { return width_; }
    int height() const { return height_; }
    int y_stride() const { return y_stride_; }
    int uv_stride() const { return uv_stride_; }

    bool isNV12() const { return isNV12_; }

    // Generate known YUV patterns
    void generateKnownPattern();
    void generateGradient();
    void generateRandomYUVImage(int width, int height, uint32_t seed = 42);
    void fillSolid(uint8_t y, uint8_t u, uint8_t v);

private:
    int width_;
    int height_;
    int y_stride_;
    int uv_stride_;
    bool isNV12_;

    TestImage y_plane_;
    TestImage u_plane_;  // For I420
    TestImage v_plane_;  // For I420
    TestImage uv_plane_; // For NV12
};

/**
 * @brief Test utilities for pixel comparison
 */
class PixelTestUtils {
public:
    // Compare two RGB/BGR images with tolerance
    static bool compareImages(const uint8_t* img1, const uint8_t* img2, int width, int height, int channels, int stride1, int stride2,
                              int tolerance = 1);

    // Calculate MSE between two images
    static double calculateMSE(const uint8_t* img1, const uint8_t* img2, int width, int height, int channels, int stride1, int stride2);

    // Calculate PSNR between two images
    static double calculatePSNR(const uint8_t* img1, const uint8_t* img2, int width, int height, int channels, int stride1, int stride2);

    // Verify RGB values are in valid range
    static bool isValidRGB(int r, int g, int b);

    // Reference YUV to RGB conversion for testing
    static void yuv2rgbReference(int y, int u, int v, int& r, int& g, int& b, bool isBT709 = false, bool isFullRange = false);

    // Debug image saving functions

    /**
     * @brief Save a TestImage as BMP file for debugging
     * @param image The image to save
     * @param filename The filename to save (without extension)
     * @return true if successful, false otherwise
     */
    static bool saveImageForDebug(const TestImage& image, const std::string& filename);

    /**
     * @brief Create a difference image highlighting pixels that differ between two images
     * @param img1 First image
     * @param img2 Second image
     * @param tolerance Tolerance for considering pixels as different
     * @return TestImage containing the difference visualization
     */
    static TestImage createDifferenceImage(const TestImage& img1, const TestImage& img2, int tolerance = 0);

    /**
     * @brief Save debug images when a comparison fails
     * @param img1 First image (e.g., AVX2 result)
     * @param img2 Second image (e.g., CPU result)
     * @param test_name Name of the test for filename generation
     * @param tolerance Tolerance used in comparison
     */
    static void saveDebugImagesOnFailure(const TestImage& img1, const TestImage& img2, const std::string& test_name, int tolerance = 0);
};

/**
 * @brief Google Test custom matchers for image comparison
 */
MATCHER_P2(ImageMatches, expected, tolerance, "Image matches within tolerance")
{
    const auto& actual_img = std::get<0>(arg);
    const auto& expected_img = std::get<1>(arg);

    return PixelTestUtils::compareImages(actual_img.data(), expected_img.data(), actual_img.width(), actual_img.height(),
                                         actual_img.channels(), actual_img.stride(), expected_img.stride(), tolerance);
}

/**
 * @brief Performance timing utilities
 */
class PerformanceTimer {
public:
    void start();
    double stopAndGetMs();

private:
    std::chrono::high_resolution_clock::time_point start_time_;
};

/**
 * @brief Test data generators
 */
class TestDataGenerator {
public:
    // Generate a set of test YUV values covering edge cases
    static std::vector<std::tuple<int, int, int>> getTestYUVValues();

    // Generate test image sizes
    static std::vector<std::pair<int, int>> getTestImageSizes();

    // Generate random YUV image with known characteristics
    static TestYUVImage generateRandomYUVImage(int width, int height, uint32_t seed = 42);
};

} // namespace ccap_test
