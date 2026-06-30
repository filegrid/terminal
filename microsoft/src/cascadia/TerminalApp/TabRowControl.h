// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TabRowControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabRowControl : TabRowControlT<TabRowControl>
    {
        TabRowControl();

        void OnNewTabButtonClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Controls::SplitButtonClickEventArgs& args);
        void OnNewTabButtonDrop(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);
        void OnNewTabButtonDragOver(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(bool, ShowElevationShield, PropertyChanged.raise, false);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, WorkspaceName, PropertyChanged.raise, L"");
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::Visibility, WorkspaceDirtyVisibility, PropertyChanged.raise, winrt::Windows::UI::Xaml::Visibility::Collapsed);
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::Visibility, WorkspaceSaveVisibility, PropertyChanged.raise, winrt::Windows::UI::Xaml::Visibility::Collapsed);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, WorkspaceLockGlyph, PropertyChanged.raise, L"");
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::Visibility, WorkspaceNameVisibility, PropertyChanged.raise, winrt::Windows::UI::Xaml::Visibility::Collapsed);
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::Visibility, WorkspaceLockVisibility, PropertyChanged.raise, winrt::Windows::UI::Xaml::Visibility::Collapsed);
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::Media::Brush, WorkspaceBackgroundBrush, PropertyChanged.raise, nullptr);
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::Media::Brush, WorkspaceForegroundBrush, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabRowControl);
}
