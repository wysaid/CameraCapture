/**
 * @file test_performance.cpp
 * @brief CCAP vs LibYUV Performance Comparison Tests
 *
 * This file contains comprehensive performance comparison tests between CCAP backends and LibYUV:
 * - CPU (always available)
 * - AVX2 (Intel/AMD x86_64 with AVX2 support) 
 * - Apple Accelerate/vImage (macOS/iOS)
 * - LibYUV (third-party reference implementation)
 *
 * All performance tests follow the multi-backend + LibYUV comparison framework.
 * Performance tests should be compiled in Release mode for accurate benchmarking.
 */

#include "ccap_convert.h"
#include "libyuv.h"
#include "test_backend_manager.h"
#include "test_utils.h"

#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

using namespace ccap_test;

// ============ CCAP vs LibYUV Performance Comparison Framework ============

/**
 * @brief Test resolution configuration
 */
struct Resolution {
    int width;
    int height;
    std::string name;
    
    Resolution(int w, int h, const std::string& n) : width(w), height(h), name(n) {}
};

/**
 * @brief RGB color conversion configuration
 */
struct RGBConversion {
    std::string name;
    int src_channels;
    int dst_channels;
    bool libyuv_available;
    std::function<void(const uint8_t*, int, uint8_t*, int, int, int)> ccap_func;
    std::function<int(const uint8_t*, int, uint8_t*, int, int, int)> libyuv_func;
    
    RGBConversion(const std::string& n, int src_ch, int dst_ch,
                  std::function<void(const uint8_t*, int, uint8_t*, int, int, int)> ccap_f,
                  std::function<int(const uint8_t*, int, uint8_t*, int, int, int)> libyuv_f,
                  bool libyuv_avail = true)
        : name(n), src_channels(src_ch), dst_channels(dst_ch), libyuv_available(libyuv_avail), 
          ccap_func(ccap_f), libyuv_func(libyuv_f) {}
};

/**
 * @brief YUV to RGB conversion configuration
 */
struct YUVConversion {
    std::string name;
    bool is_nv12; // true for NV12, false for I420
    int dst_channels;
    std::function<void(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int)> ccap_func;
    std::function<int(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int)> libyuv_func;
    
    YUVConversion(const std::string& n, bool nv12, int dst_ch,
                  std::function<void(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int)> ccap_f,
                  std::function<int(const uint8_t*, int, const uint8_t*, int, const uint8_t*, int, uint8_t*, int, int, int)> libyuv_f)
        : name(n), is_nv12(nv12), dst_channels(dst_ch), ccap_func(ccap_f), libyuv_func(libyuv_f) {}
};

/**
 * @brief Performance measurement result
 */
struct PerformanceResult {
    std::string backend_name;
    double time_ms;
    double fps;
    double bandwidth_gbps;
    bool success;

    PerformanceResult(const std::string& name, double ms, int width, int height, int channels, bool ok = true) :
        backend_name(name), time_ms(ms), success(ok) {
        if (ms > 0 && success) {
            fps = 1000.0 / ms;
            double bytes_per_frame = width * height * channels;
            double bytes_per_second = bytes_per_frame * fps;
            bandwidth_gbps = bytes_per_second / (1024.0 * 1024.0 * 1024.0);
        } else {
            fps = 0.0;
            bandwidth_gbps = 0.0;
        }
    }
};

/**
 * @brief Performance comparison utility for CCAP vs LibYUV analysis
 */
class PerformanceBenchmark {
public:
    static std::string getSpeedupClassification(double speedup) {
        if (speedup >= 8.0) return "EXCELLENT";
        if (speedup >= 4.0) return "VERY GOOD";
        if (speedup >= 2.0) return "GOOD";
        if (speedup >= 1.5) return "MODERATE";
        return "MINIMAL";
    }

