/**
 * @file minimal_example.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Example for ccap.
 * @date 2025-05
 *
 */

#include "utils/helper.h"

#include <ccap.h>
#include <iostream>

int main() {
    // Set error callback to receive error notifications
    ccap::setErrorCallback([](ccap::ErrorCode errorCode, std::string_view description) {
        std::cerr << "Error occurred - Code: " << static_cast<int>(errorCode)
                  << ", Description: " << description << std::endl;
    });

#if defined(_WIN32) || defined(_WIN64)
    // On Windows, you can force a backend like this:
    // ccap::Provider cameraProvider(-1, "msmf");
    // ccap::Provider cameraProvider(-1, "dshow");
#endif
    ccap::Provider cameraProvider;

    cameraProvider.open(selectCamera(cameraProvider, nullptr), true);

    cameraProvider.start();

    if (!cameraProvider.isStarted()) {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    for (int i = 0; i < 10; ++i) {
        auto frame = cameraProvider.grab(3000);
        if (frame) {
            printf("VideoFrame %d grabbed: width = %d, height = %d, bytes: %d, format: %s\n", (int)frame->frameIndex, frame->width,
                   frame->height, (int)frame->sizeInBytes, ccap::pixelFormatToString(frame->pixelFormat).data());
        } else {
            std::cerr << "Failed to grab frame!" << std::endl;
            exit(-1);
        }
    }

    std::cout << "Captured 10 frames, stopping..." << std::endl;
    return 0;
}