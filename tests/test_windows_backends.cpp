/**
 * @file test_windows_backends.cpp
 * @brief Windows backend selection and fallback tests
 * @date 2026-03-07
 */

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "test_utils.h"

#include <ccap.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const char* value) :
        m_name(name) {
        const DWORD length = GetEnvironmentVariableA(name, nullptr, 0);
        if (length > 0) {
            std::string buffer(length, '\0');
            const DWORD written = GetEnvironmentVariableA(name, buffer.data(), length);
            if (written > 0 && written < length) {
                m_hadValue = true;
                m_oldValue.assign(buffer.data(), written);
            }
        }

        SetEnvironmentVariableA(name, value);
    }

    ~ScopedEnvVar() {
        if (m_hadValue) {
            SetEnvironmentVariableA(m_name.c_str(), m_oldValue.c_str());
        } else {
            SetEnvironmentVariableA(m_name.c_str(), nullptr);
        }
    }

private:
    std::string m_name;
    std::string m_oldValue;
    bool m_hadValue = false;
};

fs::path findProjectRoot() {
    fs::path projectRoot = fs::current_path();
    while (projectRoot.has_parent_path()) {
        if (fs::exists(projectRoot / "CMakeLists.txt") && fs::exists(projectRoot / "tests")) {
            return projectRoot;
        }
        fs::path parent = projectRoot.parent_path();
        if (parent == projectRoot) {
            break;
        }
        projectRoot = std::move(parent);
    }
    return {};
}

fs::path getBuiltInTestVideoPath() {
    fs::path projectRoot = findProjectRoot();
    if (projectRoot.empty()) {
        return {};
    }
    return projectRoot / "tests" / "test-data" / "test.mp4";
}

std::vector<std::string> listDevicesForBackend(const char* backend) {
    ScopedEnvVar env("CCAP_WINDOWS_BACKEND", backend);
    ccap::Provider provider;
    return provider.findDeviceNames();
}

