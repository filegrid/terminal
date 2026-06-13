// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "winrt/TerminalApp.h"
#include "BasicPaneEvents.h"

namespace winrt::TerminalApp::implementation
{
    class WorkspaceManagerPaneContent : public winrt::implements<WorkspaceManagerPaneContent, IPaneContent>, public BasicPaneEvents
    {
    public:
        WorkspaceManagerPaneContent(winrt::Windows::UI::Xaml::UIElement content,
                                    winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings settings);

        void UpdateContent(const winrt::Windows::UI::Xaml::UIElement& content);
        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();
        winrt::Windows::Foundation::Size MinimumSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(const BuildStartupKind kind) const;

        winrt::hstring Title() { return RS_(L"WorkspaceManageMenuItem"); }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept;
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

    private:
        winrt::Windows::UI::Xaml::Controls::Page _root{ nullptr };
        winrt::Windows::UI::Xaml::ElementTheme _requestedTheme;
    };
}
