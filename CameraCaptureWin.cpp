/**
 * @file CameraCaptureWin.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Header file for CameraCapture class.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureWin.h"

namespace ccap
{
ProviderWin::~ProviderWin() = default;

bool ProviderWin::open(std::string_view deviceName)
{
    return false;
}

bool ProviderWin::isOpened() const
{
    return false;
}

void ProviderWin::close()
{
}

bool ProviderWin::start()
{
    return false;
}

void ProviderWin::pause()
{
}

bool ProviderWin::isStarted() const
{
    return false;
}

bool ProviderWin::set(Property prop, double value)
{
    return false;
}

double ProviderWin::get(Property prop)
{
    return 0;
}

std::shared_ptr<Frame> ProviderWin::grab(bool waitNewFrame)
{
    return std::shared_ptr<Frame>();
}

void ProviderWin::setNewFrameCallback(std::function<void(std::shared_ptr<Frame>)> callback)
{
    m_callback = std::move(callback);
}

void ProviderWin::setFrameAllocator(std::shared_ptr<Allocator> allocator)
{
    m_allocator = std::move(allocator);
}
} // namespace ccap
#endif