int virtualCameraRank(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (char ch : name) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    constexpr std::array<std::string_view, 3> keywords = {
        "obs",
        "virtual",
        "fake",
    };

    for (size_t index = 0; index < keywords.size(); ++index) {
        if (normalized.find(keywords[index]) != std::string::npos) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

std::vector<std::string> listCommonDevices() {
    auto dshowDevices = listDevicesForBackend("dshow");
    auto msmfDevices = listDevicesForBackend("msmf");
    std::vector<std::string> common;
    auto isUniqueInBoth = [&](const std::string& name) {
        return std::count(dshowDevices.begin(), dshowDevices.end(), name) == 1 &&
            std::count(msmfDevices.begin(), msmfDevices.end(), name) == 1;
    };

    for (const std::string& device : dshowDevices) {
        if (isUniqueInBoth(device) &&
            std::find(msmfDevices.begin(), msmfDevices.end(), device) != msmfDevices.end()) {
            common.push_back(device);
        }
    }

    std::stable_sort(common.begin(), common.end(), [](const std::string& lhs, const std::string& rhs) {
        return virtualCameraRank(lhs) < virtualCameraRank(rhs);
    });
    return common;
}

std::optional<ccap::DeviceInfo> getDeviceInfoForBackend(const char* backend, const std::string& deviceName) {
    ScopedEnvVar env("CCAP_WINDOWS_BACKEND", backend);
    ccap::Provider provider;
    if (!provider.open(deviceName, false)) {
        return std::nullopt;
    }

    auto info = provider.getDeviceInfo();
    provider.close();
    return info;
}

std::optional<ccap::DeviceInfo::Resolution> pickCommonResolution(const std::string& deviceName) {
    auto dshowInfo = getDeviceInfoForBackend("dshow", deviceName);
    auto msmfInfo = getDeviceInfoForBackend("msmf", deviceName);
    if (!dshowInfo || !msmfInfo) {
        return std::nullopt;
    }

    std::set<std::pair<uint32_t, uint32_t>> dshowResolutions;
    for (const auto& resolution : dshowInfo->supportedResolutions) {
        dshowResolutions.emplace(resolution.width, resolution.height);
    }

    std::vector<ccap::DeviceInfo::Resolution> common;
    for (const auto& resolution : msmfInfo->supportedResolutions) {
        if (dshowResolutions.count({ resolution.width, resolution.height }) != 0) {
            common.push_back(resolution);
        }
    }

    if (common.empty()) {
        return std::nullopt;
    }

    constexpr std::array<std::pair<uint32_t, uint32_t>, 3> preferredResolutions = {
        std::pair<uint32_t, uint32_t>{ 640, 480 },
        std::pair<uint32_t, uint32_t>{ 1280, 720 },
        std::pair<uint32_t, uint32_t>{ 1920, 1080 },
    };
    for (const auto& preferred : preferredResolutions) {
        auto match = std::find_if(common.begin(), common.end(), [&](const ccap::DeviceInfo::Resolution& resolution) {
            return resolution.width == preferred.first && resolution.height == preferred.second;
        });
        if (match != common.end()) {
            return *match;
        }
    }

    std::stable_sort(common.begin(), common.end(), [](const ccap::DeviceInfo::Resolution& lhs, const ccap::DeviceInfo::Resolution& rhs) {
        uint64_t lhsArea = static_cast<uint64_t>(lhs.width) * lhs.height;
        uint64_t rhsArea = static_cast<uint64_t>(rhs.width) * rhs.height;
        if (lhsArea != rhsArea) {
            return lhsArea < rhsArea;
        }
        if (lhs.width != rhs.width) {
            return lhs.width < rhs.width;
        }
        return lhs.height < rhs.height;
    });
    return common.front();
}

struct CapturedFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    ccap::PixelFormat pixelFormat = ccap::PixelFormat::Unknown;
    ccap::FrameOrientation orientation = ccap::FrameOrientation::Default;
    std::vector<uint8_t> bytes;
};

CapturedFrame copyFrame(const std::shared_ptr<ccap::VideoFrame>& frame) {
    CapturedFrame captured;
    captured.width = frame->width;
    captured.height = frame->height;
    captured.stride = frame->stride[0];
    captured.pixelFormat = frame->pixelFormat;
    captured.orientation = frame->orientation;
    captured.bytes.assign(frame->data[0], frame->data[0] + static_cast<size_t>(frame->stride[0]) * frame->height);
    return captured;
}

std::optional<std::vector<CapturedFrame>> captureBurstForBackend(
    const char* backend,
    const std::string& deviceName,
    const ccap::DeviceInfo::Resolution& resolution,
    int warmupFrames = 12,
    int capturedFrames = 3) {
    ScopedEnvVar env("CCAP_WINDOWS_BACKEND", backend);
    ccap::Provider provider;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    provider.set(ccap::PropertyName::Width, resolution.width);
    provider.set(ccap::PropertyName::Height, resolution.height);
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::BGR24);
    provider.set(ccap::PropertyName::FrameOrientation, ccap::FrameOrientation::TopToBottom);

    if (!provider.open(deviceName, false)) {
        return std::nullopt;
    }

    bool started = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (provider.start()) {
            started = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    if (!started) {
        provider.close();
        return std::nullopt;
    }

    for (int index = 0; index < warmupFrames; ++index) {
        auto frame = provider.grab(3000);
        if (!frame) {
            provider.close();
            return std::nullopt;
        }
    }

    std::vector<CapturedFrame> burst;
    burst.reserve(static_cast<size_t>(capturedFrames));
    for (int index = 0; index < capturedFrames; ++index) {
        auto frame = provider.grab(3000);
        if (!frame) {
            provider.close();
            return std::nullopt;
        }

        if (frame->pixelFormat != ccap::PixelFormat::BGR24 || frame->orientation != ccap::FrameOrientation::TopToBottom) {
            provider.close();
            return std::nullopt;
        }

        burst.push_back(copyFrame(frame));
    }

    provider.stop();
    provider.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return burst;
}

struct ImageDiffStats {
    double mse = 0.0;
    double psnr = 0.0;
    double meanAbsDiff = 0.0;
    double withinToleranceRatio = 0.0;
    uint8_t maxAbsDiff = 0;
    size_t totalSamples = 0;
};

ImageDiffStats calculateImageDiffStats(const CapturedFrame& lhs, const CapturedFrame& rhs, uint8_t tolerance) {
    ImageDiffStats stats;
    if (lhs.width != rhs.width || lhs.height != rhs.height || lhs.pixelFormat != rhs.pixelFormat) {
        stats.psnr = -std::numeric_limits<double>::infinity();
        return stats;
    }

    const size_t rowBytes = static_cast<size_t>(lhs.width) * 3;
    uint64_t totalAbsDiff = 0;
    uint64_t withinToleranceCount = 0;
    long double totalSquaredDiff = 0.0;

    for (uint32_t row = 0; row < lhs.height; ++row) {
        const uint8_t* lhsRow = lhs.bytes.data() + static_cast<size_t>(lhs.stride) * row;
        const uint8_t* rhsRow = rhs.bytes.data() + static_cast<size_t>(rhs.stride) * row;
        for (size_t column = 0; column < rowBytes; ++column) {
            uint8_t diff = static_cast<uint8_t>(std::abs(static_cast<int>(lhsRow[column]) - static_cast<int>(rhsRow[column])));
            totalAbsDiff += diff;
            totalSquaredDiff += static_cast<long double>(diff) * diff;
            withinToleranceCount += diff <= tolerance ? 1 : 0;
            stats.maxAbsDiff = std::max(stats.maxAbsDiff, diff);
        }
    }

    stats.totalSamples = rowBytes * lhs.height;
    stats.meanAbsDiff = static_cast<double>(totalAbsDiff) / static_cast<double>(stats.totalSamples);
    stats.withinToleranceRatio = static_cast<double>(withinToleranceCount) / static_cast<double>(stats.totalSamples);
    stats.mse = static_cast<double>(totalSquaredDiff / static_cast<long double>(stats.totalSamples));
    if (stats.mse == 0.0) {
        stats.psnr = std::numeric_limits<double>::infinity();
    } else {
        stats.psnr = 10.0 * std::log10((255.0 * 255.0) / stats.mse);
    }
    return stats;
}

std::optional<ImageDiffStats> findBestBurstMatch(const std::vector<CapturedFrame>& dshowFrames, const std::vector<CapturedFrame>& msmfFrames) {
    std::optional<ImageDiffStats> best;
    for (const auto& dshowFrame : dshowFrames) {
        for (const auto& msmfFrame : msmfFrames) {
            auto stats = calculateImageDiffStats(dshowFrame, msmfFrame, 24);
            if (!best || stats.psnr > best->psnr) {
                best = stats;
            }
        }
    }
    return best;
}

std::string formatDiffStats(const ImageDiffStats& stats) {
    std::ostringstream stream;
    stream << "PSNR=" << stats.psnr << " dB, MSE=" << stats.mse << ", mean abs diff=" << stats.meanAbsDiff
           << ", within tolerance=" << (stats.withinToleranceRatio * 100.0) << "%, max diff="
           << static_cast<int>(stats.maxAbsDiff);
    return stream.str();
}

} // namespace

