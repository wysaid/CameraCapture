/**
 * @file test_debug_detailed.cpp
 * @brief Detailed debugging of ColorShuffleTest segfault
 */

#include "ccap_convert.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>

using namespace ccap_test;

TEST(DebugTest, TestImageCreation)
{
    std::cout << "Creating TestImage 64x64x4..." << std::endl;
    TestImage rgba_img(64, 64, 4);
    std::cout << "RGBA Image created successfully" << std::endl;
    std::cout << "Width: " << rgba_img.width() << std::endl;
    std::cout << "Height: " << rgba_img.height() << std::endl;
    std::cout << "Channels: " << rgba_img.channels() << std::endl;
    std::cout << "Stride: " << rgba_img.stride() << std::endl;
    std::cout << "Size: " << rgba_img.size() << std::endl;
    std::cout << "Data pointer: " << (void*)rgba_img.data() << std::endl;

    std::cout << "Creating TestImage 64x64x3..." << std::endl;
    TestImage bgr_img(64, 64, 3);
    std::cout << "BGR Image created successfully" << std::endl;
    std::cout << "Width: " << bgr_img.width() << std::endl;
    std::cout << "Height: " << bgr_img.height() << std::endl;
    std::cout << "Channels: " << bgr_img.channels() << std::endl;
    std::cout << "Stride: " << bgr_img.stride() << std::endl;
    std::cout << "Size: " << bgr_img.size() << std::endl;
    std::cout << "Data pointer: " << (void*)bgr_img.data() << std::endl;
}

TEST(DebugTest, BasicMemoryAccess)
{
    std::cout << "Testing basic memory access..." << std::endl;
    TestImage rgba_img(64, 64, 4);

    // Try to access first pixel
    std::cout << "Accessing first pixel..." << std::endl;
    uint8_t* data = rgba_img.data();
    data[0] = 100; // R
    data[1] = 150; // G
    data[2] = 200; // B
    data[3] = 255; // A
    std::cout << "First pixel set successfully" << std::endl;

    // Try to access last pixel
    std::cout << "Accessing last pixel..." << std::endl;
    int last_offset = (rgba_img.height() - 1) * rgba_img.stride() + (rgba_img.width() - 1) * 4;
    std::cout << "Last pixel offset: " << last_offset << std::endl;
    std::cout << "Total size: " << rgba_img.size() << std::endl;

    if (last_offset + 3 < rgba_img.size()) {
        data[last_offset + 0] = 50;  // R
        data[last_offset + 1] = 100; // G
        data[last_offset + 2] = 150; // B
        data[last_offset + 3] = 255; // A
        std::cout << "Last pixel set successfully" << std::endl;
    }
    else {
        std::cout << "ERROR: Last pixel offset exceeds buffer size!" << std::endl;
    }
}

TEST(DebugTest, FillTestPattern)
{
    std::cout << "Testing fill pattern..." << std::endl;
    TestImage rgba_img(64, 64, 4);

    std::cout << "Starting pattern fill..." << std::endl;
    for (int y = 0; y < 64; ++y) {
        uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        for (int x = 0; x < 64; ++x) {
            rgba_row[x * 4 + 0] = static_cast<uint8_t>(x % 256);       // R
            rgba_row[x * 4 + 1] = static_cast<uint8_t>(y % 256);       // G
            rgba_row[x * 4 + 2] = static_cast<uint8_t>((x + y) % 256); // B
            rgba_row[x * 4 + 3] = 255;                                 // A
        }
        if (y % 16 == 0) {
            std::cout << "Filled row " << y << std::endl;
        }
    }
    std::cout << "Pattern fill completed successfully" << std::endl;
}

TEST(DebugTest, SimpleRgbaToBgrCall)
{
    std::cout << "Testing simple rgbaToBgr call..." << std::endl;
    TestImage rgba_img(4, 4, 4); // Very small image
    TestImage bgr_img(4, 4, 3);

    // Fill with simple pattern
    uint8_t* data = rgba_img.data();
    for (int i = 0; i < 4 * 4; ++i) {
        data[i * 4 + 0] = 255; // R
        data[i * 4 + 1] = 128; // G
        data[i * 4 + 2] = 64;  // B
        data[i * 4 + 3] = 255; // A
    }

    std::cout << "Calling ccap::rgbaToBgr..." << std::endl;
    std::cout << "RGBA stride: " << rgba_img.stride() << std::endl;
    std::cout << "BGR stride: " << bgr_img.stride() << std::endl;

    ccap::rgbaToBgr(rgba_img.data(), rgba_img.stride(), bgr_img.data(), bgr_img.stride(), 4, 4);

    std::cout << "rgbaToBgr call completed successfully" << std::endl;

    // Check result
    uint8_t* bgr_data = bgr_img.data();
    std::cout << "First BGR pixel: B=" << (int)bgr_data[0] << " G=" << (int)bgr_data[1] << " R=" << (int)bgr_data[2] << std::endl;
}

TEST(DebugTest, LargerRgbaToBgrCall)
{
    std::cout << "Testing larger rgbaToBgr call..." << std::endl;
    TestImage rgba_img(64, 64, 4);
    TestImage bgr_img(64, 64, 3);

    std::cout << "Filling RGBA image..." << std::endl;
    // Fill with pattern
    for (int y = 0; y < 64; ++y) {
        uint8_t* rgba_row = rgba_img.data() + y * rgba_img.stride();
        for (int x = 0; x < 64; ++x) {
            rgba_row[x * 4 + 0] = static_cast<uint8_t>(x % 256);       // R
            rgba_row[x * 4 + 1] = static_cast<uint8_t>(y % 256);       // G
            rgba_row[x * 4 + 2] = static_cast<uint8_t>((x + y) % 256); // B
            rgba_row[x * 4 + 3] = 255;                                 // A
        }
    }

    std::cout << "Calling ccap::rgbaToBgr on 64x64 image..." << std::endl;
    ccap::rgbaToBgr(rgba_img.data(), rgba_img.stride(), bgr_img.data(), bgr_img.stride(), 64, 64);

    std::cout << "Large rgbaToBgr call completed successfully" << std::endl;
}
