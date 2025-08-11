/**
 * @file 2-capture_grab_c.c
 * @brief Example demonstrating frame capture with grab method using ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 */

#include "ccap_c.h"
#include "ccap_utils_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Platform-specific includes for directory and cwd utilities
#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <ctype.h>

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

void createDirectory(const char* path) {
#if defined(_WIN32) || defined(_WIN64)
    // On Windows, _mkdir returns 0 on success, -1 on failure (e.g., already exists).
    // It's safe to call without pre-check; ignore EEXIST.
    _mkdir(path);
#else
    struct stat st = { 0 };
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
#endif
}

int main(int argc, char** argv) {
    printf("ccap C Interface Capture Grab Example\n");
    printf("Version: %s\n\n", ccap_get_version());

    // Enable verbose log to see debug information
    ccap_set_log_level(CCAP_LOG_LEVEL_VERBOSE);

    // Get current working directory and create capture directory
    char cwd[1024];
    if (argc > 0 && argv[0][0] != '.') {
        strncpy(cwd, argv[0], sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = '\0';

        // Find last slash
        char* lastSlash = strrchr(cwd, '/');
        if (!lastSlash) {
            lastSlash = strrchr(cwd, '\\');
        }
        if (lastSlash) {
            *lastSlash = '\0';
        }
    } else {
        if (!getcwd(cwd, sizeof(cwd))) {
            strcpy(cwd, ".");
        }
    }

    char captureDir[1024];
    snprintf(captureDir, sizeof(captureDir), "%s/image_capture", cwd);
    createDirectory(captureDir);

    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Failed to create provider\n");
        return -1;
    }

    // Set camera properties
    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60.0;

    ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, requestedWidth);
    ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, requestedHeight);
    ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, CCAP_PIXEL_FORMAT_RGB24);
    ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_RATE, requestedFps);

    // Select and open camera
    int deviceIndex;
    if (argc > 1 && isdigit(argv[1][0])) {
        deviceIndex = atoi(argv[1]);
    } else {
        deviceIndex = selectCamera(provider);
    }

    if (!ccap_provider_open_by_index(provider, deviceIndex, true)) {
        printf("Failed to open camera\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    if (!ccap_provider_is_started(provider)) {
        fprintf(stderr, "Failed to start camera!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    // Get actual camera properties
    int realWidth = (int)ccap_provider_get_property(provider, CCAP_PROPERTY_WIDTH);
    int realHeight = (int)ccap_provider_get_property(provider, CCAP_PROPERTY_HEIGHT);
    double realFps = ccap_provider_get_property(provider, CCAP_PROPERTY_FRAME_RATE);

    printf("Camera started successfully, requested resolution: %dx%d, real resolution: %dx%d, requested fps %.1f, real fps: %.1f\n",
           requestedWidth, requestedHeight, realWidth, realHeight, requestedFps, realFps);

    // Capture frames with 3000ms timeout
    int frameCount = 0;
    while (frameCount < 10) {
        CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
        if (frame) {
            CcapVideoFrameInfo frameInfo;
            if (ccap_video_frame_get_info(frame, &frameInfo)) {
                printf("VideoFrame %llu grabbed: width = %d, height = %d, bytes: %d\n",
                       frameInfo.frameIndex, frameInfo.width, frameInfo.height, frameInfo.sizeInBytes);

                // Save frame to directory
                char outputPath[2048];
                int result = ccap_dump_frame_to_directory(frame, captureDir, outputPath, sizeof(outputPath));
                if (result > 0) {
                    printf("VideoFrame saved to: %s\n", outputPath);
                } else {
                    fprintf(stderr, "Failed to save frame!\n");
                }
            }

            ccap_video_frame_release(frame);
            frameCount++;

            if (frameCount >= 10) {
                printf("Captured 10 frames, stopping...\n");
                break;
            }
        } else {
            fprintf(stderr, "Failed to grab frame or timeout!\n");
            break;
        }
    }

    // Cleanup
    ccap_provider_destroy(provider);

    return 0;
}
