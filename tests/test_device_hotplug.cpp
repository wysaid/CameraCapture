/**
 * @file test_device_hotplug.cpp
 * @brief Interactive test for device hot-plug handling with crash prevention
 *
 * This test verifies crash prevention when enumerating devices that have been
 * physically unplugged while their drivers remain active. Some camera drivers
 * (especially VR headsets like Oculus Quest 3) can cause crashes when their
 * BindToObject() calls are made after the device is disconnected.
 *
 * The fix uses Windows SEH (Structured Exception Handling) to catch and handle
 * these crashes gracefully, allowing enumeration to continue.
 *
 * Usage:
 *   1. Run this test with all devices connected
 *   2. When prompted, physically unplug a device
 *   3. Press Enter to continue enumeration
 *   4. The test verifies that enumeration completes without crashing
 *
 * This is NOT a regular unit test. It requires manual interaction and should be run
 * separately from automated test suites.
 *
 * @see https://github.com/wysaid/CameraCapture/issues/26
 */

#include "ccap.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

// ANSI color codes for better output readability
constexpr const char* COLOR_RESET = "\033[0m";
constexpr const char* COLOR_GREEN = "\033[32m";
constexpr const char* COLOR_YELLOW = "\033[33m";
constexpr const char* COLOR_RED = "\033[31m";
constexpr const char* COLOR_BLUE = "\033[34m";
constexpr const char* COLOR_CYAN = "\033[36m";

void printHeader(const char* message) {
    std::cout << "\n" << COLOR_CYAN << "========================================" << COLOR_RESET << "\n";
    std::cout << COLOR_CYAN << message << COLOR_RESET << "\n";
    std::cout << COLOR_CYAN << "========================================" << COLOR_RESET << "\n\n";
}

void printSuccess(const char* message) {
    std::cout << COLOR_GREEN << "[SUCCESS] " << message << COLOR_RESET << "\n";
}

void printWarning(const char* message) {
    std::cout << COLOR_YELLOW << "[WARNING] " << message << COLOR_RESET << "\n";
}

void printError(const char* message) {
    std::cout << COLOR_RED << "[ERROR] " << message << COLOR_RESET << "\n";
}

void printInfo(const char* message) {
    std::cout << COLOR_BLUE << "[INFO] " << message << COLOR_RESET << "\n";
}

void waitForUserInput() {
    std::cout << COLOR_YELLOW << "\n>>> Press Enter to continue..." << COLOR_RESET << std::flush;
    std::string dummy;
    std::getline(std::cin, dummy);
}

void printDeviceList(const std::vector<std::string>& devices, const char* title) {
    std::cout << "\n" << COLOR_BLUE << title << COLOR_RESET << "\n";
    std::cout << "----------------------------------------\n";

    if (devices.empty()) {
        printWarning("No devices found!");
    } else {
        for (size_t i = 0; i < devices.size(); ++i) {
            std::cout << "  [" << i << "] " << devices[i] << "\n";
        }
    }
    std::cout << "----------------------------------------\n";
}

void countdownTimer(int seconds) {
    std::cout << "\n" << COLOR_YELLOW;
    for (int i = seconds; i > 0; --i) {
        std::cout << "\rStarting test in " << i << " second(s)... " << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "\r" << COLOR_RESET;
}

} // anonymous namespace

/**
 * @brief Test scenario 1: Enumerate devices before and after unplugging
 *
 * This test verifies that device enumeration:
 * 1. Completes successfully with all devices connected
 * 2. Does not crash when a device with buggy driver is unplugged
 * 3. Properly skips problematic devices and continues enumeration
 */
