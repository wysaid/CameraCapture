/**
 * @file test_debug_simple.cpp
 * @brief Simple debug test to isolate the segfault issue
 */

#include "ccap_convert.h"
#include "test_utils.h"

#include <gtest/gtest.h>

using namespace ccap_test;

TEST(DebugTest, TestImageConstruction) {
    // Test simple TestImage construction
    TestImage img(64, 64, 4);

    // Test data access
    uint8_t* data = img.data();
    ASSERT_NE(data, nullptr);

    // Test basic memory write
    data[0] = 255;
    EXPECT_EQ(data[0], 255);
}

TEST(DebugTest, TestImageBasicOperations) {
    TestImage img(64, 64, 3);

    // Test stride and size
    EXPECT_GT(img.stride(), 0);
    EXPECT_GT(img.size(), 0);
    EXPECT_EQ(img.width(), 64);
    EXPECT_EQ(img.height(), 64);
    EXPECT_EQ(img.channels(), 3);
}

TEST(DebugTest, SimpleRgbaToBgrCall) {
    TestImage rgba_img(8, 8, 4);
    TestImage bgr_img(8, 8, 3);

    // Fill with some simple data
    uint8_t* rgba_data = rgba_img.data();
    for (int i = 0; i < 8 * 8 * 4; i += 4) {
        rgba_data[i + 0] = 255; // R
        rgba_data[i + 1] = 128; // G
        rgba_data[i + 2] = 64;  // B
        rgba_data[i + 3] = 255; // A
    }

    // Try the conversion
    ccap::rgbaToBgr(rgba_img.data(), rgba_img.stride(), bgr_img.data(), bgr_img.stride(), 8, 8);

    // Check one pixel
    const uint8_t* bgr_data = bgr_img.data();
    EXPECT_EQ(bgr_data[0], 64);  // B
    EXPECT_EQ(bgr_data[1], 128); // G
    EXPECT_EQ(bgr_data[2], 255); // R
}
