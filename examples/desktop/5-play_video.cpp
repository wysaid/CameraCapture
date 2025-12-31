/**
 * @file 5-play_video.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Example for playing video files using ccap::Provider
 * @date 2025-12
 *
 * This example demonstrates how to use ccap::Provider to play video files.
 * The same API works for both camera capture and video file playback.
 */

#include "utils/helper.h"

#include <ccap.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
#ifdef __linux__
    std::cerr << "\n[WARNING] Video playback is currently not supported on Linux." << std::endl;
    std::cerr << "This feature may be implemented in a future version." << std::endl;
    std::cerr << "Currently supported platforms: Windows, macOS\n"
              << std::endl;
    return 0;
#endif

    /// Enable verbose log to see debug information
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    // Set error callback to receive error notifications
    ccap::setErrorCallback([](ccap::ErrorCode errorCode, std::string_view description) {
        std::cerr << "Error - Code: " << static_cast<int>(errorCode)
                  << ", Description: " << description << std::endl;
    });

    std::string videoPath;

    if (argc < 2) {
        // Check if test.mp4 exists in current directory
        std::string defaultVideo = "test.mp4";
        if (std::filesystem::exists(defaultVideo)) {
            std::cout << "No video path provided, using default: " << defaultVideo << std::endl;
            videoPath = defaultVideo;
        } else {
            std::cerr << "Usage: " << argv[0] << " <video_file_path>" << std::endl;
            std::cerr << "Example: " << argv[0] << " /path/to/video.mp4" << std::endl;
            std::cerr << "\nNote: You can also place a test.mp4 file in the same directory as this executable." << std::endl;
            return -1;
        }
    } else {
        videoPath = argv[1];
    }

    // Check if file exists
    if (!std::filesystem::exists(videoPath)) {
        std::cerr << "Error: File not found: " << videoPath << std::endl;
        return -1;
    }

    std::string cwd = argv[0];
    if (auto lastSlashPos = cwd.find_last_of("/\\"); lastSlashPos != std::string::npos && cwd[0] != '.') {
        cwd = cwd.substr(0, lastSlashPos);
    } else {
        cwd = std::filesystem::current_path().string();
    }

    /// Create a capture directory for saving frames
    std::string captureDir = cwd + "/video_frames";
    if (!std::filesystem::exists(captureDir)) {
        std::filesystem::create_directory(captureDir);
    }

    ccap::Provider provider;

    // Set output format (works for both camera and video file)
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::RGB24);

    // Open the video file - the same open() method works for both camera and file
    std::cout << "Opening video file: " << videoPath << std::endl;
    if (!provider.open(videoPath, true)) {
        std::cerr << "Failed to open video file!" << std::endl;
        return -1;
    }

    // Check if we are in file mode
    if (provider.isFileMode()) {
        std::cout << "Provider is in FILE mode" << std::endl;

        // Get video properties (only available in file mode)
        double duration = provider.get(ccap::PropertyName::Duration);
        double frameCount = provider.get(ccap::PropertyName::FrameCount);
        double frameRate = provider.get(ccap::PropertyName::FrameRate);
        int width = static_cast<int>(provider.get(ccap::PropertyName::Width));
        int height = static_cast<int>(provider.get(ccap::PropertyName::Height));

        printf("Video properties:\n");
        printf("  Duration: %.2f seconds\n", duration);
        printf("  Frame count: %.0f\n", frameCount);
        printf("  Frame rate: %.2f fps\n", frameRate);
        printf("  Resolution: %dx%d\n", width, height);

        // Set playback speed (only works in file mode)
        provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);
    } else {
        std::cout << "Provider is in CAMERA mode (this shouldn't happen with a file path)" << std::endl;
    }

    if (!provider.isStarted()) {
        std::cerr << "Failed to start playback!" << std::endl;
        return -1;
    }

    std::cout << "Playback started. Capturing first 30 frames..." << std::endl;

    /// Grab frames from the video file
    int maxFrames = 30;
    while (auto frame = provider.grab(3000)) {
        printf("Frame %d: width=%d, height=%d, bytes=%d, time=%.2fs\n",
               (int)frame->frameIndex, frame->width, frame->height,
               (int)frame->sizeInBytes,
               provider.get(ccap::PropertyName::CurrentTime));

        // Save every 10th frame
        if (frame->frameIndex % 10 == 0) {
            if (auto dumpFile = ccap::dumpFrameToDirectory(frame.get(), captureDir); !dumpFile.empty()) {
                std::cout << "  Frame saved to: " << dumpFile << std::endl;
            }
        }

        if ((int)frame->frameIndex >= maxFrames - 1) {
            std::cout << "Captured " << maxFrames << " frames, stopping..." << std::endl;
            break;
        }
    }

    // Demonstrate seeking (only works in file mode)
    if (provider.isFileMode()) {
        std::cout << "\nDemonstrating seek functionality..." << std::endl;

        // Seek to middle of video
        double duration = provider.get(ccap::PropertyName::Duration);
        double seekTime = duration / 2.0;

        std::cout << "Seeking to " << seekTime << " seconds..." << std::endl;
        if (provider.set(ccap::PropertyName::CurrentTime, seekTime)) {
            std::cout << "Seek successful. Current time: "
                      << provider.get(ccap::PropertyName::CurrentTime) << " seconds" << std::endl;
        }
    }

    provider.stop();
    provider.close();

    std::cout << "Video playback completed." << std::endl;
    return 0;
}
