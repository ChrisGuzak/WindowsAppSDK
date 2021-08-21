﻿#include "pch.h"

#include <NotificationsLongRunningProcess_h.h>

#include "platform.h"
#include "platformfactory.h"
#include <FrameworkUdk/PushNotifications.h>

void NotificationsLongRunningPlatformImpl::Initialize()
{
    auto lock = m_lock.lock_exclusive();
    THROW_HR_IF(WPN_E_PLATFORM_UNAVAILABLE, m_shutdown);

    if (m_initialized)
    {
        return;
    }

    // Schedule event signaling after 5 seconds. This is in case we don't have any apps to track in the LRP.
    // If we realize that we need to persist the LRP, timer should be canceled.
    m_shutdownTimerManager = std::make_unique<PlatformLifetimeTimerManager>();
    m_shutdownTimerManager->Setup();

    /* TODO: Verify registry and UDK list and make sure we have apps to be tracked */

    /* TODO: Load platform components */

    m_initialized = true;
}

void NotificationsLongRunningPlatformImpl::Shutdown() noexcept
{
    auto lock = m_lock.lock_exclusive();
    if (m_shutdown)
    {
        return;
    }

    /* TODO: Shut down your components */

    m_shutdown = true;
}

void NotificationsLongRunningPlatformImpl::WaitForWinMainEvent()
{
    THROW_HR_IF_NULL(E_UNEXPECTED, m_shutdownTimerManager.get());
    m_shutdownTimerManager->Wait();
}


STDMETHODIMP_(HRESULT __stdcall) NotificationsLongRunningPlatformImpl::RegisterFullTrustApplication(
    _In_ PCSTR processName, GUID remoteId, _Out_ LPWSTR* appId) noexcept
{
    auto lock = m_lock.lock_shared();

    UNREFERENCED_PARAMETER(processName);

    GUID guidReference;
    HRESULT hr = CoCreateGuid(&guidReference);

    StringFromCLSID(guidReference, appId);

    hr = PushNotifications_RegisterFullTrustApplication(*appId, remoteId);

    return hr;
}
