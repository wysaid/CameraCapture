#include "helper.h"

#include "ccap_c.h"

#include <ccap.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" {

// Create directory if not exists
void createDirectory(const char* path) {
    try {
        std::filesystem::create_directories(path);
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to create directory %s: %s\n", path, e.what());
    }
}

// Get current working directory (portable)
int getCurrentWorkingDirectory(char* buffer, int size) {
    try {
        auto cwd = std::filesystem::current_path().string();
        if (cwd.size() >= size) {
            return -1; // Buffer too small
        }
        strcpy(buffer, cwd.c_str());
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to get current working directory: %s\n", e.what());
        return -1;
    }
}

int selectCamera(CcapProvider* provider) {
    CcapDeviceNamesList deviceList;

    if (ccap_provider_find_device_names_list(provider, &deviceList) && deviceList.deviceCount > 1) {
        printf("Multiple devices found, please select one:\n");
        for (size_t i = 0; i < deviceList.deviceCount; i++) {
            printf("  %zu: %s\n", i, deviceList.deviceNames[i]);
        }

        int selectedIndex;
        printf("Enter the index of the device you want to use: ");
        if (scanf("%d", &selectedIndex) != 1) {
            selectedIndex = 0;
        }

        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(deviceList.deviceCount)) {
            selectedIndex = 0;
            fprintf(stderr, "Invalid index, using the first device: %s\n", deviceList.deviceNames[0]);
        } else {
            printf("Using device: %s\n", deviceList.deviceNames[selectedIndex]);
        }

        return selectedIndex;
    }

    return -1; // One or no device, use default.
}

} // extern "C"

int selectCamera(ccap::Provider& provider) {
    const auto names = provider.findDeviceNames();
    if (names.size() > 1) {
        printf("Multiple devices found, please select one:\n");
        for (size_t i = 0; i < names.size(); ++i) {
            printf("  %zu: %s\n", i, names[i].c_str());
        }

        int selectedIndex;
        printf("Enter the index of the device you want to use: ");
        if (scanf("%d", &selectedIndex) != 1) {
            selectedIndex = 0;
        }

        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(names.size())) {
            selectedIndex = 0;
            fprintf(stderr, "Invalid index, using the first device: %s\n", names[0].c_str());
        } else {
            printf("Using device: %s\n", names[selectedIndex].c_str());
        }
        return selectedIndex;
    }
    return -1; // One or no device, use default.
}