    static void printCCAPvsLibYUVComparison(const std::vector<PerformanceResult>& results,
                                             const std::string& test_name, int width, int height) {
        std::cout << "\n=== CCAP vs LibYUV: " << test_name << " (" << width << "x" << height << ") ===" << std::endl;

        PerformanceResult* libyuv_result = nullptr;
        PerformanceResult* best_ccap_result = nullptr;
        double best_ccap_time = std::numeric_limits<double>::max();

        // Print individual results
        for (auto& result : results) {
            if (!result.success) {
                std::cout << "[SKIPPED] " << result.backend_name << ": Not supported" << std::endl;
                continue;
            }

            std::cout << "[BENCHMARK] " << result.backend_name << ": "
                      << std::fixed << std::setprecision(3) << result.time_ms << "ms"
                      << " (~" << std::setprecision(0) << result.fps << " FPS)"
                      << " [" << std::setprecision(2) << result.bandwidth_gbps << " GB/s]" << std::endl;

            if (result.backend_name == "LibYUV") {
                libyuv_result = const_cast<PerformanceResult*>(&result);
            } else if (result.backend_name.find("CCAP-") == 0) {
                if (result.time_ms < best_ccap_time) {
                    best_ccap_time = result.time_ms;
                    best_ccap_result = const_cast<PerformanceResult*>(&result);
                }
            }
        }

        // Print CCAP vs LibYUV comparison
        if (libyuv_result && best_ccap_result) {
            std::cout << "\n=== CCAP vs LibYUV Analysis ===" << std::endl;

            for (const auto& result : results) {
                if (!result.success || result.backend_name == "LibYUV") continue;

                double speedup = libyuv_result->time_ms / result.time_ms;
                std::string performance_class = getSpeedupClassification(speedup);

                if (speedup > 1.0) {
                    std::cout << "[WINNER] " << result.backend_name << " is "
                              << std::fixed << std::setprecision(1) << speedup << "x faster than LibYUV ("
                              << performance_class << ")" << std::endl;
                } else {
                    std::cout << "[SLOWER] " << result.backend_name << " is "
                              << std::fixed << std::setprecision(1) << (1.0 / speedup) << "x slower than LibYUV" << std::endl;
                }
            }

            std::cout << "[BEST CCAP] " << best_ccap_result->backend_name << ": "
                      << std::fixed << std::setprecision(3) << best_ccap_result->time_ms << "ms" << std::endl;

            double overall_speedup = libyuv_result->time_ms / best_ccap_result->time_ms;
            if (overall_speedup > 1.0) {
                std::cout << "[WINNER] CCAP (" << best_ccap_result->backend_name << ") is "
                          << std::fixed << std::setprecision(1) << overall_speedup << "x faster than LibYUV" << std::endl;
            } else {
                std::cout << "[SLOWER] Best CCAP backend is "
                          << std::fixed << std::setprecision(1) << (1.0 / overall_speedup) << "x slower than LibYUV" << std::endl;
            }
        }
        std::cout << std::endl;
    }
};

                /**
 * @brief CCAP vs LibYUV Performance Comparison Test Suite
 * 
 * This test suite compares CCAP backend performance against LibYUV reference implementation
 * across various conversion types and resolutions.
 */
class CCAPvsLibYUVComparisonTest : public ::testing::Test {
protected:
    // ============ Common Test Configurations ============
    
    /**
     * @brief Standard test resolutions
     */
    static const std::vector<Resolution> getStandardResolutions() {
        return {
            Resolution(640, 480, "VGA"),
            Resolution(1024, 1024, "1K_Square"),
            Resolution(1920, 1080, "1080p"),
            Resolution(3840, 2160, "4K"),
            Resolution(7680, 4320, "8K")  // For large image tests
        };
    }
    
