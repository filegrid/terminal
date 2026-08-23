//-----------------------------------------------------------
// Copyright (c) Microsoft Corporation. All Rights Reserved.
//-----------------------------------------------------------
#pragma once

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.UI.Xaml.h>

namespace TestHostApp
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    struct App : winrt::Windows::UI::Xaml::ApplicationT<App>
    {
        App();
        void OnLaunched(winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const& e);
    };
}
