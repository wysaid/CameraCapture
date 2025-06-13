/**
 * @file test_utils.cpp
 * @brief Implementation of utility functions for ccap unit tests
 */

#include "test_utils.h"

#include "ccap_utils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif
#include <functional>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace ccap_test {

// ============ TestImage Implementation ============

TestImage::TestImage(int width, int height, int channels, int alignment) :
    width_(width), height_(height), channels_(channels), data_(nullptr, [](void* p) {
#ifdef _WIN32
        _aligned_free(p);
#else
        free(p);
#endif
    }) {
    // Calculate stride with alignment
    stride_ = ((width * channels + alignment - 1) / alignment) * alignment;
    size_ = stride_ * height;

    // Allocate aligned memory
    uint8_t* ptr = nullptr;

#ifdef _WIN32
    ptr = static_cast<uint8_t*>(_aligned_malloc(size_, alignment));
#else
    // posix_memalign requires alignment to be a power of 2 and at least sizeof(void*)
    // For alignment = 1, just use regular malloc
    if (alignment <= 1) {
        ptr = static_cast<uint8_t*>(malloc(size_));
    } else {
        // Ensure alignment is at least sizeof(void*) and is a power of 2
        size_t safe_alignment = alignment;
        if (safe_alignment < sizeof(void*)) {
            safe_alignment = sizeof(void*);
        }
        // Round up to nearest power of 2 if not already
        if ((safe_alignment & (safe_alignment - 1)) != 0) {
            safe_alignment = 1;
            while (safe_alignment < alignment) {
                safe_alignment <<= 1;
            }
        }

        int result = posix_memalign(reinterpret_cast<void**>(&ptr), safe_alignment, size_);
        if (result != 0) {
            ptr = nullptr;
        }
    }
#endif

    if (!ptr) {
        throw std::bad_alloc();
    }

    data_.reset(ptr);

    // Initialize to zero
    std::memset(data_.get(), 0, size_);
}

void TestImage::fillGradient() {
    for (int y = 0; y < height_; ++y) {
        uint8_t* row = data_.get() + y * stride_;
        for (int x = 0; x < width_; ++x) {
            for (int c = 0; c < channels_; ++c) {
                // Create a gradient pattern
                int value = ((x * 255 / width_) + (y * 255 / height_) + c * 64) % 256;
                row[x * channels_ + c] = static_cast<uint8_t>(value);
            }
        }
    }
}

void TestImage::fillYUVGradient() {
    for (int y = 0; y < height_; ++y) {
        uint8_t* row = data_.get() + y * stride_;
        for (int x = 0; x < width_; ++x) {
            for (int c = 0; c < channels_; ++c) {
                int value;
                if (channels_ == 1) {
                    // Y plane: 16-235 range
                    value = 16 + ((x * 219 / width_) + (y * 219 / height_)) % 220;
                } else {
                    // UV plane: 16-240 range
                    value = 16 + ((x * 224 / width_) + (y * 224 / height_) + c * 112) % 225;
                }
                row[x * channels_ + c] = static_cast<uint8_t>(value);
            }
        }
    }
}

void TestImage::fillRandom(uint32_t seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 255);

    for (int y = 0; y < height_; ++y) {
        uint8_t* row = data_.get() + y * stride_;
        for (int x = 0; x < width_; ++x) {
            for (int c = 0; c < channels_; ++c) {
                row[x * channels_ + c] = static_cast<uint8_t>(dis(gen));
            }
        }
    }
}

void TestImage::fillSolid(uint8_t value) {
    for (int y = 0; y < height_; ++y) {
        uint8_t* row = data_.get() + y * stride_;
        for (int x = 0; x < width_ * channels_; ++x) {
            row[x] = value;
        }
    }
}

void TestImage::fillChecker(uint8_t color1, uint8_t color2, int blockSize) {
    for (int y = 0; y < height_; ++y) {
        uint8_t* row = data_.get() + y * stride_;
        for (int x = 0; x < width_; ++x) {
            bool isColor1 = ((x / blockSize) + (y / blockSize)) % 2 == 0;
            uint8_t color = isColor1 ? color1 : color2;

            for (int c = 0; c < channels_; ++c) {
                row[x * channels_ + c] = color;
            }
        }
    }
}

// ============ TestYUVImage Implementation ============

