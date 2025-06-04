/**
 * @file minimal_example.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Example for ccap.
 * @date 2025-05
 *
 */

#include <ccap.h>
#include <iostream>

int selectCamera(ccap::Provider& provider) {
    if (auto names = provider.findDeviceNames(); names.size() > 1) {
        std::cout << "Multiple devices found, please select one:" << std::endl;
        for (size_t i = 0; i < names.size(); ++i) {
            std::cout << "  " << i << ": " << names[i] << std::endl;
        }
        int selectedIndex;
        std::cout << "Enter the index of the device you want to use: ";
        std::cin >> selectedIndex;
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(names.size())) {
            selectedIndex = 0;
            std::cerr << "Invalid index, using the first device:" << names[0] << std::endl;
        } else {
            std::cout << "Using device: " << names[selectedIndex] << std::endl;
        }
        return selectedIndex;
    }

    return -1; // One or no device, use default.
}

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