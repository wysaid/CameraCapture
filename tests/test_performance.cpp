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
    void SetUp() override {
        // Performance test setup
    }

    void benchmarkFunction(const std::string& name, std::function<void()> func, int iterations = 10) {
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

    // New method for AVX2 performance comparison
    void benchmarkAVX2Comparison(const std::string& name, std::function<void()> func, int iterations = 10) {
        bool original_avx2_state = ccap::hasAVX2();
        bool avx2_supported = original_avx2_state;

        // Check if we can test both implementations by temporarily disabling AVX2
        ccap::disableAVX2(true);
        bool can_disable = !ccap::hasAVX2();
        ccap::disableAVX2(!original_avx2_state); // Restore original state

        if (!avx2_supported) {
            std::cout << "[INFO] " << name << ": AVX2 not supported, testing CPU implementation only" << std::endl;
            benchmarkFunction(name + " (CPU)", func, iterations);
            return;
        }

        if (!can_disable) {
            std::cout << "[INFO] " << name << ": Cannot disable AVX2, testing AVX2 implementation only" << std::endl;
            benchmarkFunction(name + " (AVX2)", func, iterations);
            return;
        }

        std::cout << "[INFO] " << name << ": Testing both AVX2 and CPU implementations" << std::endl;

        double avx2_time = 0.0, cpu_time = 0.0;

        // Test with AVX2 enabled
        ccap::disableAVX2(false);
        {
            // Warm up
            func();

            PerformanceTimer timer;
            timer.start();

            for (int i = 0; i < iterations; ++i) {
                func();
            }

            avx2_time = timer.stopAndGetMs() / iterations;
        }

        // Test with AVX2 disabled
        ccap::disableAVX2(true);
        {
            // Warm up
            func();

            PerformanceTimer timer;
            timer.start();

            for (int i = 0; i < iterations; ++i) {
                func();
            }

            cpu_time = timer.stopAndGetMs() / iterations;
        }

        // Restore original state
        ccap::disableAVX2(!original_avx2_state);

        // Report results
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[BENCHMARK] " << name << " (AVX2): " << avx2_time << "ms (avg of " << iterations << " runs)" << std::endl;
        std::cout << "[BENCHMARK] " << name << " (CPU):  " << cpu_time << "ms (avg of " << iterations << " runs)" << std::endl;

        if (cpu_time > 0) {
            double speedup = cpu_time / avx2_time;
            std::cout << "[SPEEDUP]   " << name << ": " << speedup << "x faster with AVX2" << std::endl;
        }
    }
};

// ============ Color Conversion Performance Tests ============

