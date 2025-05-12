/**
 * @file minimal_demo.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Demo for ccap.
 * @date 2025-05
 *
 */

#include "CameraCapture.h"

#include <cstdio>
#include <string>
#include <vector>

std::vector<std::string> findCameraNames()
{
    ccap::Provider cameraProvider; /// 不需要 Open Camera.
    std::vector<std::string> deviceNames = cameraProvider.findDeviceNames();

    if (!deviceNames.empty())
    {
        printf("## Found %zu video capture device: \n", deviceNames.size());
        int deviceIndex = 0;
        for (const auto& name : deviceNames)
        {
            printf("    %d: %s\n", deviceIndex++, name.c_str());
        }
    }
    else
    {
        fputs("Failed to find any video capture device.", stderr);
    }

    return deviceNames;
}

void printCameraInfo(const std::string& deviceName)
{
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    ccap::Provider cameraProvider(deviceName); /// 需要 Open Camera
    if (!cameraProvider.isOpened())
    {
        fprintf(stderr, "### Failed to open video capture device: %s\n", deviceName.c_str());
        return;
    }

    auto deviceInfo = cameraProvider.getDeviceInfo();
    if (!deviceInfo)
    {
        fputs("Failed to get device info.", stderr);
        return;
    }

    printf("===== Info for device: %s =======\n", deviceName.c_str());

    printf("  Supported resolutions:\n");
    for (const auto& resolution : deviceInfo->supportedResolutions)
    {
        printf("    %dx%d\n", resolution.width, resolution.height);
    }

    printf("  Supported pixel formats:\n");
    for (auto pixelFormat : deviceInfo->supportedPixelFormats)
    {
        printf("    %s\n", ccap::pixelFormatToString(pixelFormat).data());
    }

    puts("===== Info end =======\n");
}

int main()
{
    auto deviceNames = findCameraNames();
    if (deviceNames.empty())
    {
        return 1;
    }
    for (const auto& name : deviceNames)
    {
        printCameraInfo(name);
    }

    return 0;
}
