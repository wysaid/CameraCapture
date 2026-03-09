#include "helper.h"

#include "ccap_c.h"

#include <ccap.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr const char* kWindowsBackendEnvVar = "CCAP_WINDOWS_BACKEND";

ExampleCameraBackend parseCameraBackendArg(const char* arg) {
    if (arg == nullptr) {
        return EXAMPLE_CAMERA_BACKEND_UNSPECIFIED;
    }

    if (std::strcmp(arg, "--auto") == 0) {
        return EXAMPLE_CAMERA_BACKEND_AUTO;
    }
    if (std::strcmp(arg, "--msmf") == 0) {
        return EXAMPLE_CAMERA_BACKEND_MSMF;
    }
    if (std::strcmp(arg, "--dshow") == 0) {
        return EXAMPLE_CAMERA_BACKEND_DSHOW;
    }
    return EXAMPLE_CAMERA_BACKEND_UNSPECIFIED;
}

bool parseNonNegativeInt(const char* text, int* value) {
    if (text == nullptr || value == nullptr || *text == '\0') {
        return false;
    }

    char* end = nullptr;
    long parsed = std::strtol(text, &end, 10);
    if (end == text || parsed < 0 || parsed > INT_MAX) {
        return false;
    }

    while (*end != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end))) {
            return false;
        }
        ++end;
    }

    *value = static_cast<int>(parsed);
    return true;
}

int promptSelection(const char* prompt, int defaultValue, int maxValue) {
    char buffer[64] = {};
    std::printf("%s", prompt);
    if (std::fgets(buffer, sizeof(buffer), stdin) == nullptr) {
        return defaultValue;
    }

    char* end = nullptr;
    long parsed = std::strtol(buffer, &end, 10);
    if (end == buffer) {
        return defaultValue;
    }

    while (*end != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end))) {
            return defaultValue;
        }
        ++end;
    }

    if (parsed < 0 || parsed > maxValue) {
        return defaultValue;
    }

    return static_cast<int>(parsed);
}

std::string getEnvironmentValue(const char* name) {
#if defined(_WIN32) || defined(_WIN64)
    char* rawValue = nullptr;
    size_t rawLength = 0;
    if (_dupenv_s(&rawValue, &rawLength, name) != 0 || rawValue == nullptr) {
        return {};
    }

    std::string value(rawValue);
    std::free(rawValue);
    return value;
#else
    const char* rawValue = std::getenv(name);
    return rawValue != nullptr ? std::string(rawValue) : std::string();
#endif
}

void setEnvironmentValue(const char* name, const char* value) {
#if defined(_WIN32) || defined(_WIN64)
    SetEnvironmentVariableA(name, value);
#else
    if (value != nullptr && *value != '\0') {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}

class ScopedEnvironmentValue {
public:
    ScopedEnvironmentValue(const char* name, const char* value) :
        m_name(name),
        m_oldValue(getEnvironmentValue(name)),
        m_hadValue(!m_oldValue.empty()) {
        setEnvironmentValue(m_name.c_str(), value);
    }

    ~ScopedEnvironmentValue() {
        if (m_hadValue) {
            setEnvironmentValue(m_name.c_str(), m_oldValue.c_str());
        } else {
            setEnvironmentValue(m_name.c_str(), nullptr);
        }
    }

private:
    std::string m_name;
    std::string m_oldValue;
    bool m_hadValue = false;
};

int promptForDeviceIndex(const CcapDeviceNamesList& deviceList) {
    std::printf("Multiple devices found, please select one:\n");
    for (size_t index = 0; index < deviceList.deviceCount; ++index) {
        std::printf("  %zu: %s\n", index, deviceList.deviceNames[index]);
    }

    int selectedIndex = promptSelection("Enter the index of the device you want to use [0]: ", 0,
                                        static_cast<int>(deviceList.deviceCount) - 1);
    std::printf("Using device: %s\n", deviceList.deviceNames[selectedIndex]);
    return selectedIndex;
}

int promptForDeviceIndex(const std::vector<std::string>& names) {
    std::printf("Multiple devices found, please select one:\n");
    for (size_t index = 0; index < names.size(); ++index) {
        std::printf("  %zu: %s\n", index, names[index].c_str());
    }

    int selectedIndex = promptSelection("Enter the index of the device you want to use [0]: ", 0,
                                        static_cast<int>(names.size()) - 1);
    std::printf("Using device: %s\n", names[selectedIndex].c_str());
    return selectedIndex;
}

#if defined(_WIN32) || defined(_WIN64)
std::vector<std::string> listDevicesForBackend(const char* backend) {
    ScopedEnvironmentValue scopedEnv(kWindowsBackendEnvVar, backend);
    ccap::Provider provider;
    return provider.findDeviceNames();
}

bool shouldPromptForWindowsBackendSelection(const ExampleCommandLine* commandLine) {
    if (commandLine != nullptr && commandLine->cameraBackend != EXAMPLE_CAMERA_BACKEND_UNSPECIFIED) {
        return false;
    }

    if (!getEnvironmentValue(kWindowsBackendEnvVar).empty()) {
        return false;
    }

    return !listDevicesForBackend("msmf").empty() && !listDevicesForBackend("dshow").empty();
}

ExampleCameraBackend promptForWindowsBackendSelection() {
    std::printf("Windows camera backends detected. Select backend for this run:\n");
    std::printf("  0: auto  (prefer DirectShow, fallback to MSMF)\n");
    std::printf("  1: msmf\n");
    std::printf("  2: dshow\n");

    switch (promptSelection("Enter backend index [0]: ", 0, 2)) {
    case 1:
        return EXAMPLE_CAMERA_BACKEND_MSMF;
    case 2:
        return EXAMPLE_CAMERA_BACKEND_DSHOW;
    default:
        return EXAMPLE_CAMERA_BACKEND_AUTO;
    }
}

void applyWindowsBackendSelection(ExampleCameraBackend backend) {
    switch (backend) {
    case EXAMPLE_CAMERA_BACKEND_AUTO:
        setEnvironmentValue(kWindowsBackendEnvVar, nullptr);
        std::printf("Using Windows camera backend: auto (DirectShow preferred, MSMF fallback)\n");
        break;
    case EXAMPLE_CAMERA_BACKEND_MSMF:
        setEnvironmentValue(kWindowsBackendEnvVar, "msmf");
        std::printf("Using Windows camera backend: msmf\n");
        break;
    case EXAMPLE_CAMERA_BACKEND_DSHOW:
        setEnvironmentValue(kWindowsBackendEnvVar, "dshow");
        std::printf("Using Windows camera backend: dshow\n");
        break;
    case EXAMPLE_CAMERA_BACKEND_UNSPECIFIED:
        break;
    }
}
#endif

} // namespace

