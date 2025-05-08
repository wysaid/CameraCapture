/**
 * @file minimal_demo.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Demo for ccap.
 * @date 2025-05
 *
 */

#include "CameraCapture.h"

#include <cstdio>

int main(int argc, char** argv)
{
    auto cameraProvider = ccap::createProvider();
    if (auto deviceNames = cameraProvider->findDeviceNames(); !deviceNames.empty())
    {
        printf("## Found %lu video capture device: \n", deviceNames.size());
        int deviceIndex = 0;
        for (const auto& name : deviceNames)
        {
            printf("    %d: %s\n", deviceIndex++, name.c_str());
            return 0;
        }
    }

    fputs("Failed to find any video capture device.", stderr);
    return -1;
}
