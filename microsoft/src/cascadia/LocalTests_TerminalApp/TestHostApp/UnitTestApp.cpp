//-----------------------------------------------------------
// Copyright (c) Microsoft Corporation. All Rights Reserved.
//-----------------------------------------------------------
#include "pch.h"

#include <winrt/Microsoft.VisualStudio.TestPlatform.TestExecutor.WinRTCore.h>

namespace TestHostApp
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        RequestedTheme(winrt::Windows::UI::Xaml::ApplicationTheme::Dark);
    }

    /// <summary>
    /// Invoked when the application is launched normally by the end user.    Other entry points
    /// will be used such as when the application is launched to open a specific file.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched(winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const& e)
    {
        winrt::Windows::UI::Xaml::Window::Current().Activate();
        winrt::Microsoft::VisualStudio::TestPlatform::TestExecutor::WinRTCore::UnitTestClient::Run(e.Arguments());
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    winrt::Windows::UI::Xaml::Application::Start([](auto&&) {
        winrt::make<TestHostApp::App>();
    });
    return 0;
}