TestYUVImage::TestYUVImage(int width, int height, bool isNV12) :
    width_(width),
    height_(height),
    isNV12_(isNV12),
    y_plane_(width, height, 1, 32),
    u_plane_(width / 2, height / 2, 1, 32),
    v_plane_(width / 2, height / 2, 1, 32),
    uv_plane_(width / 2, height / 2, 2, 32) {
    y_stride_ = y_plane_.stride();
    uv_stride_ = isNV12 ? uv_plane_.stride() : u_plane_.stride();
}

void TestYUVImage::generateKnownPattern() {
    // Create a known YUV pattern for testing
    for (int y = 0; y < height_; ++y) {
        uint8_t* y_row = y_plane_.data() + y * y_stride_;
        for (int x = 0; x < width_; ++x) {
            // Gradient in Y
            y_row[x] = static_cast<uint8_t>(16 + (x + y) % 220);
        }
    }

    // Fill UV planes
    for (int y = 0; y < height_ / 2; ++y) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_ / 2; ++x) {
                uv_row[x * 2 + 0] = static_cast<uint8_t>(128 + (x % 64) - 32); // U
                uv_row[x * 2 + 1] = static_cast<uint8_t>(128 + (y % 64) - 32); // V
            }
        } else {
            uint8_t* u_row = u_plane_.data() + y * uv_stride_;
            uint8_t* v_row = v_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_ / 2; ++x) {
                u_row[x] = static_cast<uint8_t>(128 + (x % 64) - 32);
                v_row[x] = static_cast<uint8_t>(128 + (y % 64) - 32);
            }
        }
    }
}

void TestYUVImage::generateGradient() {
    // Y plane gradient (16-235 video range)
    for (int y = 0; y < height_; ++y) {
        uint8_t* y_row = y_plane_.data() + y * y_stride_;
        for (int x = 0; x < width_; ++x) {
            y_row[x] = static_cast<uint8_t>(16 + (x * 219 / width_));
        }
    }

    // UV planes with color variation (16-240 video range)
    for (int y = 0; y < height_ / 2; ++y) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_ / 2; ++x) {
                uv_row[x * 2 + 0] = static_cast<uint8_t>(16 + (x * 224 / (width_ / 2)));  // U (16-240)
                uv_row[x * 2 + 1] = static_cast<uint8_t>(16 + (y * 224 / (height_ / 2))); // V (16-240)
            }
        } else {
            uint8_t* u_row = u_plane_.data() + y * uv_stride_;
            uint8_t* v_row = v_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_ / 2; ++x) {
                u_row[x] = static_cast<uint8_t>(16 + (x * 224 / (width_ / 2)));  // U (16-240)
                v_row[x] = static_cast<uint8_t>(16 + (y * 224 / (height_ / 2))); // V (16-240)
            }
        }
    }
}

void TestYUVImage::fillSolid(uint8_t y, uint8_t u, uint8_t v) {
    // Fill Y plane
    for (int row = 0; row < height_; ++row) {
        uint8_t* y_row = y_plane_.data() + row * y_stride_;
        std::fill(y_row, y_row + width_, y);
    }

    // Fill UV planes
    for (int row = 0; row < height_ / 2; ++row) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + row * uv_stride_;
            for (int x = 0; x < width_ / 2; ++x) {
                uv_row[x * 2 + 0] = u;
                uv_row[x * 2 + 1] = v;
            }
        } else {
            uint8_t* u_row = u_plane_.data() + row * uv_stride_;
            uint8_t* v_row = v_plane_.data() + row * uv_stride_;
            std::fill(u_row, u_row + width_ / 2, u);
            std::fill(v_row, v_row + width_ / 2, v);
        }
    }
}

