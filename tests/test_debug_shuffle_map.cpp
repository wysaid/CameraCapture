/**
 * @file test_debug_shuffle_map.cpp
 * @brief Debug the shuffle map issue
 */

#include "ccap_convert.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace ccap_test;

TEST(ShuffleMapDebugTest, DirectColorShuffleCall)
{
    std::cout << "Testing direct colorShuffle4To3 call..." << std::endl;
    TestImage rgba_img(8, 8, 4);
    TestImage bgr_img(8, 8, 3);

    // Fill with simple data
    uint8_t* rgba_data = rgba_img.data();
    for (int i = 0; i < 8 * 8; ++i)
    {
        rgba_data[i * 4 + 0] = 100; // R
        rgba_data[i * 4 + 1] = 150; // G
        rgba_data[i * 4 + 2] = 200; // B
        rgba_data[i * 4 + 3] = 255; // A
    }

    std::cout << "Using correct 3-element shuffle map..." << std::endl;
    ccap::bgrToRgba(rgba_img.data(), rgba_img.stride(),
                    bgr_img.data(), bgr_img.stride(),
                    8, 8);

    std::cout << "Direct colorShuffle4To3 call completed successfully" << std::endl;

    // Check result
    uint8_t* bgr_data = bgr_img.data();
    std::cout << "First BGR pixel: B=" << (int)bgr_data[0]
              << " G=" << (int)bgr_data[1]
              << " R=" << (int)bgr_data[2] << std::endl;

    EXPECT_EQ(bgr_data[0], 200); // B
    EXPECT_EQ(bgr_data[1], 150); // G
    EXPECT_EQ(bgr_data[2], 100); // R
}