void testDeviceEnumerationWithHotplug() {
    printHeader("TEST: Device Enumeration with Hot-plug/Unplug");

    std::cout << COLOR_CYAN << "This test verifies crash prevention during device hot-plug:\n"
              << "  - Enumerating devices with buggy drivers after physical disconnect\n"
              << "  - Example: Oculus Quest 3 VR headset, some USB capture cards\n"
              << COLOR_RESET << "\n";

    // Step 1: Initial enumeration with all devices connected
    printInfo("Step 1: Enumerating devices with all cameras connected...");

    ccap::Provider provider;
    std::vector<std::string> devicesBeforeUnplug;

    try {
        devicesBeforeUnplug = provider.findDeviceNames();
        printSuccess("Initial enumeration completed successfully!");
        printDeviceList(devicesBeforeUnplug, "Devices Found (Initial):");
    } catch (const std::exception& e) {
        printError("Exception during initial enumeration!");
        std::cerr << "  Exception: " << e.what() << "\n";
        return;
    } catch (...) {
        printError("Unknown exception during initial enumeration!");
        return;
    }

    if (devicesBeforeUnplug.empty()) {
        printWarning("No devices found. Cannot proceed with hot-plug test.");
        printInfo("Please connect at least one camera device and try again.");
        return;
    }

    // Step 2: Wait for user to unplug a problematic device
    std::cout << "\n" << COLOR_YELLOW << "========================================\n"
              << "ACTION REQUIRED:\n"
              << "========================================\n" << COLOR_RESET;

    std::cout << COLOR_YELLOW << "Please perform the following:\n\n"
              << "  1. Identify a device with potentially buggy drivers\n"
              << "     (e.g., VR headset, virtual camera, or any device\n"
              << "      known to cause issues when unplugged)\n\n"
              << "  2. Physically disconnect/unplug that device\n\n"
              << "  3. Wait a few seconds for the OS to detect the change\n\n"
              << "  4. Press Enter to continue the test\n" << COLOR_RESET;

    std::cout << "\n" << COLOR_RED << "NOTE: If you don't have a problematic device, you can:\n"
              << "  - Still proceed to test normal device enumeration\n"
              << "  - Or unplug any camera to test hot-unplug handling\n" << COLOR_RESET;

    waitForUserInput();

    // Step 3: Enumerate again after device unplugging
    printInfo("Step 2: Re-enumerating devices after unplugging...");
    printInfo("This should NOT crash, even with buggy drivers!");

    std::vector<std::string> devicesAfterUnplug;
    bool enumerationSucceeded = false;

    try {
        // Create a new provider instance to force fresh enumeration
        ccap::Provider newProvider;
        devicesAfterUnplug = newProvider.findDeviceNames();
        enumerationSucceeded = true;

        printSuccess("Enumeration after unplug completed successfully!");
        printSuccess("Crash prevention is working correctly!");

        printDeviceList(devicesAfterUnplug, "Devices Found (After Unplug):");

    } catch (const std::exception& e) {
        printError("Exception during post-unplug enumeration!");
        std::cerr << "  Exception: " << e.what() << "\n";
        printError("This indicates the crash prevention may not be working!");
    } catch (...) {
        printError("Unknown exception during post-unplug enumeration!");
        printError("This indicates the crash prevention may not be working!");
    }

    // Step 4: Analyze results
    std::cout << "\n" << COLOR_CYAN << "========================================\n"
              << "TEST RESULTS ANALYSIS\n"
              << "========================================\n" << COLOR_RESET;

    if (!enumerationSucceeded) {
        printError("FAILED: Enumeration crashed or threw an exception!");
        printError("Crash prevention may not be working properly.");
        return;
    }

    printSuccess("PASSED: No crashes during enumeration!");

    // Compare device counts
    std::cout << "\n" << COLOR_BLUE << "Device Count Comparison:" << COLOR_RESET << "\n";
    std::cout << "  Before unplug: " << devicesBeforeUnplug.size() << " device(s)\n";
    std::cout << "  After unplug:  " << devicesAfterUnplug.size() << " device(s)\n";

    if (devicesAfterUnplug.size() < devicesBeforeUnplug.size()) {
        printSuccess("Device count decreased as expected.");
    } else if (devicesAfterUnplug.size() == devicesBeforeUnplug.size()) {
        printWarning("Device count unchanged. You may not have unplugged a device,");
        printWarning("or the unplugged device was already being filtered out.");
    } else {
        printWarning("Device count increased (unexpected). A new device may have been connected.");
    }

    // Identify missing devices
    if (devicesAfterUnplug.size() < devicesBeforeUnplug.size()) {
        std::cout << "\n" << COLOR_BLUE << "Missing Device(s):" << COLOR_RESET << "\n";
        for (const auto& device : devicesBeforeUnplug) {
            if (std::find(devicesAfterUnplug.begin(), devicesAfterUnplug.end(), device) == devicesAfterUnplug.end()) {
                std::cout << "  - " << device << "\n";
            }
        }
    }

    std::cout << "\n" << COLOR_GREEN << "========================================\n"
              << "TEST COMPLETED SUCCESSFULLY\n"
              << "========================================\n" << COLOR_RESET;
}