    /**
     * @brief RGB color shuffle conversions
     */
    static const std::vector<RGBConversion> getRGBConversions() {
        return {
            // 3-channel conversions
            RGBConversion("RGB to BGR", 3, 3,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::rgbToBgr(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    // LibYUV doesn't have direct RGB to BGR conversion
                    return -1; // Not implemented
                },
                false // LibYUV not available for this conversion
            ),
            RGBConversion("BGR to RGB", 3, 3,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::bgrToRgb(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    // LibYUV doesn't have direct BGR to RGB conversion
                    return -1; // Not implemented
                },
                false // LibYUV not available for this conversion
            ),
            
            // 4-channel conversions
            RGBConversion("RGBA to BGRA", 4, 4,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::rgbaToBgra(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::ARGBToBGRA(src, src_stride, dst, dst_stride, w, h);
                }
            ),
            RGBConversion("BGRA to RGBA", 4, 4,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::bgraToRgba(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::BGRAToARGB(src, src_stride, dst, dst_stride, w, h);
                }
            ),
            
            // Cross-channel conversions
            RGBConversion("RGBA to BGR", 4, 3,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::rgbaToBgr(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::ARGBToRGB24(src, src_stride, dst, dst_stride, w, h);
                }
            ),
            RGBConversion("RGBA to RGB", 4, 3,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::rgbaToRgb(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::ARGBToRGB24(src, src_stride, dst, dst_stride, w, h);
                }
            ),
            RGBConversion("RGB to RGBA", 3, 4,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::rgbToRgba(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::RGB24ToARGB(src, src_stride, dst, dst_stride, w, h);
                }
            ),
            RGBConversion("BGR to BGRA", 3, 4,
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::bgrToBgra(src, src_stride, dst, dst_stride, w, h);
                },
                [](const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    // LibYUV doesn't have direct BGR to BGRA, use RGB24ToARGB as approximation
                    return libyuv::RGB24ToARGB(src, src_stride, dst, dst_stride, w, h);
                }
            )
        };
    }
    
    /**
     * @brief YUV to RGB conversions
     */
    static const std::vector<YUVConversion> getYUVConversions() {
        return {
            // I420 conversions
            YUVConversion("I420 to RGB", false, 3,
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::i420ToRgb24(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::I420ToRGB24(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h);
                }
            ),
            YUVConversion("I420 to BGR", false, 3,
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::i420ToBgr24(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) -> int {
                    // LibYUV doesn't have I420ToBGR24, use I420ToRGB24 as approximation
                    return libyuv::I420ToRGB24(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h);
                }
            ),
            YUVConversion("I420 to RGBA", false, 4,
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::i420ToRgba32(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::I420ToARGB(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h);
                }
            ),
            YUVConversion("I420 to BGRA", false, 4,
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::i420ToBgra32(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* u, int u_stride, const uint8_t* v, int v_stride,
                   uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::I420ToBGRA(y, y_stride, u, u_stride, v, v_stride, dst, dst_stride, w, h);
                }
            ),
            
            // NV12 conversions
            YUVConversion("NV12 to RGB", true, 3,
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::nv12ToRgb24(y, y_stride, uv, uv_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::NV12ToRGB24(y, y_stride, uv, uv_stride, dst, dst_stride, w, h);
                }
            ),
            YUVConversion("NV12 to BGR", true, 3,
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::nv12ToBgr24(y, y_stride, uv, uv_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    // LibYUV doesn't have NV12ToBGR24, use NV12ToRGB24 as approximation
                    return libyuv::NV12ToRGB24(y, y_stride, uv, uv_stride, dst, dst_stride, w, h);
                }
            ),
            YUVConversion("NV12 to RGBA", true, 4,
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::nv12ToRgba32(y, y_stride, uv, uv_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    return libyuv::NV12ToARGB(y, y_stride, uv, uv_stride, dst, dst_stride, w, h);
                }
            ),
            YUVConversion("NV12 to BGRA", true, 4,
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) {
                    ccap::nv12ToBgra32(y, y_stride, uv, uv_stride, dst, dst_stride, w, h, ccap::ConvertFlag::Default);
                },
                [](const uint8_t* y, int y_stride, const uint8_t* uv, int uv_stride, const uint8_t* unused,
                   int unused_stride, uint8_t* dst, int dst_stride, int w, int h) -> int {
                    // LibYUV doesn't have NV12ToBGRA, use NV12ToARGB as approximation
                    return libyuv::NV12ToARGB(y, y_stride, uv, uv_stride, dst, dst_stride, w, h);
                }
            )
        };
    }
