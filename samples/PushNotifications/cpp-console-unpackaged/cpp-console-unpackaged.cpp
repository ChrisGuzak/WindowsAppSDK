﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include <windows.h>
#include <iostream>

#include <appmodel.h>
#include <wil/result.h>
#include <wil/cppwinrt.h>

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Background.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Microsoft.Windows.PushNotifications.h>
#include <winrt/Windows.Globalization.DateTimeFormatting.h>

#include <MddBootstrap.h>


using namespace winrt::Microsoft::Windows::AppLifecycle;
using namespace winrt::Microsoft::Windows::PushNotifications;
using namespace winrt::Windows::ApplicationModel::Background; // BackgroundTask APIs
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Globalization::DateTimeFormatting;

// To obtain an AAD RemoteIdentifier for your app,
// follow the instructions on https://docs.microsoft.com/azure/active-directory/develop/quickstart-register-app
//winrt::guid remoteId{ "00000000-0000-0000-0000-000000000000"}; // Replace this with own remoteId
winrt::guid remoteId{ "0160ee84-0c53-4851-9ff2-d7f5a87ed914" };

HRESULT LoadWindowsAppSDK()
{
    // Major.Minor version, MinVersion=0 to find any framework package for this major.minor version
    const UINT32 majorMinorVersion{ 0x00040001 };
    PCWSTR versionTag{ L"" };
    const PACKAGE_VERSION minVersion{};
    HRESULT hr{ MddBootstrapInitialize(majorMinorVersion, versionTag, minVersion) };
    if (FAILED(hr))
    {
        wprintf(L"Error 0x%08X in MddBootstrapInitialize(0x%08X, %s, %hu.%hu.%hu.%hu)\n",
            hr, majorMinorVersion, versionTag, minVersion.Major, minVersion.Minor, minVersion.Build, minVersion.Revision);
        return hr;
    }

    return S_OK;
}

winrt::Windows::Foundation::IAsyncOperation<PushNotificationChannel> RequestChannelAsync()
{
    auto channelOperation = PushNotificationManager::CreateChannelAsync(remoteId);

    // Setup the inprogress event handler
    channelOperation.Progress(
        [](auto&& sender, auto&& args)
        {
            if (args.status == PushNotificationChannelStatus::InProgress)
            {
                // This is basically a noop since it isn't really an error state
                std::cout << "Channel request is in progress." << std::endl << std::endl;
            }
            else if (args.status == PushNotificationChannelStatus::InProgressRetry)
            {
                LOG_HR_MSG(
                    args.extendedError,
                    "The channel request is in back-off retry mode because of a retryable error! Expect delays in acquiring it. RetryCount = %d",
                    args.retryCount);
            }
        });

    auto result = co_await channelOperation;

    if (result.Status() == PushNotificationChannelStatus::CompletedSuccess)
    {
        auto channel = result.Channel();

        DateTimeFormatter formater = DateTimeFormatter(L"on {month.abbreviated} {day.integer(1)}, {year.full} at {hour.integer(1)}:{minute.integer(2)}:{second.integer(2)}");

        std::cout << "Channel Uri: " << winrt::to_string(channel.Uri().ToString()) << std::endl << std::endl;
        std::wcout << L"Channel Uri will expire " << formater.Format(channel.ExpirationTime()).c_str() << std::endl;

        // Caller's responsibility to keep the channel alive
        co_return channel;
    }
    else if (result.Status() == PushNotificationChannelStatus::CompletedFailure)
    {
        std::cout << result.ExtendedError() << "We hit a critical non-retryable error with channel request!" << std::endl;
        co_return nullptr;
    }
    else
    {
        LOG_HR_MSG(result.ExtendedError(), "Some other failure occurred.");
        co_return nullptr;
    }

};

winrt::Microsoft::Windows::PushNotifications::PushNotificationChannel RequestChannel()
{
    auto task = RequestChannelAsync();
    if (task.wait_for(std::chrono::seconds(300)) != AsyncStatus::Completed)
    {
        task.Cancel();
        return nullptr;
    }

    auto result = task.GetResults();
    return result;
}

// Register Push Event for Foreground
void RegisterForegroundNotificationsHandler(const winrt::Microsoft::Windows::PushNotifications::PushNotificationChannel& channel)
{
    winrt::event_token token = channel.PushReceived([](const auto&, PushNotificationReceivedEventArgs const& args)
        {
            auto payload = args.Payload();

            // Do stuff to process the raw payload
            std::string payloadString(payload.begin(), payload.end());
            std::cout << "Push notification content received from FOREGROUND: " << payloadString << std::endl << std::endl;
            args.Handled(true);
        });
}

int main()
{
    // An unpackaged app must initialize dynamic dependencies so we can consume the Windows App SDK APIs.
    HRESULT loadWindowsAppSDKHr = LoadWindowsAppSDK();
    if (FAILED(loadWindowsAppSDKHr))
    {
        std::wcout << "Could not load Windows App SDK!" << std::endl;
        return 1;
    }

    PushNotificationActivationInfo info(PushNotificationRegistrationActivators::ProtocolActivator);
    PushNotificationManager::RegisterActivator(info);
    // We do not call PushNotificationManager::RegisterActivator
    //  - because the we wouldn't be able to receive background activations, once the app has closed.

    auto args = AppInstance::GetCurrent().GetActivatedEventArgs();
    auto kind = args.Kind();
    switch (kind)
    {

    case ExtendedActivationKind::Launch:
    {

        // request a channel
        PushNotificationChannel channel = RequestChannel();

        // register the chaneel
        if (channel)
        {
            RegisterForegroundNotificationsHandler(channel);
        }
        else
        {
            // troubleshooting, you would get this error when passing 0 as the guid.
            std::cout << "error" << std::endl;
        }

        std::cout << "Press 'Enter' at any time to exit App." << std::endl;
        std::cin.ignore();
    }
    break;

    case ExtendedActivationKind::Push:
    {
        PushNotificationReceivedEventArgs pushArgs = args.Data().as<PushNotificationReceivedEventArgs>();

        // Call GetDeferral to ensure that code runs in low power
        auto deferral = pushArgs.GetDeferral();

        auto payload = pushArgs.Payload();

        // Do stuff to process the raw payload
        std::string payloadString(payload.begin(), payload.end());
        std::cout << "Push notification content received from BACKGROUND: " << payloadString.c_str() << std::endl;
        std::cout << "Press 'Enter' to exit the App." << std::endl;

        // Call Complete on the deferral when finished processing the payload.
        // This removes the override that kept the app running even when the system was in a low power mode.
        deferral.Complete();
        std::cin.ignore();
    }
    break;

    default:
        // Unexpected activation type
        std::cout << "Unexpected activation type" << std::endl;
        std::cout << "Press 'Enter' to exit the App." << std::endl;
        std::cin.ignore();
        break;
    } //switch

    // Uninitialize dynamic dependencies.
    MddBootstrapShutdown();
}