/**
 * @brief Test scenario 2: Try to open a device after unplugging
 *
 * This test verifies that attempting to open an unplugged device:
 * 1. Does not crash the application
 * 2. Returns a proper error status
 * 3. Allows the application to continue normally
 */
void testOpenUnpluggedDevice() {
    printHeader("TEST: Open Device After Unplug");

    std::cout << COLOR_CYAN << "This test verifies that opening a device that was\n"
              << "unplugged after enumeration handles the error gracefully.\n"
              << COLOR_RESET << "\n";

    ccap::Provider provider;
    auto devices = provider.findDeviceNames();

    if (devices.empty()) {
        printWarning("No devices found. Skipping this test.");
        return;
    }

    printDeviceList(devices, "Available Devices:");

    std::cout << "\n" << COLOR_YELLOW << "ACTION REQUIRED:\n"
              << "  1. Note one of the devices listed above\n"
              << "  2. Physically unplug that device\n"
              << "  3. Press Enter when ready\n" << COLOR_RESET;

    waitForUserInput();

    printInfo("Attempting to open the first device from the previous enumeration...");

    ccap::Provider testProvider;
    bool openResult = false;

    try {
        // Try to open the first device from our enumerated list
        // If it was unplugged, this should fail gracefully, not crash
        openResult = testProvider.open(devices[0], false);

        if (openResult) {
            printSuccess("Device opened successfully (it may not have been unplugged).");
            testProvider.close();
        } else {
            printSuccess("Device open failed gracefully (expected if unplugged).");
            printSuccess("No crash occurred - this is the correct behavior!");
        }
    } catch (const std::exception& e) {
        printError("Exception when trying to open device!");
        std::cerr << "  Exception: " << e.what() << "\n";
    } catch (...) {
        printError("Unknown exception when trying to open device!");
    }
}

/**
 * @brief Main interactive test program
 */
int main(int argc, char** argv) {
    std::cout << COLOR_CYAN;
    std::cout << R"(
╭────────────────────────────────────────────────────────────────╮
│                                                                │
│   Device Hot-plug Interactive Test                           │
│   Testing: Crash prevention for buggy camera drivers         │
│                                                                │
╰────────────────────────────────────────────────────────────────╯
    )" << COLOR_RESET << "\n";

    std::cout << COLOR_YELLOW << "IMPORTANT: This is an INTERACTIVE test that requires:\n"
              << "  - Physical access to camera devices\n"
              << "  - Ability to connect/disconnect devices during test\n"
              << "  - Manual verification of results\n" << COLOR_RESET << "\n";

#ifndef _WIN32
    printWarning("This test is primarily designed for Windows platform.");
    printWarning("The tested issue specifically affects Windows DirectShow implementation.");
    printInfo("You can still run this test on other platforms for basic functionality.");
    std::cout << "\n";
#endif

    std::cout << COLOR_BLUE << "The test will proceed in 5 seconds...\n" << COLOR_RESET;
    countdownTimer(5);

    // Run test scenarios
    try {
        testDeviceEnumerationWithHotplug();

        std::cout << "\n\n";
        std::cout << COLOR_CYAN << "Ready for second test scenario?\n" << COLOR_RESET;
        waitForUserInput();

        testOpenUnpluggedDevice();

    } catch (const std::exception& e) {
        printError("Unhandled exception in test!");
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        printError("Unknown unhandled exception in test!");
        return 1;
    }

    std::cout << "\n" << COLOR_GREEN;
    std::cout << R"(
╭────────────────────────────────────────────────────────────────╮
│                                                                │
│   All tests completed!                                         │
│   Thank you for testing the hot-plug crash prevention.        │
│                                                                │
╰────────────────────────────────────────────────────────────────╯
    )" << COLOR_RESET << "\n";

    return 0;
}