protected:
    void SetUp() override {
#ifdef NDEBUG
        std::cout << "[INFO] CCAP vs LibYUV comparison tests running in RELEASE mode" << std::endl;
#else
        std::cout << "[WARNING] CCAP vs LibYUV comparison tests running in DEBUG mode - results may not be accurate!" << std::endl;
#endif
    }

    /**
     * @brief Generic benchmark function template for CCAP vs LibYUV comparison
     */
    template<typename CCAPFunc, typename LibYUVFunc>
    void benchmarkComparison(const std::string& test_name, int width, int height, int channels,
                           CCAPFunc ccap_func, LibYUVFunc libyuv_func) {
        std::vector<PerformanceResult> results;
        auto supported_backends = BackendTestManager::getSupportedBackends();

        // Test all CCAP backends
        for (auto backend : supported_backends) {
            ccap::setConvertBackend(backend);
            std::string backend_name = "CCAP-" + BackendTestManager::getBackendName(backend);

            // Warm up
            for (int i = 0; i < 3; ++i) {
                try {
                    ccap_func();
                } catch (...) {
                    break;
                }
            }

            // Measure CCAP performance
            auto start_time = std::chrono::high_resolution_clock::now();
            bool success = true;

            try {
                ccap_func();
            } catch (...) {
                success = false;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end_time - start_time);

            results.emplace_back(backend_name, duration.count(), width, height, channels, success);
        }

        // Test LibYUV
        // Warm up LibYUV
        for (int i = 0; i < 3; ++i) {
            try {
                libyuv_func();
            } catch (...) {
                break;
            }
        }

        // Measure LibYUV performance
        auto start_time = std::chrono::high_resolution_clock::now();
        bool libyuv_success = true;

        try {
            int result = libyuv_func();
            libyuv_success = (result == 0);
        } catch (...) {
            libyuv_success = false;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end_time - start_time);

        results.emplace_back("LibYUV", duration.count(), width, height, channels, libyuv_success);

        // Print comparison results
        PerformanceBenchmark::printCCAPvsLibYUVComparison(results, test_name, width, height);
    }
    
    // ============ Parameterized Benchmark Methods ============
    
    /**
     * @brief Parameterized RGB conversion benchmark
     */
    void benchmarkRGBConversion(const Resolution& resolution, const RGBConversion& conversion) {
        if (!conversion.libyuv_available) {
            // LibYUV not available for this conversion, only test CCAP backends
            benchmarkCCAPOnly(resolution, conversion);
            return;
        }

        TestImage src_img(resolution.width, resolution.height, conversion.src_channels);
        TestImage ccap_dst_img(resolution.width, resolution.height, conversion.dst_channels);
        TestImage libyuv_dst_img(resolution.width, resolution.height, conversion.dst_channels);
        src_img.fillRandom(42);

        auto ccap_func = [&]() {
            conversion.ccap_func(src_img.data(), src_img.stride(),
                                ccap_dst_img.data(), ccap_dst_img.stride(),
                                resolution.width, resolution.height);
        };

        auto libyuv_func = [&]() -> int {
            return conversion.libyuv_func(src_img.data(), src_img.stride(),
                                         libyuv_dst_img.data(), libyuv_dst_img.stride(),
                                         resolution.width, resolution.height);
        };

        std::string test_name = conversion.name + " " + resolution.name;
        benchmarkComparison(test_name, resolution.width, resolution.height, 
                          conversion.dst_channels, ccap_func, libyuv_func);
    }

    /**
     * @brief CCAP-only benchmark for conversions not supported by LibYUV
     */
    void benchmarkCCAPOnly(const Resolution& resolution, const RGBConversion& conversion) {
        std::vector<PerformanceResult> results;
        auto supported_backends = BackendTestManager::getSupportedBackends();
        
        TestImage src_img(resolution.width, resolution.height, conversion.src_channels);
        TestImage ccap_dst_img(resolution.width, resolution.height, conversion.dst_channels);
        src_img.fillRandom(42);

        // Test all CCAP backends
        for (auto backend : supported_backends) {
            ccap::setConvertBackend(backend);
            std::string backend_name = "CCAP-" + BackendTestManager::getBackendName(backend);

            auto ccap_func = [&]() {
                conversion.ccap_func(src_img.data(), src_img.stride(),
                                    ccap_dst_img.data(), ccap_dst_img.stride(),
                                    resolution.width, resolution.height);
            };

            // Warm up
            for (int i = 0; i < 3; ++i) {
                try {
                    ccap_func();
                } catch (...) {
                    break;
                }
            }

            // Measure CCAP performance
            auto start_time = std::chrono::high_resolution_clock::now();
            bool success = true;

            try {
                ccap_func();
            } catch (...) {
                success = false;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end_time - start_time);

            results.emplace_back(backend_name, duration.count(), resolution.width, resolution.height, conversion.dst_channels, success);
        }

        // Print CCAP-only results
        std::string test_name = conversion.name + " " + resolution.name;
        printCCAPOnlyResults(results, test_name, resolution.width, resolution.height);
    }

    /**
     * @brief Print CCAP-only benchmark results
     */
    void printCCAPOnlyResults(const std::vector<PerformanceResult>& results,
                              const std::string& test_name, int width, int height) {
        std::cout << "\n=== CCAP Only: " << test_name << " (" << width << "x" << height << ") ===" << std::endl;
        std::cout << "[INFO] LibYUV does not support this conversion - CCAP backends only" << std::endl;

        PerformanceResult* best_ccap_result = nullptr;
        double best_ccap_time = std::numeric_limits<double>::max();

        // Print individual results
        for (const auto& result : results) {
            if (!result.success) {
                std::cout << "[SKIPPED] " << result.backend_name << ": Not supported" << std::endl;
                continue;
            }

            std::cout << "[BENCHMARK] " << result.backend_name << ": "
                      << std::fixed << std::setprecision(3) << result.time_ms << "ms"
                      << " (~" << std::setprecision(0) << result.fps << " FPS)"
                      << " [" << std::setprecision(2) << result.bandwidth_gbps << " GB/s]" << std::endl;

            if (result.time_ms < best_ccap_time) {
                best_ccap_time = result.time_ms;
                best_ccap_result = const_cast<PerformanceResult*>(&result);
            }
        }

        // Print best CCAP result
        if (best_ccap_result) {
            std::cout << "[BEST CCAP] " << best_ccap_result->backend_name << ": "
                      << std::fixed << std::setprecision(3) << best_ccap_result->time_ms << "ms" << std::endl;
        }
        std::cout << std::endl;
    }
    
    /**
     * @brief Parameterized YUV conversion benchmark
     */
    void benchmarkYUVConversion(const Resolution& resolution, const YUVConversion& conversion) {
        TestYUVImage yuv_img(resolution.width, resolution.height, conversion.is_nv12);
        TestImage ccap_dst_img(resolution.width, resolution.height, conversion.dst_channels);
        TestImage libyuv_dst_img(resolution.width, resolution.height, conversion.dst_channels);
        yuv_img.generateGradient();

        auto ccap_func = [&]() {
            if (conversion.is_nv12) {
                // NV12: UV interleaved, pass UV as U channel, nullptr as V
                conversion.ccap_func(yuv_img.y_data(), yuv_img.y_stride(),
                                   yuv_img.uv_data(), yuv_img.uv_stride(),
                                   nullptr, 0,
                                   ccap_dst_img.data(), ccap_dst_img.stride(),
                                   resolution.width, resolution.height);
            } else {
                // I420: separate U and V channels
                conversion.ccap_func(yuv_img.y_data(), yuv_img.y_stride(),
                                   yuv_img.u_data(), yuv_img.u_stride(),
                                   yuv_img.v_data(), yuv_img.v_stride(),
                                   ccap_dst_img.data(), ccap_dst_img.stride(),
                                   resolution.width, resolution.height);
            }
        };

        auto libyuv_func = [&]() -> int {
            if (conversion.is_nv12) {
                // NV12: UV interleaved
                return conversion.libyuv_func(yuv_img.y_data(), yuv_img.y_stride(),
                                            yuv_img.uv_data(), yuv_img.uv_stride(),
                                            nullptr, 0,
                                            libyuv_dst_img.data(), libyuv_dst_img.stride(),
                                            resolution.width, resolution.height);
            } else {
                // I420: separate U and V channels
                return conversion.libyuv_func(yuv_img.y_data(), yuv_img.y_stride(),
                                            yuv_img.u_data(), yuv_img.u_stride(),
                                            yuv_img.v_data(), yuv_img.v_stride(),
                                            libyuv_dst_img.data(), libyuv_dst_img.stride(),
                                            resolution.width, resolution.height);
            }
        };

        std::string test_name = conversion.name + " " + resolution.name;
        benchmarkComparison(test_name, resolution.width, resolution.height, 
                          conversion.dst_channels, ccap_func, libyuv_func);
    }
    
    // ============ Legacy Benchmark Methods (kept for specialized tests) ============

    /**
     * @brief Benchmark memory allocation performance with conversion
     */
    void benchmarkMemoryAllocationPerformance(int width, int height) {
        TestYUVImage yuv_img(width, height, false); // I420
        yuv_img.generateGradient();

        auto ccap_func = [&]() {
            // Allocate new memory for each conversion to test memory performance
            TestImage ccap_rgba_img(width, height, 4);
            ccap::i420ToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                               yuv_img.u_data(), yuv_img.u_stride(),
                               yuv_img.v_data(), yuv_img.v_stride(),
                               ccap_rgba_img.data(), ccap_rgba_img.stride(),
                               width, height, ccap::ConvertFlag::Default);
        };

        auto libyuv_func = [&]() -> int {
            // Allocate new memory for each conversion to test memory performance
            TestImage libyuv_argb_img(width, height, 4);
            return libyuv::I420ToARGB(yuv_img.y_data(), yuv_img.y_stride(),
                                      yuv_img.u_data(), yuv_img.u_stride(),
                                      yuv_img.v_data(), yuv_img.v_stride(),
                                      libyuv_argb_img.data(), libyuv_argb_img.stride(),
                                      width, height);
        };

        benchmarkComparison("Memory Allocation + I420 to RGBA/ARGB", width, height, 4, ccap_func, libyuv_func);
    }

    /**
     * @brief Benchmark throughput with multiple frames conversion
     */
    void benchmarkThroughputMultipleFrames(int width, int height, int num_frames = 10) {
        TestYUVImage yuv_img(width, height, false); // I420
        TestImage ccap_rgba_img(width, height, 4);
        TestImage libyuv_argb_img(width, height, 4);
        yuv_img.generateGradient();

        auto ccap_func = [&]() {
            for (int i = 0; i < num_frames; ++i) {
                ccap::i420ToRgba32(yuv_img.y_data(), yuv_img.y_stride(),
                                   yuv_img.u_data(), yuv_img.u_stride(),
                                   yuv_img.v_data(), yuv_img.v_stride(),
                                   ccap_rgba_img.data(), ccap_rgba_img.stride(),
                                   width, height, ccap::ConvertFlag::Default);
            }
        };

        auto libyuv_func = [&]() -> int {
            for (int i = 0; i < num_frames; ++i) {
                int result = libyuv::I420ToARGB(yuv_img.y_data(), yuv_img.y_stride(),
                                                yuv_img.u_data(), yuv_img.u_stride(),
                                                yuv_img.v_data(), yuv_img.v_stride(),
                                                libyuv_argb_img.data(), libyuv_argb_img.stride(),
                                                width, height);
                if (result != 0) return result;
            }
            return 0;
        };

        benchmarkComparison("Throughput " + std::to_string(num_frames) + " Frames", width, height, 4, ccap_func, libyuv_func);
    }
};

