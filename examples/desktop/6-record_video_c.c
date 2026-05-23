/**
 * @file 6-record_video_c.c
 * @brief Example: open a camera and record frames to a video file using ccap C interface
 * @author wysaid (this@wysaid.org)
 * @date 2025-05
 *
 * Usage:
 *   ./6-record_video_c [output_path.mp4]
 *
 * Records ~5 seconds (150 frames at 30 fps) from the first available camera
 * and saves them to output_path.mp4 (default: camera_capture.mp4 next to the binary).
 */

#include "ccap_c.h"
#include "ccap_utils_c.h"
#include "utils/helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#ifndef CCAP_ENABLE_VIDEO_WRITER

int main(void) {
    fprintf(stderr, "[WARNING] Video writing is not supported on this platform.\n"
                    "Rebuild with -DCCAP_ENABLE_VIDEO_WRITER=ON (requires Windows or macOS).\n");
    return 0;
}

#else

#include "ccap_writer_c.h"

// Error callback function
void error_callback(CcapErrorCode errorCode, const char* errorDescription, void* userData) {
    (void)userData;
    printf("Error - Code: %d, Description: %s\n", (int)errorCode, errorDescription);
}

// Portable steady-clock timestamp in nanoseconds
static uint64_t steadyClockNowNs(void) {
#ifdef _WIN32
    // QueryPerformanceCounter is the Windows equivalent of CLOCK_MONOTONIC
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000000LL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000LL + (uint64_t)ts.tv_nsec;
#endif
}

int main(int argc, char** argv) {
    printf("ccap C Interface Video Recording Example\n");
    printf("Version: %s\n\n", ccap_get_version());

    ExampleCommandLine commandLine = { 0 };
    initExampleCommandLine(&commandLine, argc, argv);
    applyExampleCameraBackend(&commandLine);

    ccap_set_log_level(CCAP_LOG_LEVEL_VERBOSE);
    ccap_set_error_callback(error_callback, NULL);

    // Determine output path
    char outputPath[2048];
    if (commandLine.argc >= 2) {
        strncpy(outputPath, commandLine.argv[1], sizeof(outputPath) - 1);
        outputPath[sizeof(outputPath) - 1] = '\0';
    } else {
        char exeDir[1024];
        if (commandLine.argc > 0 && commandLine.argv[0][0] != '.') {
            strncpy(exeDir, commandLine.argv[0], sizeof(exeDir) - 1);
            exeDir[sizeof(exeDir) - 1] = '\0';
            char* lastSlash = strrchr(exeDir, '/');
            if (!lastSlash) lastSlash = strrchr(exeDir, '\\');
            if (lastSlash) *lastSlash = '\0';
        } else {
            if (getCurrentWorkingDirectory(exeDir, sizeof(exeDir)) != 0) {
                strncpy(exeDir, ".", sizeof(exeDir) - 1);
            }
        }
        snprintf(outputPath, sizeof(outputPath), "%s/camera_capture.mp4", exeDir);
    }

    printf("Output video: %s\n", outputPath);

    // Open camera
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        fprintf(stderr, "Failed to create provider\n");
        return -1;
    }

    ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, 1280);
    ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, 720);
    ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_RATE, 30.0);

    int deviceIndex = selectCamera(provider, &commandLine);

    if (!ccap_provider_open_by_index(provider, deviceIndex, true)) {
        fprintf(stderr, "Failed to open camera!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    if (!ccap_provider_is_started(provider)) {
        fprintf(stderr, "Failed to start camera!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    int realWidth = (int)ccap_provider_get_property(provider, CCAP_PROPERTY_WIDTH);
    int realHeight = (int)ccap_provider_get_property(provider, CCAP_PROPERTY_HEIGHT);
    double realFps = ccap_provider_get_property(provider, CCAP_PROPERTY_FRAME_RATE);

    printf("Camera started: %dx%d @ %.2f fps\n", realWidth, realHeight, realFps);

    // Configure and open video writer
    CcapWriterConfig writerConfig;
    memset(&writerConfig, 0, sizeof(writerConfig));
    writerConfig.codec = CCAP_VIDEO_CODEC_H264;
    writerConfig.container = CCAP_VIDEO_FORMAT_MP4;
    writerConfig.width = (uint32_t)realWidth;
    writerConfig.height = (uint32_t)realHeight;
    writerConfig.frameRate = realFps > 0.0 ? realFps : 30.0;
    writerConfig.bitRate = 0; // auto bit rate based on resolution and codec (YouTube recommended)

    CcapVideoWriter* writer = ccap_video_writer_create();
    if (!writer) {
        fprintf(stderr, "Failed to create video writer!\n");
        ccap_provider_destroy(provider);
        return -1;
    }

    if (!ccap_video_writer_open(writer, outputPath, &writerConfig)) {
        fprintf(stderr, "Failed to open video writer!\n");
        ccap_video_writer_destroy(writer);
        ccap_provider_destroy(provider);
        return -1;
    }

    // Record ~5 seconds
    const int kMaxFrames = 150;
    int recorded = 0;
    uint64_t recordStartNs = 0;
    printf("Recording %d frames (~5 seconds)...\n", kMaxFrames);

    while (recorded < kMaxFrames) {
        CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
        if (!frame) {
            fprintf(stderr, "Timeout waiting for camera frame.\n");
            break;
        }

        if (recorded == 0) {
            recordStartNs = steadyClockNowNs();
        }

        uint64_t timestampNs = steadyClockNowNs() - recordStartNs;

        CcapVideoFrameInfo frameInfo;
        if (ccap_video_frame_get_info(frame, &frameInfo)) {
            if (!ccap_video_writer_write_frame(writer, &frameInfo, timestampNs)) {
                fprintf(stderr, "Failed to write frame %d\n", recorded);
            }
        }

        ccap_video_frame_release(frame);

        if (++recorded % 30 == 0) {
            printf("  Recorded %d/%d frames...\n", recorded, kMaxFrames);
        }
    }

    ccap_video_writer_close(writer);
    ccap_video_writer_destroy(writer);

    ccap_provider_stop(provider);
    ccap_provider_close(provider);
    ccap_provider_destroy(provider);

    printf("Done! %d frames saved to: %s\n", recorded, outputPath);
    return 0;
}

#endif // CCAP_ENABLE_VIDEO_WRITER
