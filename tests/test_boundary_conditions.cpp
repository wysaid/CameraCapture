/**
 * @file test_boundary_conditions.cpp
 * @brief Tests for SIMD boundary conditions (Issue #30)
 * 
 * This test suite verifies that SIMD implementations correctly handle
 * edge cases where memory reads could extend beyond allocated buffers.
 * 
 * Critical test cases:
 * - 3->3 channel conversions (RGB<->BGR) at boundary widths
 * - 3->4 channel conversions (RGB->RGBA) at boundary widths  
 * - Various image widths that trigger different SIMD path endings
 * 
 * @see https://github.com/wysaid/CameraCapture/issues/30
 */

#include "ccap_convert.h"
#include "test_utils.h"
#include "test_backend_manager.h"
#include <gtest/gtest.h>
#include <vector>

using namespace ccap_test;

/**
 * @brief Test fixture for boundary condition tests
 * 
 * Tests various critical widths that could trigger SIMD boundary issues:
 * - Standard resolutions: 1280, 1920
 * - Near-boundary widths: 1275-1280, 1915-1920
 * - Edge cases that align differently with SIMD vector sizes
 */
class BoundaryConditionTest : public BackendParameterizedTest {
protected:
    void SetUp() override {
        BackendParameterizedTest::SetUp();
    }