void TestYUVImage::generateRandomYUVImage(int width, int height, uint32_t seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> y_dist(16, 235);  // Y range for BT.601 - changed from uint8_t to int
    std::uniform_int_distribution<int> uv_dist(16, 240); // UV range for BT.601 - changed from uint8_t to int

    // Fill Y plane with random values
    for (int row = 0; row < height; ++row) {
        uint8_t* y_row = y_plane_.data() + row * y_stride_;
        for (int col = 0; col < width; ++col) {
            y_row[col] = static_cast<uint8_t>(y_dist(gen));
        }
    }

    // Fill UV planes with random values
    for (int row = 0; row < height_ / 2; ++row) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + row * uv_stride_;
            for (int x = 0; x < width_ / 2; ++x) {
                uv_row[x * 2 + 0] = static_cast<uint8_t>(uv_dist(gen)); // U
                uv_row[x * 2 + 1] = static_cast<uint8_t>(uv_dist(gen)); // V
            }
        } else {
            uint8_t* u_row = u_plane_.data() + row * uv_stride_;
            uint8_t* v_row = v_plane_.data() + row * uv_stride_;
            for (int x = 0; x < width_ / 2; ++x) {
                u_row[x] = static_cast<uint8_t>(uv_dist(gen));
                v_row[x] = static_cast<uint8_t>(uv_dist(gen));
            }
        }
    }
}

// ============ PixelTestUtils Implementation ============

bool PixelTestUtils::compareImages(const uint8_t* img1, const uint8_t* img2, int width, int height, int channels, int stride1, int stride2,
                                   int tolerance) {
    for (int y = 0; y < height; ++y) {
        const uint8_t* row1 = img1 + y * stride1;
        const uint8_t* row2 = img2 + y * stride2;

        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                int diff = std::abs(static_cast<int>(row1[x * channels + c]) - static_cast<int>(row2[x * channels + c]));
                if (diff > tolerance) {
                    return false;
                }
            }
        }
    }
    return true;
}

double PixelTestUtils::calculateMSE(const uint8_t* img1, const uint8_t* img2, int width, int height, int channels, int stride1,
                                    int stride2) {
    double mse = 0.0;
    int total_pixels = 0;

    for (int y = 0; y < height; ++y) {
        const uint8_t* row1 = img1 + y * stride1;
        const uint8_t* row2 = img2 + y * stride2;

        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                double diff = static_cast<double>(row1[x * channels + c]) - static_cast<double>(row2[x * channels + c]);
                mse += diff * diff;
                total_pixels++;
            }
        }
    }

    return mse / total_pixels;
}

double PixelTestUtils::calculatePSNR(const uint8_t* img1, const uint8_t* img2, int width, int height, int channels, int stride1,
                                     int stride2) {
    double mse = calculateMSE(img1, img2, width, height, channels, stride1, stride2);
    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();
    }

    double max_pixel_value = 255.0;
    return 20.0 * std::log10(max_pixel_value / std::sqrt(mse));
}

void PixelTestUtils::yuv2rgbReference(int y, int u, int v, int& r, int& g, int& b, bool isBT709, bool isFullRange) {
    // Reference implementation for comparison
    if (!isFullRange) {
        y = y - 16; // video range
    }
    u = u - 128;
    v = v - 128;

    if (isBT709) {
        if (isFullRange) {
            r = (256 * y + 403 * v + 128) >> 8;
            g = (256 * y - 48 * u - 120 * v + 128) >> 8;
            b = (256 * y + 475 * u + 128) >> 8;
        } else {
            r = (298 * y + 459 * v + 128) >> 8;
            g = (298 * y - 55 * u - 136 * v + 128) >> 8;
            b = (298 * y + 541 * u + 128) >> 8;
        }
    } else { // BT601
        if (isFullRange) {
            r = (256 * y + 351 * v + 128) >> 8;
            g = (256 * y - 86 * u - 179 * v + 128) >> 8;
            b = (256 * y + 443 * u + 128) >> 8;
        } else {
            r = (298 * y + 409 * v + 128) >> 8;
            g = (298 * y - 100 * u - 208 * v + 128) >> 8;
            b = (298 * y + 516 * u + 128) >> 8;
        }
    }

    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
}

// ============ PerformanceTimer Implementation ============

void PerformanceTimer::start() { start_time_ = std::chrono::high_resolution_clock::now(); }

double PerformanceTimer::stopAndGetMs() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
    return duration.count() / 1000.0; // Convert to milliseconds
}

// ============ TestDataGenerator Implementation ============

