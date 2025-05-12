/**
 * @file CameraCaptureMSMF.cpp
 * @author wysaid (this@wysaid.org)
 * @brief Implementation for ProviderMSMF class using MSMF.
 * @date 2025-04
 *
 */

#if defined(_WIN32) || defined(_MSC_VER)

#include "CameraCaptureMSMF.h"

namespace ccap
{
ProviderMSMF::ProviderMSMF() = default;

ProviderMSMF::~ProviderMSMF()
{
    ProviderMSMF::close();
}

std::vector<std::string> ProviderMSMF::findDeviceNames()
{
    return {};
}

bool ProviderMSMF::open(std::string_view deviceName)
{
    return true;
}

bool ProviderMSMF::isOpened() const
{
    return m_isOpened;
}

std::optional<DeviceInfo> ProviderMSMF::getDeviceInfo() const
{
    return {};
}

void ProviderMSMF::close()
{
}

bool ProviderMSMF::start()
{
    return false;
}

void ProviderMSMF::stop()
{
}

bool ProviderMSMF::isStarted() const
{
    return false;
}

ProviderImp* createProviderMSMF()
{ /// Not implemented yet
    return {};
}

} // namespace ccap
#endif
