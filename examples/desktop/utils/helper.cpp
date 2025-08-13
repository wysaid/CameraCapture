/**
 * @file helper.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Utility functions for ccap examples.
 * @date 2025-08
 *
 */

#include "helper.h"

#include "ccap_c.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
#include <ccap.h>
#include <iostream>
#endif

int selectCamera(CcapProvider* provider) {
    char** deviceNames;
    size_t deviceCount;

    if (ccap_provider_find_device_names(provider, &deviceNames, &deviceCount) && deviceCount > 1) {
        printf("Multiple devices found, please select one:\n");
        for (size_t i = 0; i < deviceCount; i++) {
            printf("  %zu: %s\n", i, deviceNames[i]);
        }

        int selectedIndex;
        printf("Enter the index of the device you want to use: ");
        if (scanf("%d", &selectedIndex) != 1) {
            selectedIndex = 0;
        }

        if (selectedIndex < 0 || selectedIndex >= (int)deviceCount) {
            selectedIndex = 0;
            fprintf(stderr, "Invalid index, using the first device: %s\n", deviceNames[0]);
        } else {
            printf("Using device: %s\n", deviceNames[selectedIndex]);
        }

        ccap_provider_free_device_names(deviceNames, deviceCount);
        return selectedIndex;
    }

    if (deviceCount > 0) {
        ccap_provider_free_device_names(deviceNames, deviceCount);
    }

    return -1; // One or no device, use default.
}

#ifdef __cplusplus
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
#endif