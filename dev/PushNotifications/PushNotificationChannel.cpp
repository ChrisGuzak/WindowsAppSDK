﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "PushNotificationChannel.h"
#include "Microsoft.Windows.PushNotifications.PushNotificationChannel.g.cpp"
#include <winrt\Windows.Networking.PushNotifications.h>
#include <winrt\Windows.Foundation.h>
#include "PushNotificationReceivedEventArgs.h"

namespace winrt::Windows
{
    using namespace winrt::Windows::Networking::PushNotifications;
    using namespace winrt::Windows::Foundation;
}
namespace winrt::Microsoft
{
    using namespace winrt::Microsoft::Windows::PushNotifications;
}

namespace winrt::Microsoft::Windows::PushNotifications::implementation
{
    PushNotificationChannel::PushNotificationChannel(winrt::Windows::PushNotificationChannel const& channel): m_channel(channel) {}

    PushNotificationChannel::PushNotificationChannel(hstring const& channelUri, winrt::Windows::Foundation::DateTime const& extime)
    {
        winrt::Windows::Uri uriLocal{ channelUri };
        m_Uri = uriLocal;
        m_Extime = extime;
    }

    winrt::Windows::Uri PushNotificationChannel::Uri()
    {
        if (m_channel)
        {
            return winrt::Windows::Uri{ m_channel.Uri() };
        }
        else
        {
            return m_Uri;
        }
    }
    winrt::Windows::DateTime PushNotificationChannel::ExpirationTime()
    {
        if (m_channel)
        {
            return m_channel.ExpirationTime();
        }
        else
        {
            return m_Extime;
        }
    }
    void PushNotificationChannel::Close()
    {
        try
        {
            m_channel.Close();
        }
        catch (...)
        {
            auto channelCloseException = hresult_error(to_hresult());
            if (channelCloseException.code() != HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
            {
                throw hresult_error(to_hresult());
            }
        }
    }

    winrt::event_token PushNotificationChannel::PushReceived(winrt::Windows::TypedEventHandler<winrt::Microsoft::Windows::PushNotifications::PushNotificationChannel, winrt::Microsoft::Windows::PushNotifications::PushNotificationReceivedEventArgs> handler)
    {
        return m_channel.PushNotificationReceived([weak_self = get_weak(), handler](auto&&, auto&& args)
        {
            auto strong = weak_self.get();
            if (strong)
            {
                handler(*strong, winrt::make<winrt::Microsoft::Windows::PushNotifications::implementation::PushNotificationReceivedEventArgs>(args));
            };
        });
    }

    void PushNotificationChannel::PushReceived(winrt::event_token const& token) noexcept
    {
        m_channel.PushNotificationReceived(token);
    }
}
