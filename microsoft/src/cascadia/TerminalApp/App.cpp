// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"
#include "App.g.cpp"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace xaml = ::winrt::Windows::UI::Xaml;

#ifdef _DEBUG
namespace
{
    void _appendLaunchDebugLog(const std::wstring_view message)
    {
        try
        {
            wchar_t tempPath[MAX_PATH]{};
            if (!GetTempPathW(ARRAYSIZE(tempPath), tempPath))
            {
                return;
            }

            const auto logPath = std::filesystem::path{ tempPath } / L"wt-launch-debug.log";
            std::ofstream output{ logPath, std::ios::binary | std::ios::app };
            if (!output)
            {
                return;
            }

            const auto utf8 = til::u16u8(std::wstring{ message } + L"\n");
            output.write(utf8.data(), gsl::narrow_cast<std::streamsize>(utf8.size()));
        }
        CATCH_LOG();
    }
}
#endif

namespace winrt::TerminalApp::implementation
{
    App::App()
    {
#ifdef _DEBUG
        _appendLaunchDebugLog(L"App::App ctor enter");
#endif
        Initialize();

        // Disable XAML's automatic backplating of text when in High Contrast
        // mode: we want full control of and responsibility for the foreground
        // and background colors that we draw in XAML.
        HighContrastAdjustment(::winrt::Windows::UI::Xaml::ApplicationHighContrastAdjustment::None);
    }

    void App::Initialize()
    {
#ifdef _DEBUG
        _appendLaunchDebugLog(L"App::Initialize enter");
#endif
        // LOAD BEARING
#ifdef _DEBUG
        _appendLaunchDebugLog(L"App::Initialize before Control provider");
#endif
        AddOtherProvider(winrt::Microsoft::Terminal::Control::XamlMetaDataProvider{});
#ifdef _DEBUG
        _appendLaunchDebugLog(L"App::Initialize after Control provider");
        _appendLaunchDebugLog(L"App::Initialize before MUX provider");
#endif
        AddOtherProvider(winrt::Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider{});
#ifdef _DEBUG
        _appendLaunchDebugLog(L"App::Initialize providers-added");
#endif

        const auto dispatcherQueue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (!dispatcherQueue)
        {
#ifdef _DEBUG
            _appendLaunchDebugLog(L"App::Initialize before WindowsXamlManager");
#endif
            _windowsXamlManager = xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
#ifdef _DEBUG
            _appendLaunchDebugLog(L"App::Initialize after WindowsXamlManager");
#endif
        }
        else
        {
#ifdef _DEBUG
            _appendLaunchDebugLog(L"App::Initialize dispatcher-already-exists");
#endif
            FAIL_FAST_MSG("Terminal is not intended to run as a Universal Windows Application");
        }
    }

    AppLogic App::Logic()
    {
        static AppLogic logic;
        return logic;
    }

    /// <summary>
    /// Invoked when the application is launched normally by the end user.  Other entry points
    /// will be used such as when the application is launched to open a specific file.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched(const LaunchActivatedEventArgs& /*e*/)
    {
        // We used to support a pure UWP version of the Terminal. This method
        // was only ever used to do UWP-specific setup of our App.
    }

    void App::PrepareForSettingsUI()
    {
        if (!std::exchange(_preparedForSettingsUI, true))
        {
            AddOtherProvider(winrt::Microsoft::Terminal::Settings::Editor::XamlMetaDataProvider{});
        }
    }
}
