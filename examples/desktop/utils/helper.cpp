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
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        fprintf(stderr, "Failed to create directory %s: %s\n", path, ec.message().c_str());
    }
}

// Get current working directory (portable)
int getCurrentWorkingDirectory(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return -1;
    }

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        fprintf(stderr, "Failed to get current working directory: %s\n", ec.message().c_str());
        return -1;
    }

    std::string cwd_str = cwd.string();
    if (cwd_str.size() >= size) {
        return -1; // Buffer too small
    }
    strncpy(buffer, cwd_str.c_str(), size - 1);
    buffer[size - 1] = '\0';
    return 0;
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
