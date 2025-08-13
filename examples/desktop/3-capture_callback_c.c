/**
 * @file 3-capture_callback_c.c
 * @brief Example demonstrating frame capture with callback method using ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 */

#include "ccap_c.h"
#include "ccap_utils_c.h"
#include "utils/helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Platform-specific includes for directory, cwd and sleep
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <time.h>
#endif
#include <ctype.h>

// Global variables for callback context
static char g_captureDir[1024];
static int g_framesSaved = 0;

// Create directory via helper

// Callback function for new frames
bool frame_callback(const CcapVideoFrame* frame, void* userData) {
    (void)userData; // Suppress unused parameter warning

    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("VideoFrame %d grabbed: width = %d, height = %d, bytes: %d\n",
               (int)frameInfo.frameIndex, frameInfo.width, frameInfo.height, (int)frameInfo.sizeInBytes);

        // Save frame to directory
        char outputPath[2048];
        int result = ccap_dump_frame_to_directory(frame, g_captureDir, outputPath, sizeof(outputPath));
        if (result > 0) {
            printf("VideoFrame saved to: %s\n", outputPath);
            g_framesSaved++;
        } else {
            fprintf(stderr, "Failed to save frame!\n");
        }
    }

    return true; // Consume the frame (won't be available for grab())
}

void sleep_seconds(int seconds) {
#if defined(_WIN32) || defined(_WIN64)
    Sleep(seconds * 1000);
#else
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
    nanosleep(&ts, NULL);
#endif
}

int main(int argc, char** argv) {
    printf("ccap C Interface Capture Callback Example\n");
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
        if (getCurrentWorkingDirectory(cwd, sizeof(cwd)) != 0) {
            strncpy(cwd, ".", sizeof(cwd) - 1);
            cwd[sizeof(cwd) - 1] = '\0';
        }
    }

    snprintf(g_captureDir, sizeof(g_captureDir), "%s/image_capture", cwd);
    createDirectory(g_captureDir);

    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Failed to create provider\n");
        return -1;
    }

    // Find and print available devices
    CcapDeviceNamesList deviceList;
    if (ccap_provider_find_device_names_list(provider, &deviceList)) {
        for (size_t i = 0; i < deviceList.deviceCount; i++) {
            printf("## Found video capture device: %s\n", deviceList.deviceNames[i]);
        }
    }

    // Set camera properties
    int requestedWidth = 1920;
    int requestedHeight = 1080;
    double requestedFps = 60.0;

    ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, requestedWidth);
    ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, requestedHeight);
    ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, CCAP_PIXEL_FORMAT_BGR24);
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

    // Set frame callback
    if (!ccap_provider_set_new_frame_callback(provider, frame_callback, NULL)) {
        fprintf(stderr, "Failed to set frame callback!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    // Wait for 5 seconds to capture frames
    printf("Capturing frames for 5 seconds...\n");
    sleep_seconds(5);

    printf("Captured for 5 seconds, stopping... (Total frames saved: %d)\n", g_framesSaved);

    // Cleanup
    ccap_provider_destroy(provider);

    return 0;
}
