/**
 * @file ccap_c_example.c
 * @brief Example demonstrating the use of ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 */

#include "ccap_c.h"

#include <stdio.h>
#include <stdlib.h>

// Callback function for new frames
bool frame_callback(const CcapVideoFrame* frame, void* userData) {
    static int frameCount = 0;
    frameCount++;

    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("Frame %d: %dx%d, format=%d, timestamp=%llu\n",
               frameCount, frameInfo.width, frameInfo.height,
               frameInfo.pixelFormat, frameInfo.timestamp);
    }

    // Return false to keep the frame available for grab()
    // Return true to consume the frame (won't be available for grab())
    return false;
}

int main() {
    printf("ccap C Interface Example\n");
    printf("Version: %s\n\n", ccap_get_version());

    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Failed to create provider\n");
        return -1;
    }

    // Find available devices
    CcapDeviceNamesList deviceList;
    if (ccap_provider_find_device_names_list(provider, &deviceList)) {
        printf("Found %zu camera device(s):\n", deviceList.deviceCount);
        for (size_t i = 0; i < deviceList.deviceCount; i++) {
            printf("  %zu: %s\n", i, deviceList.deviceNames[i]);
        }
        printf("\n");
    } else {
        printf("Failed to enumerate devices\n");
    }

    // Open default camera
    if (!ccap_provider_open(provider, NULL, false)) {
        printf("Failed to open camera\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    printf("Camera opened successfully\n");

    // Get device info
    CcapDeviceInfo deviceInfo;
    if (ccap_provider_get_device_info(provider, &deviceInfo)) {
        printf("Device: %s\n", deviceInfo.deviceName ? deviceInfo.deviceName : "Unknown");
        printf("Supported pixel formats: %zu\n", deviceInfo.pixelFormatCount);
        printf("Supported resolutions: %zu\n", deviceInfo.resolutionCount);

        if (deviceInfo.resolutionCount > 0) {
            printf("First resolution: %dx%d\n",
                   deviceInfo.supportedResolutions[0].width,
                   deviceInfo.supportedResolutions[0].height);
        }
    }

    // Set camera properties
    ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, 640);
    ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, 480);
    ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_RATE, 30.0);

    // Set frame callback
    ccap_provider_set_new_frame_callback(provider, frame_callback, NULL);

    // Start capturing
    if (!ccap_provider_start(provider)) {
        printf("Failed to start camera\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    printf("Camera started, capturing frames...\n");

    // Capture frames for 5 seconds using both callback and grab methods
    for (int i = 0; i < 10; i++) {
        // Try to grab a frame (synchronous method)
        CcapVideoFrame* frame = ccap_provider_grab(provider, 5000); // 1 second timeout
        if (frame) {
            CcapVideoFrameInfo frameInfo;
            if (ccap_video_frame_get_info(frame, &frameInfo)) {
                printf("Grabbed frame: %dx%d, size=%u bytes\n",
                       frameInfo.width, frameInfo.height, frameInfo.sizeInBytes);
            }

            // Release the frame
            ccap_video_frame_release(frame);
        } else {
            printf("Failed to grab frame or timeout\n");
        }
    }

    // Stop capturing
    ccap_provider_stop(provider);
    printf("Camera stopped\n");

    // Close and cleanup
    ccap_provider_close(provider);
    ccap_provider_destroy(provider);

    printf("Example completed successfully\n");
    return 0;
}
