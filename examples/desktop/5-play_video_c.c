/**
 * @file 5-play_video_c.c
 * @brief Example demonstrating video file playback using ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 *
 * This example demonstrates how to use ccap C interface to play video files.
 * The same API works for both camera capture and video file playback.
 */

#include "ccap_c.h"
#include "ccap_utils_c.h"
#include "utils/helper.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Error callback function
void error_callback(CcapErrorCode errorCode, const char* errorDescription, void* userData) {
    (void)userData; // Unused parameter
    printf("Error - Code: %d, Description: %s\n", (int)errorCode, errorDescription);
}

int main(int argc, char** argv) {
    printf("ccap C Interface Video Playback Example\n");
    printf("Version: %s\n\n", ccap_get_version());

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video_file_path>\n", argv[0]);
        fprintf(stderr, "Example: %s /path/to/video.mp4\n", argv[0]);
        return -1;
    }

    const char* videoPath = argv[1];

    // Enable verbose log to see debug information
    ccap_set_log_level(CCAP_LOG_LEVEL_VERBOSE);

    // Set error callback to receive error notifications
    ccap_set_error_callback(error_callback, NULL);

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
    snprintf(captureDir, sizeof(captureDir), "%s/video_frames", cwd);
    createDirectory(captureDir);

    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("Failed to create provider\n");
        return -1;
    }

    // Set output format (works for both camera and video file)
    ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, CCAP_PIXEL_FORMAT_RGB24);

    // Open the video file - the same open() method works for both camera and file
    printf("Opening video file: %s\n", videoPath);
    if (!ccap_provider_open(provider, videoPath, true)) {
        fprintf(stderr, "Failed to open video file!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    // Check if we are in file mode
    if (ccap_provider_is_file_mode(provider)) {
        printf("Provider is in FILE mode\n");

        // Get video properties (only available in file mode)
        double duration = ccap_provider_get_property(provider, CCAP_PROPERTY_DURATION);
        double frameCount = ccap_provider_get_property(provider, CCAP_PROPERTY_FRAME_COUNT);
        double frameRate = ccap_provider_get_property(provider, CCAP_PROPERTY_FRAME_RATE);
        int width = (int)ccap_provider_get_property(provider, CCAP_PROPERTY_WIDTH);
        int height = (int)ccap_provider_get_property(provider, CCAP_PROPERTY_HEIGHT);

        printf("Video properties:\n");
        printf("  Duration: %.2f seconds\n", duration);
        printf("  Frame count: %.0f\n", frameCount);
        printf("  Frame rate: %.2f fps\n", frameRate);
        printf("  Resolution: %dx%d\n", width, height);

        // Set playback speed (only works in file mode)
        ccap_provider_set_property(provider, CCAP_PROPERTY_PLAYBACK_SPEED, 1.0);
    } else {
        printf("Provider is in CAMERA mode (this shouldn't happen with a file path)\n");
    }

    if (!ccap_provider_is_started(provider)) {
        fprintf(stderr, "Failed to start playback!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    printf("Playback started. Capturing first 30 frames...\n");

    // Grab frames from the video file
    int maxFrames = 30;
    int frameCount = 0;
    
    while (frameCount < maxFrames) {
        CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
        if (frame) {
            CcapVideoFrameInfo frameInfo;
            if (ccap_video_frame_get_info(frame, &frameInfo)) {
                double currentTime = ccap_provider_get_property(provider, CCAP_PROPERTY_CURRENT_TIME);
                printf("Frame %d: width=%d, height=%d, bytes=%d, time=%.2fs\n",
                       (int)frameInfo.frameIndex, frameInfo.width, frameInfo.height,
                       (int)frameInfo.sizeInBytes, currentTime);

                // Save every 10th frame
                if (frameInfo.frameIndex % 10 == 0) {
                    char outputPath[2048];
                    int result = ccap_dump_frame_to_directory(frame, captureDir, outputPath, sizeof(outputPath));
                    if (result >= 0) {
                        printf("  Frame saved to: %s\n", outputPath);
                    }
                }
            }

            ccap_video_frame_release(frame);
            frameCount++;
        } else {
            printf("No more frames or timeout\n");
            break;
        }
    }

    printf("Captured %d frames, stopping...\n", frameCount);

    // Demonstrate seeking (only works in file mode)
    if (ccap_provider_is_file_mode(provider)) {
        printf("\nDemonstrating seek functionality...\n");

        // Seek to middle of video
        double duration = ccap_provider_get_property(provider, CCAP_PROPERTY_DURATION);
        double seekTime = duration / 2.0;

        printf("Seeking to %.2f seconds...\n", seekTime);
        if (ccap_provider_set_property(provider, CCAP_PROPERTY_CURRENT_TIME, seekTime)) {
            double currentTime = ccap_provider_get_property(provider, CCAP_PROPERTY_CURRENT_TIME);
            printf("Seek successful. Current time: %.2f seconds\n", currentTime);
        }
    }

    // Cleanup
    ccap_provider_stop(provider);
    ccap_provider_close(provider);
    ccap_provider_destroy(provider);

    printf("Video playback completed.\n");
    return 0;
}