std::vector<std::tuple<int, int, int>> TestDataGenerator::getTestYUVValues() {
    return {
        { 16, 128, 128 },  // Video black
        { 235, 128, 128 }, // Video white
        { 0, 128, 128 },   // Full black
        { 255, 128, 128 }, // Full white
        { 128, 0, 0 },     // Min UV
        { 128, 255, 255 }, // Max UV
        { 81, 90, 240 },   // Red
        { 145, 54, 34 },   // Green
        { 41, 240, 110 },  // Blue
        { 210, 16, 146 },  // Yellow
        { 170, 166, 16 },  // Cyan
        { 106, 202, 222 }, // Magenta
    };
}

std::vector<std::pair<int, int>> TestDataGenerator::getTestImageSizes() {
    return {
        { 4, 4 },      // Minimum
        { 16, 16 },    // Small
        { 64, 64 },    // Medium
        { 128, 96 },   // Common aspect ratio
        { 256, 256 },  // Large square
        { 320, 240 },  // QVGA
        { 640, 480 },  // VGA
        { 1280, 720 }, // HD
    };
}

TestYUVImage TestDataGenerator::generateRandomYUVImage(int width, int height, uint32_t seed) {
    TestYUVImage yuv_img(width, height, true); // Default to NV12

    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> y_dis(16, 235);  // Video range Y - changed from int to int (was already correct)
    std::uniform_int_distribution<int> uv_dis(16, 240); // UV range - changed from int to int (was already correct)

    // Fill Y plane
    for (int y = 0; y < height; ++y) {
        uint8_t* y_row = yuv_img.y_data() + y * yuv_img.y_stride();
        for (int x = 0; x < width; ++x) {
            y_row[x] = static_cast<uint8_t>(y_dis(gen));
        }
    }

    // Fill UV plane (NV12 format)
    for (int y = 0; y < height / 2; ++y) {
        uint8_t* uv_row = yuv_img.uv_data() + y * yuv_img.uv_stride();
        for (int x = 0; x < width / 2; ++x) {
            uv_row[x * 2 + 0] = static_cast<uint8_t>(uv_dis(gen)); // U
            uv_row[x * 2 + 1] = static_cast<uint8_t>(uv_dis(gen)); // V
        }
    }

    return yuv_img;
}

// ============ PixelTestUtils Debug Functions Implementation ============

bool PixelTestUtils::saveImageForDebug(const TestImage& image, const std::string& filename) {
    // Include ccap_utils.h functionality
    std::string full_filename = filename + ".bmp";

    // Determine if this is BGR format (we'll assume RGB by default, but allow BGR for 3-channel images)
    bool isBGR = (image.channels() == 3); // Assume 3-channel images are BGR for ccap compatibility
    bool hasAlpha = (image.channels() == 4);

    return ccap::saveRgbDataAsBMP(full_filename.c_str(), image.data(), image.width(), image.stride(), image.height(), isBGR, hasAlpha,
                                  false // isTopToBottom
    );
}

TestImage PixelTestUtils::createDifferenceImage(const TestImage& img1, const TestImage& img2, int tolerance) {
    // Images must have the same dimensions
    if (img1.width() != img2.width() || img1.height() != img2.height() || img1.channels() != img2.channels()) {
        throw std::invalid_argument("Images must have the same dimensions for difference calculation");
    }

    int width = img1.width();
    int height = img1.height();
    int channels = img1.channels();

    // Create difference image (will be RGB/BGR depending on input)
    TestImage diff_image(width, height, std::max(channels, 3)); // Ensure at least 3 channels for visualization

    for (int y = 0; y < height; ++y) {
        const uint8_t* row1 = img1.data() + y * img1.stride();
        const uint8_t* row2 = img2.data() + y * img2.stride();
        uint8_t* diff_row = diff_image.data() + y * diff_image.stride();

        for (int x = 0; x < width; ++x) {
            bool pixels_differ = false;
            int max_diff = 0;

            // Check if pixels differ
            for (int c = 0; c < channels; ++c) {
                int diff = std::abs(static_cast<int>(row1[x * channels + c]) - static_cast<int>(row2[x * channels + c]));
                max_diff = std::max(max_diff, diff);
                if (diff > tolerance) {
                    pixels_differ = true;
                }
            }

            // Color code the difference
            if (pixels_differ) {
                // Red for significant differences
                diff_row[x * diff_image.channels() + 0] = 255; // R or B (depending on format)
                diff_row[x * diff_image.channels() + 1] = 0;   // G
                diff_row[x * diff_image.channels() + 2] = 0;   // B or R (depending on format)
            } else if (max_diff > 0) {
                // Yellow for minor differences within tolerance
                diff_row[x * diff_image.channels() + 0] = 255; // R or B
                diff_row[x * diff_image.channels() + 1] = 255; // G
                diff_row[x * diff_image.channels() + 2] = 0;   // B or R
            } else {
                // Green for identical pixels
                diff_row[x * diff_image.channels() + 0] = 0;   // R or B
                diff_row[x * diff_image.channels() + 1] = 255; // G
                diff_row[x * diff_image.channels() + 2] = 0;   // B or R
            }

            // Set alpha channel if present
            if (diff_image.channels() == 4) {
                diff_row[x * diff_image.channels() + 3] = 255;
            }
        }
    }

    return diff_image;
}

