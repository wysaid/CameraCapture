/**
 * @file test_performance.cpp
 * @brief Performance benchmarks for ccap_convert functions
 */

#include "ccap_convert.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>

using namespace ccap_test;

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Performance test setup
    }

    void benchmarkFunction(const std::string& name, std::function<void()> func, int iterations = 10)
    {
        // Warm up
        func();

        PerformanceTimer timer;
        timer.start();

        for (int i = 0; i < iterations; ++i) {
            func();
        }

        double total_ms = timer.stopAndGetMs();
        double avg_ms = total_ms / iterations;

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[BENCHMARK] " << name << ": " << avg_ms << "ms (avg of " << iterations << " runs)" << std::endl;
    }
};

// ============ Color Conversion Performance Tests ============

TEST_F(PerformanceTest, ColorShufflePerformance)
{
    const int width = 1920;
    const int height = 1080;
    const int iterations = 5;

    TestImage rgba_src(width, height, 4);
    TestImage rgb_dst(width, height, 3);
    TestImage bgr_dst(width, height, 3);
    TestImage rgba_dst(width, height, 4);

    rgba_src.fillGradient();

    // Test RGBA to RGB
    benchmarkFunction(
        "RGBA->RGB (1920x1080)",
        [&]() { ccap::rgbaToRgb(rgba_src.data(), rgba_src.stride(), rgb_dst.data(), rgb_dst.stride(), width, height); }, iterations);

    // Test RGBA to BGR
    benchmarkFunction(
        "RGBA->BGR (1920x1080)",
        [&]() { ccap::rgbaToBgr(rgba_src.data(), rgba_src.stride(), bgr_dst.data(), bgr_dst.stride(), width, height); }, iterations);

    // Test RGB to RGBA
    benchmarkFunction(
        "RGB->RGBA (1920x1080)",
        [&]() { ccap::rgbToRgba(rgb_dst.data(), rgb_dst.stride(), rgba_dst.data(), rgba_dst.stride(), width, height); }, iterations);

    // Test 4-channel shuffle
    benchmarkFunction(
        "RGBA Shuffle (1920x1080)",
        [&]() { ccap::rgbaToBgra(rgba_src.data(), rgba_src.stride(), rgba_dst.data(), rgba_dst.stride(), width, height); }, iterations);
}

// ============ YUV to RGB Performance Tests ============

TEST_F(PerformanceTest, YUVConversionPerformance)
{
    const int width = 1920;
    const int height = 1080;
    const int iterations = 3;

    TestYUVImage nv12_img(width, height, true);
    TestYUVImage i420_img(width, height, false);
    TestImage rgb_dst(width, height, 3);
    TestImage bgr_dst(width, height, 3);
    TestImage rgba_dst(width, height, 4);
    TestImage bgra_dst(width, height, 4);

    nv12_img.generateGradient();
    i420_img.generateGradient();

    // NV12 conversions
    benchmarkFunction(
        "NV12->RGB24 BT601 (1920x1080)",
        [&]() {
            ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                              rgb_dst.stride(), width, height, ccap::ConvertFlag::Default);
        },
        iterations);

    benchmarkFunction(
        "NV12->BGR24 BT601 (1920x1080)",
        [&]() {
            ccap::nv12ToBgr24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), bgr_dst.data(),
                              bgr_dst.stride(), width, height, ccap::ConvertFlag::Default);
        },
        iterations);

    benchmarkFunction(
        "NV12->RGBA32 BT709 (1920x1080)",
        [&]() {
            ccap::nv12ToRgba32(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgba_dst.data(),
                               rgba_dst.stride(), width, height, ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange);
        },
        iterations);

    benchmarkFunction(
        "NV12->BGRA32 FullRange (1920x1080)",
        [&]() {
            ccap::nv12ToBgra32(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), bgra_dst.data(),
                               bgra_dst.stride(), width, height, ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange);
        },
        iterations);

    // I420 conversions
    benchmarkFunction(
        "I420->RGB24 BT601 (1920x1080)",
        [&]() {
            ccap::i420ToRgb24(i420_img.y_data(), i420_img.y_stride(), i420_img.u_data(), i420_img.uv_stride(), i420_img.v_data(),
                              i420_img.uv_stride(), rgb_dst.data(), rgb_dst.stride(), width, height, ccap::ConvertFlag::Default);
        },
        iterations);

    benchmarkFunction(
        "I420->BGR24 BT709 (1920x1080)",
        [&]() {
            ccap::i420ToBgr24(i420_img.y_data(), i420_img.y_stride(), i420_img.u_data(), i420_img.uv_stride(), i420_img.v_data(),
                              i420_img.uv_stride(), bgr_dst.data(), bgr_dst.stride(), width, height,
                              ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange);
        },
        iterations);

    benchmarkFunction(
        "I420->RGBA32 FullRange (1920x1080)",
        [&]() {
            ccap::i420ToRgba32(i420_img.y_data(), i420_img.y_stride(), i420_img.u_data(), i420_img.uv_stride(), i420_img.v_data(),
                               i420_img.uv_stride(), rgba_dst.data(), rgba_dst.stride(), width, height,
                               ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange);
        },
        iterations);
}

