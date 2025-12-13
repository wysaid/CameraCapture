/**
 * @file test_frame_conversions.cpp
 * @brief Tests for VideoFrame conversion functions (inplaceConvertFrame and related APIs)
 * @note These tests cover the high-level frame conversion API that uses PixelFormat enums
 */

#include "ccap_convert_frame.h"
#include "ccap_core.h"
#include "test_utils.h"
#include "test_backend_manager.h"
#include <gtest/gtest.h>
#include <memory>

using namespace ccap_test;

// Helper function to create a VideoFrame for testing
// Returns a frame with data pointing to EXTERNAL memory (simulating camera buffer)
// This matches the design constraint for inplaceConvertFrame*() functions
std::unique_ptr<ccap::VideoFrame> createTestFrame(int width, int height, ccap::PixelFormat pixelFormat) {
    auto frame = std::make_unique<ccap::VideoFrame>();
    frame->width = width;
    frame->height = height;
    frame->pixelFormat = pixelFormat;
    frame->orientation = ccap::FrameOrientation::TopToBottom;
    
    int channels = 3;
    if (pixelFormat == ccap::PixelFormat::RGBA32 || pixelFormat == ccap::PixelFormat::BGRA32) {
        channels = 4;
    }
    
    frame->stride[0] = width * channels;
    
    // Create allocator but DON'T allocate memory yet
    // This simulates the real-world scenario where frame->data[0] points to external memory
    frame->allocator = std::make_shared<ccap::DefaultAllocator>();
    
    // Allocate external buffer (simulating camera buffer)
    // In real usage, this would be the camera's buffer
    static std::vector<std::vector<uint8_t>> external_buffers;
    external_buffers.push_back(std::vector<uint8_t>(frame->stride[0] * height, 0xDD));
    frame->data[0] = external_buffers.back().data();
    
    return frame;
}

// ============ Frame RGB/BGR Conversion Tests ============

class FrameRGBBGRConversionTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
        width_ = 64;
        height_ = 64;
    }

    int width_;
    int height_;
};

TEST_P(FrameRGBBGRConversionTest, RGB24_To_BGR24_InplaceConversion) {
    auto backend = GetParam();
    
    auto frame = createTestFrame(width_, height_, ccap::PixelFormat::RGB24);
    
    // Fill with test pattern
    TestImage test_pattern(width_, height_, 3);
    test_pattern.fillGradient();
    memcpy(frame->data[0], test_pattern.data(), width_ * height_ * 3);
    
    // Store original data for verification
    std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + width_ * height_ * 3);
    
    // Convert RGB24 -> BGR24
    bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::BGR24, false);
    ASSERT_TRUE(success) << "RGB24 to BGR24 conversion failed, backend: " 
                         << BackendTestManager::getBackendName(backend);
    
    EXPECT_EQ(frame->pixelFormat, ccap::PixelFormat::BGR24) 
        << "Frame pixel format not updated";
    
    // Verify R and B channels are swapped
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int offset = y * frame->stride[0] + x * 3;
            EXPECT_EQ(original_data[offset + 0], frame->data[0][offset + 2]) 
                << "R->B swap failed at (" << x << "," << y << "), backend: " 
                << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(original_data[offset + 1], frame->data[0][offset + 1]) 
                << "G should remain unchanged at (" << x << "," << y << "), backend: " 
                << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(original_data[offset + 2], frame->data[0][offset + 0]) 
                << "B->R swap failed at (" << x << "," << y << "), backend: " 
                << BackendTestManager::getBackendName(backend);
        }
    }
}

