/**
 * @file test_utils.cpp
 * @brief Implementation of utility functions for ccap unit tests
 */

#include "test_utils.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <cstdlib>

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

TestImage::TestImage(int width, int height, int channels, int alignment)
    : width_(width), height_(height), channels_(channels), data_(nullptr, [](void* p) {
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
#ifdef _WIN32
    uint8_t* ptr = static_cast<uint8_t*>(_aligned_malloc(size_, alignment));
#else
    uint8_t* ptr = nullptr;
    posix_memalign(reinterpret_cast<void**>(&ptr), alignment, size_);
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

TestYUVImage::TestYUVImage(int width, int height, bool isNV12)
    : width_(width), height_(height), isNV12_(isNV12),
      y_plane_(width, height, 1, 32),
      u_plane_(width/2, height/2, 1, 32),
      v_plane_(width/2, height/2, 1, 32),
      uv_plane_(width/2, height/2, 2, 32) {
    
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
    for (int y = 0; y < height_/2; ++y) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_/2; ++x) {
                uv_row[x * 2 + 0] = static_cast<uint8_t>(128 + (x % 64) - 32); // U
                uv_row[x * 2 + 1] = static_cast<uint8_t>(128 + (y % 64) - 32); // V
            }
        } else {
            uint8_t* u_row = u_plane_.data() + y * uv_stride_;
            uint8_t* v_row = v_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_/2; ++x) {
                u_row[x] = static_cast<uint8_t>(128 + (x % 64) - 32);
                v_row[x] = static_cast<uint8_t>(128 + (y % 64) - 32);
            }
        }
    }
}

void TestYUVImage::generateGradient() {
    // Y plane gradient
    for (int y = 0; y < height_; ++y) {
        uint8_t* y_row = y_plane_.data() + y * y_stride_;
        for (int x = 0; x < width_; ++x) {
            y_row[x] = static_cast<uint8_t>(16 + (x * 219 / width_));
        }
    }
    
    // UV planes with color variation
    for (int y = 0; y < height_/2; ++y) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_/2; ++x) {
                uv_row[x * 2 + 0] = static_cast<uint8_t>(64 + (x * 128 / (width_/2))); // U
                uv_row[x * 2 + 1] = static_cast<uint8_t>(64 + (y * 128 / (height_/2))); // V
            }
        } else {
            uint8_t* u_row = u_plane_.data() + y * uv_stride_;
            uint8_t* v_row = v_plane_.data() + y * uv_stride_;
            for (int x = 0; x < width_/2; ++x) {
                u_row[x] = static_cast<uint8_t>(64 + (x * 128 / (width_/2)));
                v_row[x] = static_cast<uint8_t>(64 + (y * 128 / (height_/2)));
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
    for (int row = 0; row < height_/2; ++row) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + row * uv_stride_;
            for (int x = 0; x < width_/2; ++x) {
                uv_row[x * 2 + 0] = u;
                uv_row[x * 2 + 1] = v;
            }
        } else {
            uint8_t* u_row = u_plane_.data() + row * uv_stride_;
            uint8_t* v_row = v_plane_.data() + row * uv_stride_;
            std::fill(u_row, u_row + width_/2, u);
            std::fill(v_row, v_row + width_/2, v);
        }
    }
}

void TestYUVImage::generateRandomYUVImage(int width, int height, uint32_t seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint8_t> y_dist(16, 235);  // Y range for BT.601
    std::uniform_int_distribution<uint8_t> uv_dist(16, 240); // UV range for BT.601
    
    // Fill Y plane with random values
    for (int row = 0; row < height_; ++row) {
        uint8_t* y_row = y_plane_.data() + row * y_stride_;
        for (int col = 0; col < width_; ++col) {
            y_row[col] = y_dist(gen);
        }
    }
    
    // Fill UV planes with random values
    for (int row = 0; row < height_/2; ++row) {
        if (isNV12_) {
            uint8_t* uv_row = uv_plane_.data() + row * uv_stride_;
            for (int x = 0; x < width_/2; ++x) {
                uv_row[x * 2 + 0] = uv_dist(gen);  // U
                uv_row[x * 2 + 1] = uv_dist(gen);  // V
            }
        } else {
            uint8_t* u_row = u_plane_.data() + row * uv_stride_;
            uint8_t* v_row = v_plane_.data() + row * uv_stride_;
            for (int x = 0; x < width_/2; ++x) {
                u_row[x] = uv_dist(gen);
                v_row[x] = uv_dist(gen);
            }
        }
    }
}

