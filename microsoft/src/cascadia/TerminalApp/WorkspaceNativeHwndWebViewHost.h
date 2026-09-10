// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>

#include <memory>
#include <functional>
#include <string_view>

struct ICoreWebView2;

namespace winrt::TerminalApp::implementation
{
    // This owns a browser controller hosted by a real child HWND. The XAML
    // element is geometry only: it never contains a WebView2 control, visual,
    // or forwarded browser input.
    class WorkspaceNativeHwndWebViewHost final : public std::enable_shared_from_this<WorkspaceNativeHwndWebViewHost>
    {
    public:
        using CoreReadyCallback = std::function<void(ICoreWebView2*)>;

        static std::shared_ptr<WorkspaceNativeHwndWebViewHost> Create(
            winrt::Windows::UI::Xaml::FrameworkElement layout,
            HWND parentWindow,
            winrt::hstring url,
            CoreReadyCallback coreReady = {});

        // Mapping uses the native WebView2 COM API, so callers do not need to
        // pull WRL/WebView2 implementation details into XAML glue code.
        static HRESULT MapVirtualHostToFolder(ICoreWebView2* core,
                                              std::wstring_view hostName,
                                              std::wstring_view folder) noexcept;

        ~WorkspaceNativeHwndWebViewHost();

        void Show();
        void Hide();
        void Close() noexcept;
        void Focus();

    private:
        WorkspaceNativeHwndWebViewHost(winrt::Windows::UI::Xaml::FrameworkElement layout,
                                       HWND parentWindow,
                                       winrt::hstring url,
                                       CoreReadyCallback coreReady);

        void _Initialize();
        void _Resize();
        void _SendMouse(unsigned int kind, const winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs& args);
        void _LogFailure(std::wstring_view eventName, HRESULT result) const;

        winrt::Windows::UI::Xaml::FrameworkElement _layout{ nullptr };
        HWND _parentWindow{};
        winrt::hstring _url;
        CoreReadyCallback _coreReady;
        bool _closed{};
    };
}