    /**
     * @brief Test RGB to BGR conversion at a specific width
     * 
     * This tests the 3->3 channel conversion which:
     * - Processes 5 pixels per iteration (AVX2)
     * - Reads 16 bytes each time (overshooting by 1 byte)
     * - Previously crashed at width=1280 when x=1275
     */
    void testRgbToBgrAtWidth(int width) {
        auto backend = GetParam();
        const int height = 2; // Keep height small, we're testing width boundaries
        
        TestImage src(width, height, 3);
        TestImage dst(width, height, 3);
        
        // Fill with recognizable pattern
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t* pixel = src.data() + y * src.stride() + x * 3;
                pixel[0] = (uint8_t)((x + 0) % 256); // R
                pixel[1] = (uint8_t)((x + 1) % 256); // G
                pixel[2] = (uint8_t)((x + 2) % 256); // B
            }
        }
        
        // This should not crash with the fix
        ccap::rgbToBgr(src.data(), src.stride(), 
                       dst.data(), dst.stride(), 
                       width, height);
        
        // Verify conversion correctness at various positions
        std::vector<int> testPositions;
        testPositions.push_back(0);
        if (width > 1) testPositions.push_back(1);
        if (width > 2) testPositions.push_back(width/2);
        if (width > 2) testPositions.push_back(width-2);
        if (width > 1) testPositions.push_back(width-1);
        
        for (int x : testPositions) {
            if (x < 0 || x >= width) continue;
            
            for (int y = 0; y < height; ++y) {
                const uint8_t* srcPixel = src.data() + y * src.stride() + x * 3;
                const uint8_t* dstPixel = dst.data() + y * dst.stride() + x * 3;
                
                EXPECT_EQ(srcPixel[0], dstPixel[2]) 
                    << "R->B mismatch at (" << x << "," << y << ") width=" << width
                    << " backend: " << BackendTestManager::getBackendName(backend);
                EXPECT_EQ(srcPixel[1], dstPixel[1]) 
                    << "G->G mismatch at (" << x << "," << y << ") width=" << width
                    << " backend: " << BackendTestManager::getBackendName(backend);
                EXPECT_EQ(srcPixel[2], dstPixel[0]) 
                    << "B->R mismatch at (" << x << "," << y << ") width=" << width
                    << " backend: " << BackendTestManager::getBackendName(backend);
            }
        }
    }

    /**
     * @brief Test RGB to RGBA conversion at a specific width
     * 
     * This tests the 3->4 channel conversion which:
     * - Processes 8 pixels per iteration (AVX2)
     * - Reads up to 28 bytes (16 bytes from offset 12)
     * - Previously would crash at width=1280 when x=1272
     */
    void testRgbToRgbaAtWidth(int width) {
        auto backend = GetParam();
        const int height = 2;
        
        TestImage src(width, height, 3);
        TestImage dst(width, height, 4);
        
        // Fill with pattern
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t* pixel = src.data() + y * src.stride() + x * 3;
                pixel[0] = (uint8_t)((x * 3 + 0) % 256);
                pixel[1] = (uint8_t)((x * 3 + 1) % 256);
                pixel[2] = (uint8_t)((x * 3 + 2) % 256);
            }
        }
        
        // This should not crash with the fix
        ccap::rgbToRgba(src.data(), src.stride(), 
                        dst.data(), dst.stride(), 
                        width, height);
        
        // Verify conversion
        std::vector<int> testPositions;
        testPositions.push_back(0);
        if (width > 1) testPositions.push_back(1);
        if (width > 2) testPositions.push_back(width/2);
        if (width > 2) testPositions.push_back(width-2);
        if (width > 1) testPositions.push_back(width-1);
        
        for (int x : testPositions) {
            if (x < 0 || x >= width) continue;
            
            for (int y = 0; y < height; ++y) {
                const uint8_t* srcPixel = src.data() + y * src.stride() + x * 3;
                const uint8_t* dstPixel = dst.data() + y * dst.stride() + x * 4;
                
                EXPECT_EQ(srcPixel[0], dstPixel[0]) 
                    << "R mismatch at (" << x << "," << y << ") width=" << width
                    << " backend: " << BackendTestManager::getBackendName(backend);
                EXPECT_EQ(srcPixel[1], dstPixel[1]) 
                    << "G mismatch at (" << x << "," << y << ") width=" << width
                    << " backend: " << BackendTestManager::getBackendName(backend);
                EXPECT_EQ(srcPixel[2], dstPixel[2]) 
                    << "B mismatch at (" << x << "," << y << ") width=" << width
                    << " backend: " << BackendTestManager::getBackendName(backend);
                EXPECT_EQ(255, dstPixel[3]) 
                    << "A mismatch at (" << x << "," << y << ") width=" << width
                    << " backend: " << BackendTestManager::getBackendName(backend);
            }
        }
    }

    /**
     * @brief Test BGR to RGB conversion (same as RGB to BGR)
     */
    void testBgrToRgbAtWidth(int width) {
        auto backend = GetParam();
        const int height = 2;
        
        TestImage src(width, height, 3);
        TestImage dst(width, height, 3);
        
        src.fillRandom(12345);
        
        ccap::bgrToRgb(src.data(), src.stride(), 
                       dst.data(), dst.stride(), 
                       width, height);
        
        // Spot check
        for (int x : {0, width-1}) {
            const uint8_t* srcPixel = src.data() + x * 3;
            const uint8_t* dstPixel = dst.data() + x * 3;
            EXPECT_EQ(srcPixel[0], dstPixel[2]) << "width=" << width;
            EXPECT_EQ(srcPixel[2], dstPixel[0]) << "width=" << width;
        }
    }

    /**
     * @brief Test BGR to BGRA conversion
     */
    void testBgrToBgraAtWidth(int width) {
        auto backend = GetParam();
        const int height = 2;
        
        TestImage src(width, height, 3);
        TestImage dst(width, height, 4);
        
        src.fillRandom(54321);
        
        ccap::bgrToBgra(src.data(), src.stride(), 
                        dst.data(), dst.stride(), 
                        width, height);
        
        // Verify alpha and spot check colors
        for (int x : {0, width/2, width-1}) {
            const uint8_t* srcPixel = src.data() + x * 3;
            const uint8_t* dstPixel = dst.data() + x * 4;
            EXPECT_EQ(srcPixel[0], dstPixel[0]) << "width=" << width;
            EXPECT_EQ(srcPixel[1], dstPixel[1]) << "width=" << width;
            EXPECT_EQ(srcPixel[2], dstPixel[2]) << "width=" << width;
            EXPECT_EQ(255, dstPixel[3]) << "width=" << width;
        }
    }
};