TEST(WindowsBackendsTest, ForcedBackendsEnumerateWithoutCrash) {
    for (const char* backend : { "dshow", "msmf" }) {
        SCOPED_TRACE(backend);
        auto deviceNames = listDevicesForBackend(backend);
        for (const std::string& name : deviceNames) {
            EXPECT_FALSE(name.empty());
        }
    }
}

TEST(WindowsBackendsTest, AutoEnumerationContainsForcedBackendResults) {
    auto dshowDevices = listDevicesForBackend("dshow");
    auto msmfDevices = listDevicesForBackend("msmf");
    auto autoDevices = listDevicesForBackend("auto");

    for (const std::string& name : dshowDevices) {
        EXPECT_NE(std::find(autoDevices.begin(), autoDevices.end(), name), autoDevices.end()) << name;
    }

    for (const std::string& name : msmfDevices) {
        EXPECT_NE(std::find(autoDevices.begin(), autoDevices.end(), name), autoDevices.end()) << name;
    }
}

TEST(WindowsBackendsTest, FilePlaybackStillWorksWhenMSMFIsForced) {
#ifdef CCAP_ENABLE_FILE_PLAYBACK
    ScopedEnvVar env("CCAP_WINDOWS_BACKEND", "msmf");
    fs::path videoPath = getBuiltInTestVideoPath();
    if (videoPath.empty() || !fs::exists(videoPath)) {
        GTEST_SKIP() << "Built-in test video not found";
    }

    ccap::Provider provider;
    ASSERT_TRUE(provider.open(videoPath.string(), false));
    EXPECT_TRUE(provider.isOpened());
    EXPECT_TRUE(provider.isFileMode());
    provider.close();
#else
    GTEST_SKIP() << "File playback support is disabled";
#endif
}

