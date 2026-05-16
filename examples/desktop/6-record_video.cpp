/**
 * @file 6-record_video.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Example: open a camera and record frames to a video file.
 * @date 2025-05
 *
 * Usage:
 *   ./6-record_video [output_path.mp4]
 *
 * Records ~5 seconds (150 frames at 30 fps) from the first available camera
 * and saves them to output_path.mp4 (default: camera_capture.mp4 next to the binary).
 */

#include "utils/helper.h"

#include <ccap.h>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef CCAP_ENABLE_VIDEO_WRITER

int main() {
    std::cerr << "[WARNING] Video writing is not supported on this platform.\n"
              << "Rebuild with -DCCAP_ENABLE_VIDEO_WRITER=ON (requires Windows or macOS).\n";
    return 0;
}

#else

#include <ccap_writer.h>
#include <chrono>

int main(int argc, char** argv) {
    ExampleCommandLine commandLine{};
    initExampleCommandLine(&commandLine, argc, argv);
    applyExampleCameraBackend(&commandLine);

    ccap::setLogLevel(ccap::LogLevel::Verbose);

    ccap::setErrorCallback([](ccap::ErrorCode errorCode, std::string_view description) {
        std::cerr << "Error - Code: " << static_cast<int>(errorCode)
                  << ", Description: " << description << "\n";
    });

    // Determine output path
    std::string outputPath;
    if (commandLine.argc >= 2) {
        outputPath = commandLine.argv[1];
    } else {
        std::string exeDir = commandLine.argv[0];
        if (auto pos = exeDir.find_last_of("/\\"); pos != std::string::npos && !exeDir.empty() && exeDir[0] != '.') {
            exeDir = exeDir.substr(0, pos);
        } else {
            exeDir = std::filesystem::current_path().string();
        }
        outputPath = exeDir + "/camera_capture.mp4";
    }

    std::cout << "Output video: " << outputPath << "\n";

    // Open camera
    ccap::Provider cameraProvider;
    cameraProvider.set(ccap::PropertyName::Width, 1280);
    cameraProvider.set(ccap::PropertyName::Height, 720);
    cameraProvider.set(ccap::PropertyName::FrameRate, 30.0);

    int deviceIndex = selectCamera(cameraProvider, &commandLine);
    cameraProvider.open(deviceIndex, true);

    if (!cameraProvider.isStarted()) {
        std::cerr << "Failed to start camera!\n";
        return -1;
    }

    int realWidth = static_cast<int>(cameraProvider.get(ccap::PropertyName::Width));
    int realHeight = static_cast<int>(cameraProvider.get(ccap::PropertyName::Height));
    double realFps = cameraProvider.get(ccap::PropertyName::FrameRate);

    printf("Camera started: %dx%d @ %.2f fps\n", realWidth, realHeight, realFps);

    // Configure and open video writer
    ccap::WriterConfig writerConfig;
    writerConfig.width = static_cast<uint32_t>(realWidth);
    writerConfig.height = static_cast<uint32_t>(realHeight);
    writerConfig.frameRate = realFps > 0.0 ? realFps : 30.0;

    ccap::VideoWriter writer;
    if (!writer.open(outputPath, writerConfig)) {
        std::cerr << "Failed to open video writer!\n";
        return -1;
    }

    // Record ~5 seconds
    constexpr int kMaxFrames = 150;
    int recorded = 0;
    using Clock = std::chrono::steady_clock;
    Clock::time_point recordStart;
    std::cout << "Recording " << kMaxFrames << " frames (~5 seconds)...\n";

    while (recorded < kMaxFrames) {
        auto frame = cameraProvider.grab(3000);
        if (!frame) {
            std::cerr << "Timeout waiting for camera frame.\n";
            break;
        }

        if (recorded == 0) {
            recordStart = Clock::now();
        }
        auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - recordStart);
        uint64_t timestampNs = static_cast<uint64_t>(elapsedNs.count());

        if (!writer.writeFrame(*frame, timestampNs)) {
            std::cerr << "Failed to write frame " << recorded << "\n";
        }

        if (++recorded % 30 == 0) {
            printf("  Recorded %d/%d frames...\n", recorded, kMaxFrames);
        }
    }

    writer.close();
    cameraProvider.stop();
    cameraProvider.close();

    printf("Done! %d frames saved to: %s\n", recorded, outputPath.c_str());
    return 0;
}

#endif // CCAP_ENABLE_VIDEO_WRITER