TEST_P(FrameRGBBGRConversionTest, BGR24_To_RGB24_InplaceConversion) {
    auto backend = GetParam();
    
    auto frame = createTestFrame(width_, height_, ccap::PixelFormat::BGR24);
    
    // Fill with test pattern
    TestImage test_pattern(width_, height_, 3);
    test_pattern.fillRandom(42);
    memcpy(frame->data[0], test_pattern.data(), width_ * height_ * 3);
    
    // Store original data for verification
    std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + width_ * height_ * 3);
    
    // Convert BGR24 -> RGB24
    bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::RGB24, false);
    ASSERT_TRUE(success) << "BGR24 to RGB24 conversion failed, backend: " 
                         << BackendTestManager::getBackendName(backend);
    
    EXPECT_EQ(frame->pixelFormat, ccap::PixelFormat::RGB24) 
        << "Frame pixel format not updated";
    
    // Verify channels are swapped (should be symmetric operation)
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int offset = y * frame->stride[0] + x * 3;
            EXPECT_EQ(original_data[offset + 0], frame->data[0][offset + 2]) 
                << "Channel swap failed at (" << x << "," << y << "), backend: " 
                << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(original_data[offset + 1], frame->data[0][offset + 1]) 
                << "G should remain unchanged at (" << x << "," << y << "), backend: " 
                << BackendTestManager::getBackendName(backend);
            EXPECT_EQ(original_data[offset + 2], frame->data[0][offset + 0]) 
                << "Channel swap failed at (" << x << "," << y << "), backend: " 
                << BackendTestManager::getBackendName(backend);
        }
    }
}

TEST_P(FrameRGBBGRConversionTest, RGBA32_To_BGRA32_InplaceConversion) {
    auto backend = GetParam();
    
    auto frame = createTestFrame(width_, height_, ccap::PixelFormat::RGBA32);
    
    // Fill with test pattern
    TestImage test_pattern(width_, height_, 4);
    test_pattern.fillChecker(100, 200);
    memcpy(frame->data[0], test_pattern.data(), width_ * height_ * 4);
    
    // Store original data for verification
    std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + width_ * height_ * 4);
    
    // Convert RGBA32 -> BGRA32
    bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::BGRA32, false);
    ASSERT_TRUE(success) << "RGBA32 to BGRA32 conversion failed, backend: " 
                         << BackendTestManager::getBackendName(backend);
    
    EXPECT_EQ(frame->pixelFormat, ccap::PixelFormat::BGRA32) 
        << "Frame pixel format not updated";
    
    // Verify R/B channels swapped, alpha preserved
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int offset = y * frame->stride[0] + x * 4;
            EXPECT_EQ(original_data[offset + 0], frame->data[0][offset + 2]) 
                << "R->B swap failed at (" << x << "," << y << ")";
            EXPECT_EQ(original_data[offset + 1], frame->data[0][offset + 1]) 
                << "G should remain unchanged at (" << x << "," << y << ")";
            EXPECT_EQ(original_data[offset + 2], frame->data[0][offset + 0]) 
                << "B->R swap failed at (" << x << "," << y << ")";
            EXPECT_EQ(original_data[offset + 3], frame->data[0][offset + 3]) 
                << "Alpha should remain unchanged at (" << x << "," << y << ")";
        }
    }
}

TEST_P(FrameRGBBGRConversionTest, RGB24_To_BGRA32_InplaceConversion) {
    auto backend = GetParam();
    
    auto frame = createTestFrame(width_, height_, ccap::PixelFormat::RGB24);
    
    // Fill with test pattern
    TestImage test_pattern(width_, height_, 3);
    test_pattern.fillGradient();
    memcpy(frame->data[0], test_pattern.data(), width_ * height_ * 3);
    
    // Store original data for verification
    std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + width_ * height_ * 3);
    
    // Convert RGB24 -> BGRA32
    bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::BGRA32, false);
    ASSERT_TRUE(success) << "RGB24 to BGRA32 conversion failed, backend: " 
                         << BackendTestManager::getBackendName(backend);
    
    EXPECT_EQ(frame->pixelFormat, ccap::PixelFormat::BGRA32) 
        << "Frame pixel format not updated";
    
    // Verify RGB->BGRA conversion (swap + add alpha)
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int src_offset = y * test_pattern.stride() + x * 3;
            int dst_offset = y * frame->stride[0] + x * 4;
            
            EXPECT_EQ(original_data[src_offset + 0], frame->data[0][dst_offset + 2]) 
                << "R->B conversion failed at (" << x << "," << y << ")";
            EXPECT_EQ(original_data[src_offset + 1], frame->data[0][dst_offset + 1]) 
                << "G should be preserved at (" << x << "," << y << ")";
            EXPECT_EQ(original_data[src_offset + 2], frame->data[0][dst_offset + 0]) 
                << "B->R conversion failed at (" << x << "," << y << ")";
            EXPECT_EQ(255, frame->data[0][dst_offset + 3]) 
                << "Alpha should be 255 at (" << x << "," << y << ")";
        }
    }
}