// Test the exact case reported in issue #30
TEST_P(BoundaryConditionTest, Issue30_1280x720_RGB_to_BGR) {
    testRgbToBgrAtWidth(1280);
}

TEST_P(BoundaryConditionTest, Issue30_1280x720_BGR_to_RGB) {
    testBgrToRgbAtWidth(1280);
}

// Test 3->4 conversion that also had boundary issues
TEST_P(BoundaryConditionTest, Issue30_1280x720_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1280);
}

TEST_P(BoundaryConditionTest, Issue30_1280x720_BGR_to_BGRA) {
    testBgrToBgraAtWidth(1280);
}

// Test 1920x1080 (Full HD)
TEST_P(BoundaryConditionTest, FullHD_1920_RGB_to_BGR) {
    testRgbToBgrAtWidth(1920);
}

TEST_P(BoundaryConditionTest, FullHD_1920_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1920);
}

// Test critical boundary widths for 3->3 conversion (processes 5 pixels at a time)
// These widths are chosen to trigger different remainder cases in SIMD loops
TEST_P(BoundaryConditionTest, Boundary_Width_1275_RGB_to_BGR) {
    testRgbToBgrAtWidth(1275); // Should be last full SIMD iteration for old buggy code
}

TEST_P(BoundaryConditionTest, Boundary_Width_1276_RGB_to_BGR) {
    testRgbToBgrAtWidth(1276);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1277_RGB_to_BGR) {
    testRgbToBgrAtWidth(1277);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1278_RGB_to_BGR) {
    testRgbToBgrAtWidth(1278);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1279_RGB_to_BGR) {
    testRgbToBgrAtWidth(1279);
}

// Test critical boundary widths for 3->4 conversion (processes 8 pixels at a time)
TEST_P(BoundaryConditionTest, Boundary_Width_1272_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1272); // Would crash with old buggy code
}

TEST_P(BoundaryConditionTest, Boundary_Width_1273_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1273);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1274_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1274);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1278_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1278);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1279_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1279);
}

// Test widths around 1920 (Full HD)
TEST_P(BoundaryConditionTest, Boundary_Width_1915_RGB_to_BGR) {
    testRgbToBgrAtWidth(1915);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1916_RGB_to_BGR) {
    testRgbToBgrAtWidth(1916);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1917_RGB_to_BGR) {
    testRgbToBgrAtWidth(1917);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1918_RGB_to_BGR) {
    testRgbToBgrAtWidth(1918);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1919_RGB_to_BGR) {
    testRgbToBgrAtWidth(1919);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1912_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1912); // 1920-8, critical for 3->4
}

TEST_P(BoundaryConditionTest, Boundary_Width_1918_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1918);
}

TEST_P(BoundaryConditionTest, Boundary_Width_1919_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1919);
}

// Test some odd widths to ensure robustness
TEST_P(BoundaryConditionTest, OddWidth_1281_RGB_to_BGR) {
    testRgbToBgrAtWidth(1281);
}

TEST_P(BoundaryConditionTest, OddWidth_1283_RGB_to_BGR) {
    testRgbToBgrAtWidth(1283);
}

TEST_P(BoundaryConditionTest, OddWidth_1285_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1285);
}

TEST_P(BoundaryConditionTest, OddWidth_1287_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(1287);
}

// Small widths to test edge cases
TEST_P(BoundaryConditionTest, SmallWidth_5_RGB_to_BGR) {
    testRgbToBgrAtWidth(5); // Exactly one SIMD iteration for 3->3
}

TEST_P(BoundaryConditionTest, SmallWidth_6_RGB_to_BGR) {
    testRgbToBgrAtWidth(6); // One pixel past SIMD
}

