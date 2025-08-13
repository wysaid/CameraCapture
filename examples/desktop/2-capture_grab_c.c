/**
 * @file 2-capture_grab_c.c
 * @brief Example demonstrating frame capture with grab method using ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 */

#include "ccap_c.h"
#include "ccap_utils_c.h"
#include "utils/helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

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
        if (getCurrentWorkingDirectory(cwd, sizeof(cwd)) != 0) {
            strncpy(cwd, ".", sizeof(cwd) - 1);
            cwd[sizeof(cwd) - 1] = '\0';
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
                printf("VideoFrame %d grabbed: width = %d, height = %d, bytes: %d\n",
                       (int)frameInfo.frameIndex, frameInfo.width, frameInfo.height, (int)frameInfo.sizeInBytes);

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