TEST_P(FrameRGBBGRConversionTest, BGRA32_To_RGB24_InplaceConversion) {
    auto backend = GetParam();
    
    auto frame = createTestFrame(width_, height_, ccap::PixelFormat::BGRA32);
    
    // Fill with test pattern
    TestImage test_pattern(width_, height_, 4);
    test_pattern.fillRandom(999);
    memcpy(frame->data[0], test_pattern.data(), width_ * height_ * 4);
    
    // Store original data for verification
    std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + width_ * height_ * 4);
    
    // Convert BGRA32 -> RGB24
    bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::RGB24, false);
    ASSERT_TRUE(success) << "BGRA32 to RGB24 conversion failed, backend: " 
                         << BackendTestManager::getBackendName(backend);
    
    EXPECT_EQ(frame->pixelFormat, ccap::PixelFormat::RGB24) 
        << "Frame pixel format not updated";
    
    // Verify BGRA->RGB conversion (swap + remove alpha)
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int src_offset = y * test_pattern.stride() + x * 4;
            int dst_offset = y * frame->stride[0] + x * 3;
            
            EXPECT_EQ(original_data[src_offset + 0], frame->data[0][dst_offset + 2]) 
                << "B->R conversion failed at (" << x << "," << y << ")";
            EXPECT_EQ(original_data[src_offset + 1], frame->data[0][dst_offset + 1]) 
                << "G should be preserved at (" << x << "," << y << ")";
            EXPECT_EQ(original_data[src_offset + 2], frame->data[0][dst_offset + 0]) 
                << "R->B conversion failed at (" << x << "," << y << ")";
        }
    }
}

INSTANTIATE_BACKEND_TEST(FrameRGBBGRConversionTest);

// ============ Large Frame Tests (Stress Testing) ============

class LargeFrameConversionTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
    }
};