// ============ PixelTestUtils Implementation ============

bool PixelTestUtils::compareImages(const uint8_t* img1, const uint8_t* img2,
                                  int width, int height, int channels,
                                  int stride1, int stride2, int tolerance) {
    for (int y = 0; y < height; ++y) {
        const uint8_t* row1 = img1 + y * stride1;
        const uint8_t* row2 = img2 + y * stride2;
        
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                int diff = std::abs(static_cast<int>(row1[x * channels + c]) - 
                                   static_cast<int>(row2[x * channels + c]));
                if (diff > tolerance) {
                    return false;
                }
            }
        }
    }
    return true;
}

double PixelTestUtils::calculateMSE(const uint8_t* img1, const uint8_t* img2,
                                   int width, int height, int channels,
                                   int stride1, int stride2) {
    double mse = 0.0;
    int total_pixels = 0;
    
    for (int y = 0; y < height; ++y) {
        const uint8_t* row1 = img1 + y * stride1;
        const uint8_t* row2 = img2 + y * stride2;
        
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                double diff = static_cast<double>(row1[x * channels + c]) - 
                             static_cast<double>(row2[x * channels + c]);
                mse += diff * diff;
                total_pixels++;
            }
        }
    }
    
    return mse / total_pixels;
}

double PixelTestUtils::calculatePSNR(const uint8_t* img1, const uint8_t* img2,
                                     int width, int height, int channels,
                                     int stride1, int stride2) {
    double mse = calculateMSE(img1, img2, width, height, channels, stride1, stride2);
    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    
    double max_pixel_value = 255.0;
    return 20.0 * std::log10(max_pixel_value / std::sqrt(mse));
}

bool PixelTestUtils::isValidRGB(int r, int g, int b) {
    return r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255;
}

void PixelTestUtils::yuv2rgbReference(int y, int u, int v, int& r, int& g, int& b,
                                     bool isBT709, bool isFullRange) {
    // Reference implementation for comparison
    if (!isFullRange) {
        y = y - 16;  // video range
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

void PerformanceTimer::start() {
    start_time_ = std::chrono::high_resolution_clock::now();
}

double PerformanceTimer::stopAndGetMs() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
    return duration.count() / 1000.0; // Convert to milliseconds
}

// ============ TestDataGenerator Implementation ============

std::vector<std::tuple<int, int, int>> TestDataGenerator::getTestYUVValues() {
    return {
        {16, 128, 128},    // Video black
        {235, 128, 128},   // Video white
        {0, 128, 128},     // Full black
        {255, 128, 128},   // Full white
        {128, 0, 0},       // Min UV
        {128, 255, 255},   // Max UV
        {81, 90, 240},     // Red
        {145, 54, 34},     // Green
        {41, 240, 110},    // Blue
        {210, 16, 146},    // Yellow
        {170, 166, 16},    // Cyan
        {106, 202, 222},   // Magenta
    };
}

std::vector<std::pair<int, int>> TestDataGenerator::getTestImageSizes() {
    return {
        {4, 4},       // Minimum
        {16, 16},     // Small
        {64, 64},     // Medium
        {128, 96},    // Common aspect ratio
        {256, 256},   // Large square
        {320, 240},   // QVGA
        {640, 480},   // VGA
        {1280, 720},  // HD
    };
}

TestYUVImage TestDataGenerator::generateRandomYUVImage(int width, int height, uint32_t seed) {
    TestYUVImage yuv_img(width, height, true); // Default to NV12
    
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> y_dis(16, 235);  // Video range Y
    std::uniform_int_distribution<> uv_dis(16, 240); // UV range
    
    // Fill Y plane
    for (int y = 0; y < height; ++y) {
        uint8_t* y_row = yuv_img.y_data() + y * yuv_img.y_stride();
        for (int x = 0; x < width; ++x) {
            y_row[x] = static_cast<uint8_t>(y_dis(gen));
        }
    }
    
    // Fill UV plane (NV12 format)
    for (int y = 0; y < height/2; ++y) {
        uint8_t* uv_row = yuv_img.uv_data() + y * yuv_img.uv_stride();
        for (int x = 0; x < width/2; ++x) {
            uv_row[x * 2 + 0] = static_cast<uint8_t>(uv_dis(gen)); // U
            uv_row[x * 2 + 1] = static_cast<uint8_t>(uv_dis(gen)); // V
        }
    }
    
    return yuv_img;
}

} // namespace ccap_test
