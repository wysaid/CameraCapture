/**
 * @file helper.h
 * @author wysaid (this@wysaid.org)
 * @brief Utility functions for ccap examples.
 * @date 2025-08
 *
 */
#pragma once

#include <stddef.h> // for size_t

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ExampleCameraBackend {
    EXAMPLE_CAMERA_BACKEND_UNSPECIFIED = 0,
    EXAMPLE_CAMERA_BACKEND_AUTO,
    EXAMPLE_CAMERA_BACKEND_MSMF,
    EXAMPLE_CAMERA_BACKEND_DSHOW,
} ExampleCameraBackend;

typedef struct ExampleCommandLine {
    int argc;
    char** argv;
    ExampleCameraBackend cameraBackend;
} ExampleCommandLine;

void initExampleCommandLine(ExampleCommandLine* commandLine, int argc, char** argv);
void applyExampleCameraBackend(const ExampleCommandLine* commandLine);
int getExampleCameraIndex(const ExampleCommandLine* commandLine);
const char* getExampleCameraBackendName(ExampleCameraBackend backend);

// Select a camera device index interactively when multiple devices are found.
// Returns:
//  - index >= 0: selected device index
//  - -1: zero or one device available (use default/open first)
typedef struct CcapProvider CcapProvider; // forward declaration from ccap_c.h
int selectCamera(CcapProvider* provider, const ExampleCommandLine* commandLine);
// Create directory if not exists (portable)
void createDirectory(const char* path);
// Get current working directory (portable)
// Returns 0 on success, -1 on failure
int getCurrentWorkingDirectory(char* buffer, size_t size);

#ifdef __cplusplus
} // extern "C"

// C++ overload for ccap::Provider
namespace ccap {
class Provider;
}
int selectCamera(ccap::Provider& provider, const ExampleCommandLine* commandLine = nullptr);
#endif
