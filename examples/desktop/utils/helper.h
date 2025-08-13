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

// Select a camera device index interactively when multiple devices are found.
// Returns:
//  - index >= 0: selected device index
//  - -1: zero or one device available (use default/open first)
typedef struct CcapProvider CcapProvider; // forward declaration from ccap_c.h
int selectCamera(CcapProvider* provider);
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
int selectCamera(ccap::Provider& provider);
#endif