void PixelTestUtils::saveDebugImagesOnFailure(const TestImage& img1, const TestImage& img2, const std::string& test_name, int tolerance) {
    // Create safe filename from test name
    std::string safe_test_name = test_name;
    // Replace spaces and special characters with underscores
    std::replace_if(safe_test_name.begin(), safe_test_name.end(), [](char c) { return !std::isalnum(c); }, '_');

    std::cout << "[DEBUG] Saving debug images for failed test: " << test_name << std::endl;

    // Extract backend names from test_name if it contains comparison info
    std::string img1_suffix, img2_suffix;
    if (test_name.find("_CPU_vs_") != std::string::npos) {
        // This is a CPU baseline comparison, img1=backend, img2=CPU
        size_t vs_pos = test_name.find("_CPU_vs_");
        std::string backend_name = test_name.substr(vs_pos + 8); // Extract backend name after "_CPU_vs_"
        
        // Clean up backend name for filename (replace spaces with underscores, remove duplicates)
        std::replace(backend_name.begin(), backend_name.end(), ' ', '_');
        
        // Convert to lowercase for simpler filenames
        std::transform(backend_name.begin(), backend_name.end(), backend_name.begin(), ::tolower);
        
        img1_suffix = backend_name + "_result";
        img2_suffix = "cpu_result";
    } else {
        // Fallback to generic naming
        img1_suffix = "expected";
        img2_suffix = "error_result";
    }

    // Save both original images
    std::string img1_filename = "debug_" + safe_test_name + "_" + img1_suffix;
    std::string img2_filename = "debug_" + safe_test_name + "_" + img2_suffix;
    std::string diff_filename = "debug_" + safe_test_name + "_difference";

    if (saveImageForDebug(img1, img1_filename)) {
        std::cout << "[DEBUG] Saved " << img1_suffix << ": " << img1_filename << ".bmp" << std::endl;
    } else {
        std::cout << "[ERROR] Failed to save " << img1_suffix << " image" << std::endl;
    }

    if (saveImageForDebug(img2, img2_filename)) {
        std::cout << "[DEBUG] Saved " << img2_suffix << ": " << img2_filename << ".bmp" << std::endl;
    } else {
        std::cout << "[ERROR] Failed to save " << img2_suffix << " image" << std::endl;
    }

    // Create and save difference image
    try {
        TestImage diff_image = createDifferenceImage(img1, img2, tolerance);
        if (saveImageForDebug(diff_image, diff_filename)) {
            std::cout << "[DEBUG] Saved difference image: " << diff_filename << ".bmp" << std::endl;
            std::cout << "[DEBUG] Difference image legend: Red=significant diff, Yellow=minor diff, Green=identical" << std::endl;
        } else {
            std::cout << "[ERROR] Failed to save difference image" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[ERROR] Failed to create difference image: " << e.what() << std::endl;
    }

    // Calculate and report statistics
    double mse = calculateMSE(img1.data(), img2.data(), img1.width(), img1.height(), img1.channels(), img1.stride(), img2.stride());
    double psnr = calculatePSNR(img1.data(), img2.data(), img1.width(), img1.height(), img1.channels(), img1.stride(), img2.stride());

    std::cout << "[DEBUG] Image comparison statistics:" << std::endl;
    std::cout << "[DEBUG]   MSE: " << mse << std::endl;
    std::cout << "[DEBUG]   PSNR: " << psnr << " dB" << std::endl;
    std::cout << "[DEBUG]   Tolerance used: " << tolerance << std::endl;
}

} // namespace ccap_test
