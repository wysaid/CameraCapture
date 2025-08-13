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
    ccap::Provider cameraProvider;
    cameraProvider.open(selectCamera(cameraProvider), true);

    cameraProvider.start();

    if (!cameraProvider.isStarted()) {
        std::cerr << "Failed to start camera!" << std::endl;
        return -1;
    }

    for (int i = 0; i < 10; ++i) {
        auto frame = cameraProvider.grab(3000);
        if (frame) {
            printf("VideoFrame %lld grabbed: width = %d, height = %d, bytes: %d, format: %s\n", frame->frameIndex, frame->width,
                   frame->height, frame->sizeInBytes, ccap::pixelFormatToString(frame->pixelFormat).data());
        } else {
            std::cerr << "Failed to grab frame!" << std::endl;
            exit(-1);
        }
    }

    std::cout << "Captured 10 frames, stopping..." << std::endl;
    return 0;
}