TEST(WindowsBackendsTest, ForcedBackendCanOpenWhenDeviceExists) {
    for (const char* backend : { "dshow", "msmf" }) {
        SCOPED_TRACE(backend);
        ScopedEnvVar env("CCAP_WINDOWS_BACKEND", backend);
        ccap::Provider probe;
        auto deviceNames = probe.findDeviceNames();
        if (deviceNames.empty()) {
            continue;
        }

        ccap::Provider provider;
        ASSERT_TRUE(provider.open(deviceNames.front(), false));
        EXPECT_TRUE(provider.isOpened());
        provider.close();
    }
}

TEST(WindowsBackendsTest, SharedPhysicalCameraProducesConsistentFrameMetadata) {
    auto commonDevices = listCommonDevices();
    if (commonDevices.empty()) {
        GTEST_SKIP() << "No camera is shared between DirectShow and Media Foundation";
    }

    auto resolution = pickCommonResolution(commonDevices.front());
    if (!resolution) {
        GTEST_SKIP() << "No common resolution was reported for the shared camera";
    }

    auto dshowFrames = captureBurstForBackend("dshow", commonDevices.front(), *resolution, 8, 1);
    auto msmfFrames = captureBurstForBackend("msmf", commonDevices.front(), *resolution, 8, 1);
    ASSERT_TRUE(dshowFrames.has_value()) << "DirectShow failed to capture a frame";
    ASSERT_TRUE(msmfFrames.has_value()) << "MSMF failed to capture a frame";
    ASSERT_EQ(dshowFrames->size(), 1u);
    ASSERT_EQ(msmfFrames->size(), 1u);

    const auto& dshowFrame = dshowFrames->front();
    const auto& msmfFrame = msmfFrames->front();
    EXPECT_EQ(dshowFrame.width, msmfFrame.width);
    EXPECT_EQ(dshowFrame.height, msmfFrame.height);
    EXPECT_EQ(dshowFrame.pixelFormat, ccap::PixelFormat::BGR24);
    EXPECT_EQ(msmfFrame.pixelFormat, ccap::PixelFormat::BGR24);
    EXPECT_EQ(dshowFrame.orientation, ccap::FrameOrientation::TopToBottom);
    EXPECT_EQ(msmfFrame.orientation, ccap::FrameOrientation::TopToBottom);
}

TEST(WindowsBackendsTest, MSMFFramesMatchDirectShowWithinTolerance) {
    auto commonDevices = listCommonDevices();
    if (commonDevices.empty()) {
        GTEST_SKIP() << "No camera is shared between DirectShow and Media Foundation";
    }

    auto resolution = pickCommonResolution(commonDevices.front());
    if (!resolution) {
        GTEST_SKIP() << "No common resolution was reported for the shared camera";
    }

    auto dshowFrames = captureBurstForBackend("dshow", commonDevices.front(), *resolution);
    auto msmfFrames = captureBurstForBackend("msmf", commonDevices.front(), *resolution);
    ASSERT_TRUE(dshowFrames.has_value()) << "DirectShow failed to capture a comparison burst";
    ASSERT_TRUE(msmfFrames.has_value()) << "MSMF failed to capture a comparison burst";

    auto bestStats = findBestBurstMatch(*dshowFrames, *msmfFrames);
    ASSERT_TRUE(bestStats.has_value()) << "Failed to compare DirectShow and MSMF bursts";

    EXPECT_GE(bestStats->psnr, 20.0) << formatDiffStats(*bestStats);
    EXPECT_LE(bestStats->meanAbsDiff, 12.0) << formatDiffStats(*bestStats);
    EXPECT_GE(bestStats->withinToleranceRatio, 0.80) << formatDiffStats(*bestStats);
}

#endif