
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AboutDialog.h"
#include "AboutDialog.g.cpp"
#include "../../../../src/core/generated/ExtBuildVersion.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal;

namespace winrt::TerminalApp::implementation
{
    AboutDialog::AboutDialog()
    {
        InitializeComponent();
    }

    void AboutDialog::OnPrimaryButtonClick(const IInspectable&, const winrt::Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs&)
    {
        winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri{ L"https://github.com/filegrid/terminal/issues/new" });
    }

    winrt::hstring AboutDialog::ApplicationDisplayName()
    {
        return L"GeekTerminal";
    }

    winrt::hstring AboutDialog::ApplicationVersion()
    {
        return winrt::hstring{ WorkspaceExt::Build::AboutVersion };
    }
}