TEST_F(PerformanceTest, ColorShufflePerformance) {
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

TEST_F(PerformanceTest, YUVConversionPerformance) {
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

TEST_F(PerformanceTest, ScalingPerformance) {
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

TEST_F(PerformanceTest, MemoryBandwidth) {
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

TEST_F(PerformanceTest, AVX2Detection) {
    bool has_avx2 = ccap::hasAVX2();

    std::cout << "[INFO] AVX2 support: " << (has_avx2 ? "YES" : "NO") << std::endl;

    if (has_avx2) {
        std::cout << "[INFO] SIMD acceleration should be enabled" << std::endl;
    } else {
        std::cout << "[INFO] Falling back to scalar implementation" << std::endl;
    }

    // This is always a pass, just for information
    EXPECT_TRUE(true);
}

// ============ AVX2 vs CPU Performance Comparison Tests ============

TEST_F(PerformanceTest, AVX2_vs_CPU_ColorShuffle) {
    const int width = 1920;
    const int height = 1080;
    const int iterations = 5;

    TestImage rgba_src(width, height, 4);
    TestImage rgb_dst(width, height, 3);
    TestImage bgr_dst(width, height, 3);
    TestImage rgba_dst(width, height, 4);

    rgba_src.fillGradient();

    std::cout << "\n=== AVX2 vs CPU Color Shuffle Performance ===" << std::endl;

    // Test RGBA to RGB
    benchmarkAVX2Comparison(
        "RGBA->RGB (1920x1080)",
        [&]() { ccap::rgbaToRgb(rgba_src.data(), rgba_src.stride(), rgb_dst.data(), rgb_dst.stride(), width, height); }, iterations);

    // Test RGBA to BGR
    benchmarkAVX2Comparison(
        "RGBA->BGR (1920x1080)",
        [&]() { ccap::rgbaToBgr(rgba_src.data(), rgba_src.stride(), bgr_dst.data(), bgr_dst.stride(), width, height); }, iterations);

    // Test 4-channel shuffle
    benchmarkAVX2Comparison(
        "RGBA->BGRA (1920x1080)",
        [&]() { ccap::rgbaToBgra(rgba_src.data(), rgba_src.stride(), rgba_dst.data(), rgba_dst.stride(), width, height); }, iterations);

    std::cout << "=============================================" << std::endl;
}

TEST_F(PerformanceTest, AVX2_vs_CPU_YUVConversion) {
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

    std::cout << "\n=== AVX2 vs CPU YUV Conversion Performance ===" << std::endl;

    // NV12 conversions
    benchmarkAVX2Comparison(
        "NV12->RGB24 BT601 (1920x1080)",
        [&]() {
            ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                              rgb_dst.stride(), width, height, ccap::ConvertFlag::Default);
        },
        iterations);

    benchmarkAVX2Comparison(
        "NV12->BGR24 BT601 (1920x1080)",
        [&]() {
            ccap::nv12ToBgr24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), bgr_dst.data(),
                              bgr_dst.stride(), width, height, ccap::ConvertFlag::Default);
        },
        iterations);

    benchmarkAVX2Comparison(
        "NV12->RGBA32 BT709 (1920x1080)",
        [&]() {
            ccap::nv12ToRgba32(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgba_dst.data(),
                               rgba_dst.stride(), width, height, ccap::ConvertFlag::BT709 | ccap::ConvertFlag::VideoRange);
        },
        iterations);

    benchmarkAVX2Comparison(
        "NV12->BGRA32 FullRange (1920x1080)",
        [&]() {
            ccap::nv12ToBgra32(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), bgra_dst.data(),
                               bgra_dst.stride(), width, height, ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange);
        },
        iterations);

    // I420 conversions
    benchmarkAVX2Comparison(
        "I420->RGB24 BT601 (1920x1080)",
        [&]() {
            ccap::i420ToRgb24(i420_img.y_data(), i420_img.y_stride(), i420_img.u_data(), i420_img.uv_stride(), i420_img.v_data(),
                              i420_img.uv_stride(), rgb_dst.data(), rgb_dst.stride(), width, height, ccap::ConvertFlag::Default);
        },
        iterations);

    benchmarkAVX2Comparison(
        "I420->RGBA32 FullRange (1920x1080)",
        [&]() {
            ccap::i420ToRgba32(i420_img.y_data(), i420_img.y_stride(), i420_img.u_data(), i420_img.uv_stride(), i420_img.v_data(),
                               i420_img.uv_stride(), rgba_dst.data(), rgba_dst.stride(), width, height,
                               ccap::ConvertFlag::BT601 | ccap::ConvertFlag::FullRange);
        },
        iterations);

    std::cout << "===============================================" << std::endl;
}

TEST_F(PerformanceTest, AVX2_vs_CPU_ScalingComparison) {
    struct TestSize {
        int width, height;
        std::string name;
        int iterations;
    };

    std::vector<TestSize> sizes = { { 640, 480, "VGA", 8 }, { 1280, 720, "HD", 5 }, { 1920, 1080, "FHD", 3 }, { 3840, 2160, "4K", 2 } };

    std::cout << "\n=== AVX2 vs CPU Scaling Performance ===" << std::endl;

    for (const auto& size : sizes) {
        TestYUVImage nv12_img(size.width, size.height, true);
        TestImage rgb_dst(size.width, size.height, 3);

        nv12_img.generateGradient();

        std::string benchmark_name =
            "NV12->RGB24 " + size.name + " (" + std::to_string(size.width) + "x" + std::to_string(size.height) + ")";

        benchmarkAVX2Comparison(
            benchmark_name,
            [&]() {
                ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                                  rgb_dst.stride(), size.width, size.height);
            },
            size.iterations);
    }

    std::cout << "========================================" << std::endl;
}

