/**
 * @file CameraCaptureMac.mm
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#if __APPLE__

#include "CameraCaptureMac.h"

namespace ccap
{
ProviderMac::~ProviderMac() = default;

bool ProviderMac::open(std::string_view deviceName)
{
    // Implementation for opening the camera on macOS
    return true;
}

bool ProviderMac::isOpened() const
{
    // Implementation for checking if the camera is opened on macOS
    return true;
}

void ProviderMac::close()
{
    // Implementation for closing the camera on macOS
}

void ProviderMac::start()
{
    // Implementation for starting the camera capture on macOS
}

void ProviderMac::pause()
{
    // Implementation for stopping the camera capture on macOS
}

void ProviderMac::isStarted()
{
    // Implementation for checking if the camera capture is started on macOS
}

bool ProviderMac::set(Pro


} // namespace ccap

#endif