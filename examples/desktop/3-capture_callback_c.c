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

// Context structure for callback data
typedef struct {
    char captureDir[2048];
    int framesSaved;
} CallbackContext;

// Create directory via helper

// Callback function for new frames
bool frame_callback(const CcapVideoFrame* frame, void* userData) {
    CallbackContext* context = (CallbackContext*)userData;
    if (!context) {
        fprintf(stderr, "Invalid callback context!\n");
        return true;
    }

    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("VideoFrame %d grabbed: width = %d, height = %d, bytes: %d\n",
               (int)frameInfo.frameIndex, frameInfo.width, frameInfo.height, (int)frameInfo.sizeInBytes);

        // Save frame to directory
        char outputPath[2048];
        int result = ccap_dump_frame_to_directory(frame, context->captureDir, outputPath, sizeof(outputPath));
        if (result >= 0) {
            printf("VideoFrame saved to: %s\n", outputPath);
            context->framesSaved++;
        } else {
            fprintf(stderr, "Failed to save frame!\n");
        }
    }

    return true; // Consume the frame (won't be available for grab())
}

// Error callback function
void error_callback(CcapErrorCode errorCode, const char* errorDescription, void* userData) {
    (void)userData; // Unused parameter
    printf("Camera Error - Code: %d, Description: %s\n", (int)errorCode, errorDescription);
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

    // Initialize callback context
    CallbackContext callbackContext = { 0 };
    snprintf(callbackContext.captureDir, sizeof(callbackContext.captureDir), "%s/image_capture", cwd);
    createDirectory(callbackContext.captureDir);

    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Failed to create provider\n");
        return -1;
    }

    // Set error callback to receive error notifications
    ccap_set_error_callback(error_callback, NULL);

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
    if (!ccap_provider_set_new_frame_callback(provider, frame_callback, &callbackContext)) {
        fprintf(stderr, "Failed to set frame callback!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    // Wait for 5 seconds to capture frames
    printf("Capturing frames for 5 seconds...\n");
    sleep_seconds(5);

    printf("Captured for 5 seconds, stopping... (Total frames saved: %d)\n", callbackContext.framesSaved);

    // Cleanup
    ccap_provider_destroy(provider);

    return 0;
}