TEST_P(LargeFrameConversionTest, Large_1280x720_RGB24_To_BGR24) {
    auto backend = GetParam();
    const int width = 1280;
    const int height = 720;
    
    auto frame = createTestFrame(width, height, ccap::PixelFormat::RGB24);
    
    // Fill with gradient pattern
    TestImage test_pattern(width, height, 3);
    test_pattern.fillGradient();
    memcpy(frame->data[0], test_pattern.data(), width * height * 3);
    
    // Store original data for verification
    std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + width * height * 3);
    
    // Convert RGB24 -> BGR24
    auto start = std::chrono::high_resolution_clock::now();
    bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::BGR24, false);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    ASSERT_TRUE(success) << "1280x720 RGB24 to BGR24 conversion failed, backend: " 
                         << BackendTestManager::getBackendName(backend);
    
    // Performance check - should complete in reasonable time
    EXPECT_LT(duration.count(), 500) << "Conversion too slow for 1280x720, backend: " 
                                     << BackendTestManager::getBackendName(backend);
    
    // Verify correctness for multiple points across the image
    // Check corners, center, and various positions to ensure AVX2 code works correctly
    std::vector<std::pair<int, int>> test_points = {
        {0, 0},              // Top-left corner
        {width-1, 0},        // Top-right corner
        {0, height-1},       // Bottom-left corner
        {width-1, height-1}, // Bottom-right corner
        {width/2, height/2}, // Center
        {10, 10},            // Early in scan (before AVX2 alignment)
        {width/3, height/3}, // Middle-ish area
        {width*2/3, height*2/3}, // Another middle area
    };
    
    for (const auto& point : test_points) {
        int x = point.first;
        int y = point.second;
        int offset = y * frame->stride[0] + x * 3;
        
        EXPECT_EQ(original_data[offset + 0], frame->data[0][offset + 2]) 
            << "R->B swap failed at (" << x << "," << y << "), backend: " 
            << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(original_data[offset + 1], frame->data[0][offset + 1]) 
            << "G unchanged failed at (" << x << "," << y << "), backend: " 
            << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(original_data[offset + 2], frame->data[0][offset + 0]) 
            << "B->R swap failed at (" << x << "," << y << "), backend: " 
            << BackendTestManager::getBackendName(backend);
    }
    
    // Also verify a full row in the middle to catch any AVX2 patchSize issues
    int mid_row = height / 2;
    for (int x = 0; x < width; ++x) {
        int offset = mid_row * frame->stride[0] + x * 3;
        EXPECT_EQ(original_data[offset + 0], frame->data[0][offset + 2]) 
            << "Full row check: R->B swap failed at x=" << x << ", backend: " 
            << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(original_data[offset + 1], frame->data[0][offset + 1]) 
            << "Full row check: G unchanged failed at x=" << x << ", backend: " 
            << BackendTestManager::getBackendName(backend);
        EXPECT_EQ(original_data[offset + 2], frame->data[0][offset + 0]) 
            << "Full row check: B->R swap failed at x=" << x << ", backend: " 
            << BackendTestManager::getBackendName(backend);
    }
}

TEST_P(LargeFrameConversionTest, Large_1920x1080_BGR24_To_RGB24) {
    auto backend = GetParam();
    const int width = 1920;
    const int height = 1080;
    
    auto frame = createTestFrame(width, height, ccap::PixelFormat::BGR24);
    
    // Fill with random pattern
    TestImage test_pattern(width, height, 3);
    test_pattern.fillRandom(12345);
    memcpy(frame->data[0], test_pattern.data(), width * height * 3);
    
    // Store original data for verification
    std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + width * height * 3);
    
    // Convert BGR24 -> RGB24
    auto start = std::chrono::high_resolution_clock::now();
    bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::RGB24, false);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    ASSERT_TRUE(success) << "1920x1080 BGR24 to RGB24 conversion failed, backend: " 
                         << BackendTestManager::getBackendName(backend);
    
    // Performance check
    EXPECT_LT(duration.count(), 1000) << "Conversion too slow for 1920x1080, backend: " 
                                      << BackendTestManager::getBackendName(backend);
    
    // Verify correctness for sample points
    std::vector<std::pair<int, int>> test_points = {
        {0, 0}, {width-1, 0}, {0, height-1}, {width-1, height-1},
        {width/2, height/2}, {15, 15}, {width/3, height/3}
    };
    
    for (const auto& point : test_points) {
        int x = point.first;
        int y = point.second;
        int offset = y * frame->stride[0] + x * 3;
        
        EXPECT_EQ(original_data[offset + 0], frame->data[0][offset + 2]) 
            << "Channel swap failed at (" << x << "," << y << ")";
        EXPECT_EQ(original_data[offset + 1], frame->data[0][offset + 1]) 
            << "G unchanged failed at (" << x << "," << y << ")";
        EXPECT_EQ(original_data[offset + 2], frame->data[0][offset + 0]) 
            << "Channel swap failed at (" << x << "," << y << ")";
    }
}

INSTANTIATE_BACKEND_TEST(LargeFrameConversionTest);

// ============ Edge Case Tests ============

class FrameConversionEdgeCaseTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
    }
};