// ============ Scaling Performance Tests ============

TEST_F(PerformanceTest, ScalingPerformance)
{
    struct TestSize {
        int width, height;
        std::string name;
    };

    std::vector<TestSize> sizes = { { 640, 480, "VGA" }, { 1280, 720, "HD" }, { 1920, 1080, "FHD" }, { 3840, 2160, "4K" } };

    for (const auto& size : sizes) {
        TestYUVImage nv12_img(size.width, size.height, true);
        TestImage rgb_dst(size.width, size.height, 3);

        nv12_img.generateGradient();

        std::string benchmark_name =
            "NV12->RGB24 " + size.name + " (" + std::to_string(size.width) + "x" + std::to_string(size.height) + ")";

        benchmarkFunction(
            benchmark_name,
            [&]() {
                ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                                  rgb_dst.stride(), size.width, size.height);
            },
            size.width >= 1920 ? 2 : 5); // Fewer iterations for larger images
    }
}

// ============ Memory Bandwidth Tests ============

TEST_F(PerformanceTest, MemoryBandwidth)
{
    const int width = 1920;
    const int height = 1080;
    const int iterations = 10;

    TestImage src(width, height, 4);
    TestImage dst(width, height, 4);

    src.fillRandom();

    // Simple memory copy as baseline
    benchmarkFunction("Memory Copy (1920x1080x4)", [&]() { std::memcpy(dst.data(), src.data(), src.stride() * height); }, iterations);
}

// ============ AVX2 Detection Test ============

TEST_F(PerformanceTest, AVX2Detection)
{
    bool has_avx2 = ccap::hasAVX2();

    std::cout << "[INFO] AVX2 support: " << (has_avx2 ? "YES" : "NO") << std::endl;

    if (has_avx2) {
        std::cout << "[INFO] SIMD acceleration should be enabled" << std::endl;
    }
    else {
        std::cout << "[INFO] Falling back to scalar implementation" << std::endl;
    }

    // This is always a pass, just for information
    EXPECT_TRUE(true);
}

// ============ Throughput Tests ============

TEST_F(PerformanceTest, ThroughputMeasurement)
{
    const int width = 1920;
    const int height = 1080;
    const int iterations = 10;

    TestYUVImage nv12_img(width, height, true);
    TestImage rgb_dst(width, height, 3);

    nv12_img.generateGradient();

    PerformanceTimer timer;
    timer.start();

    for (int i = 0; i < iterations; ++i) {
        ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                          rgb_dst.stride(), width, height);
    }

    double total_ms = timer.stopAndGetMs();
    double avg_ms = total_ms / iterations;

    // Calculate throughput
    size_t input_bytes = width * height * 3 / 2; // YUV420 format
    size_t output_bytes = width * height * 3;    // RGB format
    size_t total_bytes = input_bytes + output_bytes;

    double throughput_gbps = (total_bytes * 1000.0) / (avg_ms * 1024 * 1024 * 1024);
    double fps = 1000.0 / avg_ms;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[THROUGHPUT] Processing: " << avg_ms << "ms per frame" << std::endl;
    std::cout << "[THROUGHPUT] FPS: " << fps << " frames/second" << std::endl;
    std::cout << "[THROUGHPUT] Bandwidth: " << throughput_gbps << " GB/s" << std::endl;

    // Reasonable performance expectations
    EXPECT_LT(avg_ms, 50.0) << "Conversion should be under 50ms for 1080p";
    EXPECT_GT(fps, 20.0) << "Should achieve at least 20 FPS for 1080p";
}
