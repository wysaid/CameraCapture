/**
 * @file CameraCaptureMac.mm
 * @author wysaid (this@wysaid.org)
 * @brief Source file for ProviderMac class.
 * @date 2025-04
 *
 */

#include <memory>
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

bool ProviderMac::start()
{
    // Implementation for starting the camera capture on macOS
    return false;
}

void ProviderMac::stop()
{
    // Implementation for stopping the camera capture on macOS
}

bool ProviderMac::isStarted() const
{
    // Implementation for checking if the camera capture is started on macOS
    return false;
}

} // namespace ccap

#endif