TEST_P(FrameConversionEdgeCaseTest, Odd_Width_RGB24_To_BGR24) {
    auto backend = GetParam();
    
    // Odd widths can cause alignment issues with SIMD code
    std::vector<int> odd_widths = {1, 3, 5, 7, 11, 13, 17, 31, 63, 127};
    
    for (int width : odd_widths) {
        int height = 8;
        auto frame = createTestFrame(width, height, ccap::PixelFormat::RGB24);
        
        TestImage test_pattern(width, height, 3);
        test_pattern.fillRandom(width); // Different seed for each width
        memcpy(frame->data[0], test_pattern.data(), width * height * 3);
        
        // Save original stride before conversion (might change after conversion due to alignment)
        int original_stride = frame->stride[0];
        std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + original_stride * height);
        
        bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::BGR24, false);
        ASSERT_TRUE(success) << "Conversion failed for width=" << width << ", backend: " 
                             << BackendTestManager::getBackendName(backend);
        
        // Verify all pixels
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Use original stride for original_data, new stride for frame->data
                int original_offset = y * original_stride + x * 3;
                int new_offset = y * frame->stride[0] + x * 3;
                EXPECT_EQ(original_data[original_offset + 0], frame->data[0][new_offset + 2]) 
                    << "Failed at width=" << width << ", pos=(" << x << "," << y << ")";
                EXPECT_EQ(original_data[original_offset + 1], frame->data[0][new_offset + 1]) 
                    << "Failed at width=" << width << ", pos=(" << x << "," << y << ")";
                EXPECT_EQ(original_data[original_offset + 2], frame->data[0][new_offset + 0]) 
                    << "Failed at width=" << width << ", pos=(" << x << "," << y << ")";
            }
        }
    }
}

TEST_P(FrameConversionEdgeCaseTest, Non_Multiple_Of_Patch_Size_Widths) {
    auto backend = GetParam();
    
    // Test widths that are not multiples of common SIMD patch sizes (5, 8, 10, 16)
    // These should trigger the scalar fallback code path
    std::vector<int> test_widths = {
        6, 9, 14, 18, 23, 33, 47, 62, 77, 99, 
        100, 127, 255, 319, 639, 641, 1279, 1281
    };
    
    for (int width : test_widths) {
        int height = 4;
        auto frame = createTestFrame(width, height, ccap::PixelFormat::RGB24);
        
        TestImage test_pattern(width, height, 3);
        test_pattern.fillGradient();
        memcpy(frame->data[0], test_pattern.data(), width * height * 3);
        
        // Save original stride before conversion
        int original_stride = frame->stride[0];
        std::vector<uint8_t> original_data(frame->data[0], frame->data[0] + original_stride * height);
        
        bool success = ccap::inplaceConvertFrameRGB(frame.get(), ccap::PixelFormat::BGR24, false);
        ASSERT_TRUE(success) << "Conversion failed for width=" << width << ", backend: " 
                             << BackendTestManager::getBackendName(backend);
        
        // Verify last few pixels (most likely to have issues)
        for (int x = std::max(0, width - 10); x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                // Use original stride for original_data, new stride for frame->data
                int original_offset = y * original_stride + x * 3;
                int new_offset = y * frame->stride[0] + x * 3;
                EXPECT_EQ(original_data[original_offset + 0], frame->data[0][new_offset + 2]) 
                    << "Failed at width=" << width << ", pos=(" << x << "," << y << ")";
                EXPECT_EQ(original_data[original_offset + 1], frame->data[0][new_offset + 1]) 
                    << "Failed at width=" << width << ", pos=(" << x << "," << y << ")";
                EXPECT_EQ(original_data[original_offset + 2], frame->data[0][new_offset + 0]) 
                    << "Failed at width=" << width << ", pos=(" << x << "," << y << ")";
            }
        }
    }
}

INSTANTIATE_BACKEND_TEST(FrameConversionEdgeCaseTest);
