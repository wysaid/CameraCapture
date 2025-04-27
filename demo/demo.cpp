#include "CameraCapture.h"

#include <cstdio>
#include <iostream>

int main()
{
    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    auto cameraProvider = ccap::createProvider();
    if (!cameraProvider)
    {
        std::cerr << "Failed to create provider!" << std::endl;
        return -1;
    }

    cameraProvider->set(ccap::PropertyName::Width, 1280);
    cameraProvider->set(ccap::PropertyName::Height, 720);

    cameraProvider->open() && cameraProvider->start();

    if (!cameraProvider->isStarted())
    {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    while (cameraProvider->isStarted())
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

    return 0;
}