// ============ CCAP vs LibYUV Performance Comparison Tests ============

// Parameterized RGB Conversion Tests
TEST_F(CCAPvsLibYUVComparisonTest, RGB_Conversions_Comprehensive) {
    auto resolutions = getStandardResolutions();
    auto rgb_conversions = getRGBConversions();
    
    // Test key resolution + conversion combinations for RGB conversions
    std::vector<std::pair<int, int>> key_resolution_indices = {
        {1, 0}, {1, 1}, {1, 2}, {1, 3}, // 1080p with first 4 RGB conversions
        {2, 0}, {2, 1}, {2, 2}, {2, 3}, // 4K with first 4 RGB conversions  
        {0, 4}, {0, 5}, {0, 6}, {0, 7}  // VGA with cross-channel conversions
    };
    
    for (const auto& [res_idx, conv_idx] : key_resolution_indices) {
        if (res_idx < resolutions.size() && conv_idx < rgb_conversions.size()) {
            const auto& resolution = resolutions[res_idx];
            const auto& conversion = rgb_conversions[conv_idx];
            benchmarkRGBConversion(resolution, conversion);
        }
    }
}

// Parameterized YUV Conversion Tests
TEST_F(CCAPvsLibYUVComparisonTest, YUV_Conversions_Comprehensive) {
    auto resolutions = getStandardResolutions();
    auto yuv_conversions = getYUVConversions();
    
    // Test key resolution + conversion combinations for YUV conversions
    std::vector<std::pair<int, int>> key_resolution_indices = {
        {1, 0}, {1, 1}, {1, 2}, {1, 3}, // 1080p with I420 conversions
        {2, 4}, {2, 5}, {2, 6}, {2, 7}, // 4K with NV12 conversions
        {0, 0}, {0, 4}                  // VGA with I420->RGB and NV12->RGB
    };
    
    for (const auto& [res_idx, conv_idx] : key_resolution_indices) {
        if (res_idx < resolutions.size() && conv_idx < yuv_conversions.size()) {
            const auto& resolution = resolutions[res_idx];
            const auto& conversion = yuv_conversions[conv_idx];
            benchmarkYUVConversion(resolution, conversion);
        }
    }
}

