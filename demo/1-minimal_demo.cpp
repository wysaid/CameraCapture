/**
* @file minimal_demo.cpp
* @author wysaid (this@wysaid.org)
* @brief Demo for ccap.
* @date 2025-05
*
*/

#include "CameraCapture.h"

#include <iostream>

int main(int argc, char** argv)
{
    auto cameraProvider = ccap::createProvider();
    cameraProvider->open();
    cameraProvider->start();

    if (!cameraProvider->isStarted())
    {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    for (int i = 0; i < 10; ++i)
    {
        auto frame = cameraProvider->grab(true);
        if (frame)
        {
            printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);
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
