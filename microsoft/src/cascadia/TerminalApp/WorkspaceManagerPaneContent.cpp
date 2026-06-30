// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceManagerPaneContent.h"
#include "Utils.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    WorkspaceManagerPaneContent::WorkspaceManagerPaneContent(const UIElement content,
                                                             const CascadiaSettings settings) :
        _root{ Controls::Page{} }
    {
        UpdateSettings(settings);
        UpdateContent(content);
    }

    void WorkspaceManagerPaneContent::UpdateContent(const UIElement& content)
    {
        _root.Background(BackgroundBrush());
        _root.Content(content);
    }

    void WorkspaceManagerPaneContent::UpdateSettings(const CascadiaSettings& settings)
    {
        _requestedTheme = settings.GlobalSettings().CurrentTheme().RequestedTheme();
        _root.Background(BackgroundBrush());
    }

    FrameworkElement WorkspaceManagerPaneContent::GetRoot()
    {
        return _root;
    }

    Size WorkspaceManagerPaneContent::MinimumSize()
    {
        return { 1, 1 };
    }

    void WorkspaceManagerPaneContent::Focus(const FocusState reason)
    {
        if (reason != FocusState::Unfocused)
        {
            _root.Focus(reason);
        }
    }

    void WorkspaceManagerPaneContent::Close()
    {
    }

    INewContentArgs WorkspaceManagerPaneContent::GetNewTerminalArgs(const BuildStartupKind /*kind*/) const
    {
        return BaseContentArgs(L"workspace-manager");
    }

    winrt::hstring WorkspaceManagerPaneContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xE713" };
        return winrt::hstring{ glyph };
    }

    Windows::Foundation::IReference<winrt::Windows::UI::Color> WorkspaceManagerPaneContent::TabColor() const noexcept
    {
        return nullptr;
    }

    winrt::Windows::UI::Xaml::Media::Brush WorkspaceManagerPaneContent::BackgroundBrush()
    {
        static const auto key = winrt::box_value(L"SettingsUiTabBrush");
        return ThemeLookup(Application::Current().Resources(), _requestedTheme, key).try_as<winrt::Windows::UI::Xaml::Media::Brush>();
    }
}