TEST_P(BoundaryConditionTest, SmallWidth_8_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(8); // Exactly one SIMD iteration for 3->4
}

TEST_P(BoundaryConditionTest, SmallWidth_9_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(9); // One pixel past SIMD
}

TEST_P(BoundaryConditionTest, SmallWidth_10_RGB_to_RGBA) {
    testRgbToRgbaAtWidth(10); // Two pixels past SIMD
}

// Very small widths (should use scalar fallback entirely)
TEST_P(BoundaryConditionTest, TinyWidth_1_RGB_to_BGR) {
    testRgbToBgrAtWidth(1);
}

TEST_P(BoundaryConditionTest, TinyWidth_2_RGB_to_BGR) {
    testRgbToBgrAtWidth(2);
}

TEST_P(BoundaryConditionTest, TinyWidth_3_RGB_to_BGR) {
    testRgbToBgrAtWidth(3);
}

TEST_P(BoundaryConditionTest, TinyWidth_4_RGB_to_BGR) {
    testRgbToBgrAtWidth(4);
}

// Test with stride != width * channels (non-contiguous memory)
TEST_P(BoundaryConditionTest, NonContiguous_1280_RGB_to_BGR) {
    auto backend = GetParam();
    const int width = 1280;
    const int height = 2;
    const int srcStride = width * 3 + 32; // Add padding
    const int dstStride = width * 3 + 16; // Different padding
    
    std::vector<uint8_t> srcBuffer(srcStride * height);
    std::vector<uint8_t> dstBuffer(dstStride * height);
    
    // Fill source
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = srcBuffer.data() + y * srcStride + x * 3;
            pixel[0] = (uint8_t)((x + 0) % 256);
            pixel[1] = (uint8_t)((x + 1) % 256);
            pixel[2] = (uint8_t)((x + 2) % 256);
        }
    }
    
    // Convert
    ccap::rgbToBgr(srcBuffer.data(), srcStride, 
                   dstBuffer.data(), dstStride, 
                   width, height);
    
    // Verify
    for (int y = 0; y < height; ++y) {
        for (int x : {0, width-1}) {
            const uint8_t* srcPixel = srcBuffer.data() + y * srcStride + x * 3;
            const uint8_t* dstPixel = dstBuffer.data() + y * dstStride + x * 3;
            EXPECT_EQ(srcPixel[0], dstPixel[2]) << "x=" << x << " y=" << y;
            EXPECT_EQ(srcPixel[2], dstPixel[0]) << "x=" << x << " y=" << y;
        }
    }
}

TEST_P(BoundaryConditionTest, NonContiguous_1280_RGB_to_RGBA) {
    auto backend = GetParam();
    const int width = 1280;
    const int height = 2;
    const int srcStride = width * 3 + 64;
    const int dstStride = width * 4 + 32;
    
    std::vector<uint8_t> srcBuffer(srcStride * height);
    std::vector<uint8_t> dstBuffer(dstStride * height);
    
    // Fill and convert
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = srcBuffer.data() + y * srcStride + x * 3;
            pixel[0] = (uint8_t)(x % 256);
            pixel[1] = (uint8_t)((x + 1) % 256);
            pixel[2] = (uint8_t)((x + 2) % 256);
        }
    }
    
    ccap::rgbToRgba(srcBuffer.data(), srcStride, 
                    dstBuffer.data(), dstStride, 
                    width, height);
    
    // Verify alpha and some pixels
    for (int y = 0; y < height; ++y) {
        for (int x : {0, width/2, width-1}) {
            const uint8_t* dstPixel = dstBuffer.data() + y * dstStride + x * 4;
            EXPECT_EQ(255, dstPixel[3]) << "Alpha at x=" << x << " y=" << y;
        }
    }
}

INSTANTIATE_BACKEND_TEST(BoundaryConditionTest);