// Specialized Performance Tests
TEST_F(CCAPvsLibYUVComparisonTest, Large_8K_RGBA_To_BGR_Performance) {
    Resolution large_resolution(7680, 4320, "8K");
    auto rgb_conversions = getRGBConversions();
    
    // Find RGBA to BGR conversion (should be first in the list)
    for (const auto& conversion : rgb_conversions) {
        if (conversion.name.find("RGBA to BGR") != std::string::npos) {
            benchmarkRGBConversion(large_resolution, conversion);
            break;
        }
    }
}

TEST_F(CCAPvsLibYUVComparisonTest, Memory_Allocation_Performance_Multi_Resolution) {
    std::vector<Resolution> test_resolutions = {
        Resolution(640, 480, "VGA"),
        Resolution(1920, 1080, "1080p")
    };
    
    for (const auto& resolution : test_resolutions) {
        benchmarkMemoryAllocationPerformance(resolution.width, resolution.height);
    }
}

TEST_F(CCAPvsLibYUVComparisonTest, Throughput_Multiple_Frames_Multi_Resolution) {
    struct ThroughputTest {
        Resolution resolution;
        int frame_count;
    };
    
    std::vector<ThroughputTest> throughput_tests = {
        { Resolution(640, 480, "VGA"), 64 },
        { Resolution(1280, 720, "720p"), 32 },
        { Resolution(1920, 1080, "1080p"), 16 },
        { Resolution(2560, 1440, "1440p"), 8 },
        { Resolution(3840, 2160, "4K"), 4 },
        { Resolution(7680, 4320, "8K"), 2 }
    };
    
    for (const auto& test : throughput_tests) {
        benchmarkThroughputMultipleFrames(test.resolution.width, test.resolution.height, test.frame_count);
    }
}
