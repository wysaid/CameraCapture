/**
 * @file 1-minimal_example_c.c
 * @brief Minimal example demonstrating the use of ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 */

#include "ccap_c.h"
#include "ccap_utils_c.h"
#include "utils/helper.h"

#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("ccap C Interface Minimal Example\n");
    printf("Version: %s\n\n", ccap_get_version());

    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Failed to create provider\n");
        return -1;
    }

    // Select and open camera
    int deviceIndex = selectCamera(provider);
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

    printf("Camera started successfully\n");

    // Capture 10 frames
    for (int i = 0; i < 10; i++) {
        CcapVideoFrame* frame = ccap_provider_grab(provider, 3000); // 3 second timeout
        if (frame) {
            CcapVideoFrameInfo frameInfo;
            if (ccap_video_frame_get_info(frame, &frameInfo)) {
                // Get pixel format string
                char formatStr[64];
                ccap_pixel_format_to_string(frameInfo.pixelFormat, formatStr, sizeof(formatStr));

                printf("VideoFrame %llu grabbed: width = %d, height = %d, bytes: %d, format: %s\n",
                       frameInfo.frameIndex, frameInfo.width, frameInfo.height,
                       frameInfo.sizeInBytes, formatStr);
            }

            ccap_video_frame_release(frame);
        } else {
            fprintf(stderr, "Failed to grab frame!\n");
            ccap_provider_destroy(provider);
            return -1;
        }
    }

    printf("Captured 10 frames, stopping...\n");

    // Cleanup
    ccap_provider_destroy(provider);

    return 0;
}
