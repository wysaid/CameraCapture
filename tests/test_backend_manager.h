/**
 * @file test_backend_manager.h
 * @brief Backend management utilities for ccap unit tests
 */

#pragma once

#include "ccap_convert.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace ccap_test {

/**
 * @brief Backend test manager with explicit backend control
 */
class BackendTestManager {
public:
    /**
     * @brief Backend test fixture that ensures specific backend is used
     */
    class BackendTestFixture : public ::testing::Test {
    protected:
        void SetUp() override {
            original_backend_ = ccap::getConvertBackend();
        }

        void TearDown() override {
            ccap::setConvertBackend(original_backend_);
        }

        /**
         * @brief Set and verify specific backend
         */
        void setBackend(ccap::ConvertBackend backend) {
            bool success = ccap::setConvertBackend(backend);
            ASSERT_TRUE(success) << "Failed to set backend: " << getBackendName(backend);

            auto current = ccap::getConvertBackend();
            ASSERT_EQ(current, backend) << "Backend verification failed. Expected: "
                                        << getBackendName(backend)
                                        << ", Got: " << getBackendName(current);
        }

        ccap::ConvertBackend getCurrentBackend() const {
            return ccap::getConvertBackend();
        }

    private:
        ccap::ConvertBackend original_backend_;
    };

    /**
     * @brief Get supported backends on current platform
     */
    static std::vector<ccap::ConvertBackend> getSupportedBackends() {
        std::vector<ccap::ConvertBackend> backends;

        // CPU is always supported
        backends.push_back(ccap::ConvertBackend::CPU);

        // Check AVX2 support
        if (ccap::hasAVX2()) {
            backends.push_back(ccap::ConvertBackend::AVX2);
        }

        // Check Apple Accelerate support
        if (ccap::hasAppleAccelerate()) {
            backends.push_back(ccap::ConvertBackend::AppleAccelerate);
        }

        return backends;
    }

    /**
     * @brief Get backend name as string
     */
    static std::string getBackendName(ccap::ConvertBackend backend) {
        switch (backend) {
        case ccap::ConvertBackend::CPU:
            return "CPU";
        case ccap::ConvertBackend::AVX2:
            return "AVX2";
        case ccap::ConvertBackend::AppleAccelerate:
            return "vImage";
        case ccap::ConvertBackend::AUTO:
            return "AUTO";
        default:
            return "Unknown";
        }
    }

    /**
     * @brief Check if backend is supported
     */
    static bool isBackendSupported(ccap::ConvertBackend backend) {
        auto supported = getSupportedBackends();
        return std::find(supported.begin(), supported.end(), backend) != supported.end();
    }
};

/**
 * @brief Parameterized test class for testing all supported backends
 */
class BackendParameterizedTest : public BackendTestManager::BackendTestFixture, public ::testing::WithParamInterface<ccap::ConvertBackend> {
protected:
    void SetUp() override {
        BackendTestFixture::SetUp();
        auto backend = GetParam();

        // Skip if backend is not supported
        if (!BackendTestManager::isBackendSupported(backend)) {
            GTEST_SKIP() << "Backend " << BackendTestManager::getBackendName(backend)
                         << " is not supported on this platform";
        }

        setBackend(backend);
    }
};

} // namespace ccap_test

// Macro to create parameterized tests for all supported backends
#define INSTANTIATE_BACKEND_TEST(TestSuiteName)                                     \
    INSTANTIATE_TEST_SUITE_P(                                                       \
        AllBackends,                                                                \
        TestSuiteName,                                                              \
        ::testing::ValuesIn(ccap_test::BackendTestManager::getSupportedBackends()), \
        [](const ::testing::TestParamInfo<ccap::ConvertBackend>& info) {            \
            return ccap_test::BackendTestManager::getBackendName(info.param);       \
        })