extern "C" {

void initExampleCommandLine(ExampleCommandLine* commandLine, int argc, char** argv) {
    if (commandLine == nullptr) {
        return;
    }

    commandLine->argc = argc;
    commandLine->argv = argv;
    commandLine->cameraBackend = EXAMPLE_CAMERA_BACKEND_UNSPECIFIED;

    if (argv == nullptr || argc <= 0) {
        return;
    }

    int writeIndex = 1;
    for (int readIndex = 1; readIndex < argc; ++readIndex) {
        ExampleCameraBackend backend = parseCameraBackendArg(argv[readIndex]);
        if (backend != EXAMPLE_CAMERA_BACKEND_UNSPECIFIED) {
#if defined(_WIN32) || defined(_WIN64)
            commandLine->cameraBackend = backend;
#endif
            continue;
        }

        argv[writeIndex++] = argv[readIndex];
    }

    commandLine->argc = writeIndex;
    argv[writeIndex] = nullptr;
}

void applyExampleCameraBackend(const ExampleCommandLine* commandLine) {
#if defined(_WIN32) || defined(_WIN64)
    ExampleCameraBackend backend = commandLine != nullptr ? commandLine->cameraBackend : EXAMPLE_CAMERA_BACKEND_UNSPECIFIED;
    if (backend == EXAMPLE_CAMERA_BACKEND_UNSPECIFIED && shouldPromptForWindowsBackendSelection(commandLine)) {
        backend = promptForWindowsBackendSelection();
    }

    applyWindowsBackendSelection(backend);
#else
    (void)commandLine;
#endif
}

int getExampleCameraIndex(const ExampleCommandLine* commandLine) {
    if (commandLine == nullptr || commandLine->argv == nullptr || commandLine->argc <= 1) {
        return -1;
    }

    int deviceIndex = -1;
    return parseNonNegativeInt(commandLine->argv[1], &deviceIndex) ? deviceIndex : -1;
}

const char* getExampleCameraBackendName(ExampleCameraBackend backend) {
    switch (backend) {
    case EXAMPLE_CAMERA_BACKEND_AUTO:
        return "auto";
    case EXAMPLE_CAMERA_BACKEND_MSMF:
        return "msmf";
    case EXAMPLE_CAMERA_BACKEND_DSHOW:
        return "dshow";
    case EXAMPLE_CAMERA_BACKEND_UNSPECIFIED:
    default:
        return "unspecified";
    }
}

// Create directory if not exists
void createDirectory(const char* path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        fprintf(stderr, "Failed to create directory %s: %s\n", path, ec.message().c_str());
    }
}

// Get current working directory (portable)
int getCurrentWorkingDirectory(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return -1;
    }

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        fprintf(stderr, "Failed to get current working directory: %s\n", ec.message().c_str());
        return -1;
    }

    std::string cwd_str = cwd.string();
    if (cwd_str.size() >= size) {
        return -1; // Buffer too small
    }
    strncpy(buffer, cwd_str.c_str(), size - 1);
    buffer[size - 1] = '\0';
    return 0;
}

int selectCamera(CcapProvider* provider, const ExampleCommandLine* commandLine) {
    int deviceIndex = getExampleCameraIndex(commandLine);
    if (deviceIndex >= 0) {
        return deviceIndex;
    }

    CcapDeviceNamesList deviceList;

    if (ccap_provider_find_device_names_list(provider, &deviceList) && deviceList.deviceCount > 1) {
        return promptForDeviceIndex(deviceList);
    }

    return -1; // One or no device, use default.
}

} // extern "C"

int selectCamera(ccap::Provider& provider, const ExampleCommandLine* commandLine) {
    int deviceIndex = getExampleCameraIndex(commandLine);
    if (deviceIndex >= 0) {
        return deviceIndex;
    }

    const auto names = provider.findDeviceNames();
    if (names.size() > 1) {
        return promptForDeviceIndex(names);
    }
    return -1; // One or no device, use default.
}