TEST_F(PerformanceTest, AVX2_vs_CPU_DetailedAnalysis) {
    const int width = 1920;
    const int height = 1080;
    const int iterations = 10;

    TestYUVImage nv12_img(width, height, true);
    TestImage rgb_dst(width, height, 3);
    nv12_img.generateGradient();

    bool original_avx2_state = ccap::hasAVX2();
    bool avx2_supported = original_avx2_state;

    std::cout << "\n=== Detailed AVX2 Performance Analysis ===" << std::endl;
    std::cout << "[INFO] Platform AVX2 Support: " << (avx2_supported ? "YES" : "NO") << std::endl;

    if (!avx2_supported) {
        std::cout << "[INFO] AVX2 not supported, testing CPU implementation only" << std::endl;
        benchmarkFunction(
            "NV12->RGB24 (CPU)",
            [&]() {
                ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                                  rgb_dst.stride(), width, height);
            },
            iterations);
        return;
    }

    // Test AVX2 disable/enable functionality
    ccap::disableAVX2(true);
    bool can_disable = !ccap::hasAVX2();
    ccap::disableAVX2(false);
    bool can_enable = ccap::hasAVX2();
    ccap::disableAVX2(!original_avx2_state); // Restore

    std::cout << "[INFO] AVX2 Control: " << (can_disable && can_enable ? "WORKING" : "LIMITED") << std::endl;

    if (!(can_disable && can_enable)) {
        std::cout << "[WARN] Cannot properly control AVX2 state, testing current implementation only" << std::endl;
        benchmarkFunction(
            "NV12->RGB24 (Current)",
            [&]() {
                ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                                  rgb_dst.stride(), width, height);
            },
            iterations);
        return;
    }

    // Detailed performance measurement
    double times_avx2[iterations], times_cpu[iterations];

    // Measure AVX2 performance
    ccap::disableAVX2(false);
    for (int i = 0; i < iterations; ++i) {
        PerformanceTimer timer;
        timer.start();
        ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                          rgb_dst.stride(), width, height);
        times_avx2[i] = timer.stopAndGetMs();
    }

    // Measure CPU performance
    ccap::disableAVX2(true);
    for (int i = 0; i < iterations; ++i) {
        PerformanceTimer timer;
        timer.start();
        ccap::nv12ToRgb24(nv12_img.y_data(), nv12_img.y_stride(), nv12_img.uv_data(), nv12_img.uv_stride(), rgb_dst.data(),
                          rgb_dst.stride(), width, height);
        times_cpu[i] = timer.stopAndGetMs();
    }

    // Restore original state
    ccap::disableAVX2(!original_avx2_state);

    // Calculate statistics
    double avg_avx2 = 0, avg_cpu = 0;
    double min_avx2 = times_avx2[0], max_avx2 = times_avx2[0];
    double min_cpu = times_cpu[0], max_cpu = times_cpu[0];

    for (int i = 0; i < iterations; ++i) {
        avg_avx2 += times_avx2[i];
        avg_cpu += times_cpu[i];
        min_avx2 = std::min(min_avx2, times_avx2[i]);
        max_avx2 = std::max(max_avx2, times_avx2[i]);
        min_cpu = std::min(min_cpu, times_cpu[i]);
        max_cpu = std::max(max_cpu, times_cpu[i]);
    }
    avg_avx2 /= iterations;
    avg_cpu /= iterations;

    // Calculate throughput
    size_t input_bytes = width * height * 3 / 2; // YUV420 format
    size_t output_bytes = width * height * 3;    // RGB format
    size_t total_bytes = input_bytes + output_bytes;

    double throughput_avx2_gbps = (total_bytes * 1000.0) / (avg_avx2 * 1024 * 1024 * 1024);
    double throughput_cpu_gbps = (total_bytes * 1000.0) / (avg_cpu * 1024 * 1024 * 1024);
    double fps_avx2 = 1000.0 / avg_avx2;
    double fps_cpu = 1000.0 / avg_cpu;
    double speedup = avg_cpu / avg_avx2;

    // Report detailed results
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n[DETAILED RESULTS] NV12->RGB24 (1920x1080)" << std::endl;
    std::cout << "AVX2 Implementation:" << std::endl;
    std::cout << "  Average: " << avg_avx2 << "ms" << std::endl;
    std::cout << "  Min/Max: " << min_avx2 << "ms / " << max_avx2 << "ms" << std::endl;
    std::cout << "  FPS: " << fps_avx2 << " frames/second" << std::endl;
    std::cout << "  Bandwidth: " << throughput_avx2_gbps << " GB/s" << std::endl;

    std::cout << "CPU Implementation:" << std::endl;
    std::cout << "  Average: " << avg_cpu << "ms" << std::endl;
    std::cout << "  Min/Max: " << min_cpu << "ms / " << max_cpu << "ms" << std::endl;
    std::cout << "  FPS: " << fps_cpu << " frames/second" << std::endl;
    std::cout << "  Bandwidth: " << throughput_cpu_gbps << " GB/s" << std::endl;

    std::cout << "Performance Comparison:" << std::endl;
    std::cout << "  Speedup: " << speedup << "x faster with AVX2" << std::endl;
    std::cout << "  Efficiency: "
              << (speedup >= 2.0     ? "EXCELLENT" :
                      speedup >= 1.5 ? "GOOD" :
                      speedup >= 1.2 ? "MODERATE" :
                                       "LIMITED")
              << std::endl;

    std::cout << "==========================================" << std::endl;

    // Performance expectations
    EXPECT_GT(speedup, 1.0) << "AVX2 should be faster than CPU implementation";
    EXPECT_LT(avg_avx2, 30.0) << "AVX2 conversion should be under 30ms for 1080p";
    EXPECT_GT(fps_avx2, 30.0) << "AVX2 should achieve at least 30 FPS for 1080p";
}
