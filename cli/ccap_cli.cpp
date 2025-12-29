/**
 * @file ccap_cli.cpp
 * @brief CLI tool for CameraCapture (ccap) library - Main entry point
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 *
 * A command-line interface for camera capture operations.
 * Supports camera enumeration, capture, format conversion, and optional window preview.
 *
 * This file serves as the main entry point, with functionality split into modules:
 * - args_parser: Command-line argument parsing
 * - ccap_cli_utils: Device management, format conversion, and preview utilities
 */

#include "args_parser.h"
#include "ccap_cli_utils.h"

#include <ccap.h>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ccap_cli::printUsage(argv[0]);
        return 0;
    }

    auto opts = ccap_cli::parseArgs(argc, argv);

    if (opts.showHelp) {
        ccap_cli::printUsage(argv[0]);
        return 0;
    }

    if (opts.showVersion) {
        ccap_cli::printVersion();
        return 0;
    }

    // Check for conflicting options
    if (opts.captureCountSpecified && opts.enableLoop) {
        std::cerr << "Error: -c/--count and --loop are mutually exclusive." << std::endl;
        std::cerr << "Use -c to limit captured frames, or --loop for video looping, but not both." << std::endl;
        return 1;
    }

    // --loop only applies to video mode
    if (opts.enableLoop && opts.videoFilePath.empty()) {
        std::cerr << "Warning: --loop option is only effective for video file playback." << std::endl;
    }

    // --save requires -o
    if (opts.saveFrames && opts.outputDir.empty()) {
        std::cerr << "Error: --save requires -o/--output to specify output directory." << std::endl;
        return 1;
    }

    if (opts.verbose) {
        ccap::setLogLevel(ccap::LogLevel::Verbose);
    }

    // Set error callback
    ccap::setErrorCallback([](ccap::ErrorCode errorCode, std::string_view description) {
        std::cerr << "Camera Error - Code: " << static_cast<int>(errorCode)
                  << ", Description: " << description << std::endl;
    });

    // Handle different modes
    if (opts.listDevices) {
        return ccap_cli::listDevices();
    }

    if (opts.showDeviceInfo) {
        return ccap_cli::showDeviceInfo(opts.deviceInfoIndex);
    }

    if (!opts.convertInput.empty()) {
        return ccap_cli::convertYuvToImage(opts);
    }

    // Check if video file playback is requested but not supported on Linux
#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    if (!opts.videoFilePath.empty()) {
        std::cerr << "Error: Video file playback is not supported on Linux.\n"
                  << "\n"
                  << "Video file playback is currently only available on:\n"
                  << "  - Windows (using Media Foundation)\n"
                  << "  - macOS (using AVFoundation)\n"
                  << "\n"
                  << "Linux support may be added in future versions.\n"
                  << "For now, you can:\n"
                  << "  - Use Windows or macOS for video processing\n"
                  << "  - Download pre-built binaries from: https://ccap.work\n"
                  << std::endl;
        return 1;
    }
#endif

    // Check if we should just print info (no action specified)
    bool hasAction = opts.enablePreview || opts.saveFrames || opts.captureCountSpecified || !opts.outputDir.empty();

    // If video file specified without action, print video info
    if (!opts.videoFilePath.empty() && !hasAction) {
        return ccap_cli::printVideoInfo(opts.videoFilePath);
    }

    // If camera device specified without action, print camera info
    if (opts.videoFilePath.empty() && !hasAction &&
        (opts.inputSource.empty() || !opts.deviceName.empty() || opts.deviceIndex >= 0)) {
        // Print camera info (equivalent to --device-info)
        if (!opts.deviceName.empty()) {
            // Find device index by name
            ccap::Provider provider;
            auto deviceNames = provider.findDeviceNames();
            for (size_t i = 0; i < deviceNames.size(); ++i) {
                if (deviceNames[i] == opts.deviceName) {
                    return ccap_cli::showDeviceInfo(static_cast<int>(i));
                }
            }
            std::cerr << "Device not found: " << opts.deviceName << std::endl;
            return 1;
        }
        return ccap_cli::showDeviceInfo(opts.deviceIndex);
    }

#ifdef CCAP_CLI_WITH_GLFW
    if (opts.enablePreview) {
        return ccap_cli::runPreview(opts);
    }
#else
    if (opts.enablePreview) {
        std::cerr << "Error: Preview feature is not available in this build.\n"
                  << "\n"
                  << "To use --preview functionality:\n"
                  << "  1. Download a pre-built version with preview support from: https://ccap.work\n"
                  << "  2. Or rebuild the CLI tool with GLFW support:\n"
                  << "     - Install GLFW development library: sudo apt-get install libglfw3-dev\n"
                  << "       (or equivalent package manager command for your system)\n"
                  << "     - Rebuild with: cmake -DCCAP_CLI_WITH_GLFW=ON ..\n"
                  << std::endl;
        return 1;
    }
#endif

    // Default: capture mode
    if (!opts.outputDir.empty() || opts.captureCountSpecified || opts.saveFrames) {
        return ccap_cli::captureFrames(opts);
    }

    // No specific action, show help
    ccap_cli::printUsage(argv[0]);
    return 0;
}
