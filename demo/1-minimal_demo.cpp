/**
 * @file minimal_demo.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Demo for ccap.
 * @date 2025-05
 *
 */

#include "ccap.h"

#include <iostream>

int main()
{
    ccap::Provider cameraProvider(-1); // Open the default camera
    cameraProvider.start();

    if (!cameraProvider.isStarted())
    {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    for (int i = 0; i < 10; ++i)
    {
        auto frame = cameraProvider.grab(3000);
        if (frame)
        {
            printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d, format: %s\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes, ccap::pixelFormatToString(frame->pixelFormat).data());
        }
        else
        {
            std::cerr << "Failed to grab frame!" << std::endl;
            break;
        }
    }

    std::cout << "Captured 10 frames, stopping..." << std::endl;
    return 0;
}
