/**
 * @file ccap_cli_wuffs.cpp
 * @brief Wuffs-based image format support for ccap CLI
 * @author wysaid (this@wysaid.org)
 * @date 2025-12
 *
 * This file provides additional image format support using the wuffs library.
 * Note: Wuffs is primarily a decoding library, so this currently enables
 * reading more input formats. Output is still in BMP format.
 * Only compiled when WITH_WUFFS is enabled.
 */

#ifdef CCAP_CLI_WITH_WUFFS

#include "ccap_cli.h"

#include <ccap.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// Define implementation before including wuffs
#define WUFFS_IMPLEMENTATION

#include "wuffs-v0.4.c"

namespace ccap_cli {

/**
 * @brief Save frame using wuffs library
 *
 * Note: Wuffs is primarily a decoding library and doesn't have PNG/JPEG encoders.
 * This function currently saves as BMP format but is structured to allow
 * adding encoding support if wuffs adds encoders in the future.
 *
 * @param frame Video frame to save (must be RGB format)
 * @param outputPath Output file path without extension (extension will be added)
 * @return true on success, false on error
 */
bool saveFrameWithWuffs(ccap::VideoFrame* frame, const std::string& outputPath) {
    if (!frame || !frame->data[0]) {
        fprintf(stderr, "Invalid frame data\n");
        return false;
    }

    // Check if it's RGB format
    if (!ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatRGBColorBit)) {
        fprintf(stderr, "Frame must be RGB format for image saving\n");
        return false;
    }

    bool hasAlpha = ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatAlphaColorBit);
    bool isBGR = ccap::pixelFormatInclude(frame->pixelFormat, ccap::kPixelFormatBGRBit);
    bool isTopToBottom = (frame->orientation == ccap::FrameOrientation::TopToBottom);

    // Currently wuffs doesn't have PNG/JPEG encoders, so save as BMP
    // The outputPath is expected to be without extension, we add .bmp
    std::string bmpPath = outputPath + ".bmp";
    if (ccap::saveRgbDataAsBMP(bmpPath.c_str(), frame->data[0],
                               frame->width, frame->stride[0], frame->height,
                               isBGR, hasAlpha, isTopToBottom)) {
        printf("Saved BMP to: %s\n", bmpPath.c_str());
        return true;
    }

    fprintf(stderr, "Failed to save BMP: %s\n", bmpPath.c_str());
    return false;
}

} // namespace ccap_cli

#endif // CCAP_CLI_WITH_WUFFS
