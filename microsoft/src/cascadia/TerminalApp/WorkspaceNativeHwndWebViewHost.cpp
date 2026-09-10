// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "WorkspaceNativeHwndWebViewHost.h"
#include "../../../../src/core/chat/WorkspaceDiagnosticLog.h"
#include "../../../packages/Microsoft.Web.WebView2.1.0.1661.34/build/native/include/WebView2.h"

#include <wrl.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

using namespace winrt;
using namespace ::Microsoft::WRL;

namespace winrt::TerminalApp::implementation
{
    namespace
    {
        struct HostState
        {
            ComPtr<ICoreWebView2Environment> Environment;
            ComPtr<ICoreWebView2CompositionController> CompositionController;
            ComPtr<ICoreWebView2Controller> Controller;
            ComPtr<ICoreWebView2> Core;
            Windows::UI::Composition::ContainerVisual Visual{ nullptr };
        };

        // This is a raw WebView2 COM composition controller. The XAML Grid is
        // only its visual target and pointer source, never a XAML WebView2.
        std::mutex s_hostsMutex;
        std::unordered_map<WorkspaceNativeHwndWebViewHost*, HostState> s_hosts;
    }

    WorkspaceNativeHwndWebViewHost::WorkspaceNativeHwndWebViewHost(const Windows::UI::Xaml::FrameworkElement layout,
                                                                     const HWND parentWindow,
                                                                     hstring url,
                                                                     CoreReadyCallback coreReady) :
        _layout(layout),
        _parentWindow(parentWindow),
        _url(std::move(url)),
        _coreReady(std::move(coreReady))
    {
    }

    std::shared_ptr<WorkspaceNativeHwndWebViewHost> WorkspaceNativeHwndWebViewHost::Create(const Windows::UI::Xaml::FrameworkElement layout,
                                                                                              const HWND parentWindow,
                                                                                              hstring url,
                                                                                              CoreReadyCallback coreReady)
    {
        auto result = std::shared_ptr<WorkspaceNativeHwndWebViewHost>{ new WorkspaceNativeHwndWebViewHost{ layout, parentWindow, std::move(url), std::move(coreReady) } };
        result->_Initialize();
        return result;
    }

    WorkspaceNativeHwndWebViewHost::~WorkspaceNativeHwndWebViewHost()
    {
        Close();
    }

    HRESULT WorkspaceNativeHwndWebViewHost::MapVirtualHostToFolder(ICoreWebView2* const core,
                                                                    const std::wstring_view hostName,
                                                                    const std::wstring_view folder) noexcept
    {
        if (!core || hostName.empty() || folder.empty())
        {
            return E_INVALIDARG;
        }
        ComPtr<ICoreWebView2_3> core3;
        const auto queryResult = core->QueryInterface(IID_PPV_ARGS(&core3));
        if (FAILED(queryResult))
        {
            return queryResult;
        }
        return core3->SetVirtualHostNameToFolderMapping(hostName.data(), folder.data(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
    }

    void WorkspaceNativeHwndWebViewHost::_Initialize()
    {
        if (!_parentWindow || !_layout)
        {
            _LogFailure(L"workspace_native_webview_missing_parent", E_HANDLE);
            return;
        }
        auto visual = Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(_layout).Compositor().CreateContainerVisual();
        Windows::UI::Xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(_layout, visual);
        {
            std::scoped_lock lock{ s_hostsMutex };
            s_hosts[this].Visual = visual;
        }
        _layout.SizeChanged([weak = weak_from_this()](auto&&, auto&&) {
            if (const auto self = weak.lock())
            {
                self->_Resize();
            }
        });
        _layout.Loaded([weak = weak_from_this()](auto&&, auto&&) {
            if (const auto self = weak.lock())
            {
                self->Show();
            }
        });
        _layout.Unloaded([weak = weak_from_this()](auto&&, auto&&) {
            if (const auto self = weak.lock())
            {
                self->Hide();
            }
        });
        _layout.PointerPressed([weak = weak_from_this()](auto&&, const auto& args) {
            if (const auto self = weak.lock())
            {
                self->_SendMouse(COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN, args);
                // Prevent TerminalPage from immediately moving focus back to
                // the terminal pane after this press reaches WebView2.
                args.Handled(true);
            }
        });
        _layout.PointerReleased([weak = weak_from_this()](auto&&, const auto& args) {
            if (const auto self = weak.lock())
            {
                self->_SendMouse(COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP, args);
                args.Handled(true);
            }
        });
        _layout.PointerMoved([weak = weak_from_this()](auto&&, const auto& args) {
            if (const auto self = weak.lock())
            {
                self->_SendMouse(COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE, args);
                args.Handled(true);
            }
        });

        Json::Value payload{ Json::objectValue };
        terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
        payload["parentWindow"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(reinterpret_cast<uintptr_t>(_parentWindow)) };
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_composition_create_requested", payload);
        const auto request = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [weak = weak_from_this()](const HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                    const auto self = weak.lock();
                    if (!self || self->_closed)
                    {
                        return S_OK;
                    }
                    if (FAILED(result) || !environment)
                    {
                        self->_LogFailure(L"workspace_native_webview_environment_failed", result);
                        return S_OK;
                    }
                    ComPtr<ICoreWebView2Environment3> environment3;
                    if (const auto query = environment->QueryInterface(IID_PPV_ARGS(&environment3)); FAILED(query))
                    {
                        self->_LogFailure(L"workspace_native_webview_composition_unavailable", query);
                        return S_OK;
                    }
                    {
                        std::scoped_lock lock{ s_hostsMutex };
                        s_hosts[self.get()].Environment = environment;
                    }
                    return environment3->CreateCoreWebView2CompositionController(
                        self->_parentWindow,
                        Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
                            [weak](const HRESULT controllerResult, ICoreWebView2CompositionController* compositionController) -> HRESULT {
                                const auto host = weak.lock();
                                if (!host || host->_closed)
                                {
                                    return S_OK;
                                }
                                if (FAILED(controllerResult) || !compositionController)
                                {
                                    host->_LogFailure(L"workspace_native_webview_composition_controller_failed", controllerResult);
                                    return S_OK;
                                }
                                ComPtr<ICoreWebView2Controller> controller;
                                ComPtr<ICoreWebView2> core;
                                if (FAILED(compositionController->QueryInterface(IID_PPV_ARGS(&controller))) || FAILED(controller->get_CoreWebView2(&core)))
                                {
                                    host->_LogFailure(L"workspace_native_webview_core_failed", E_NOINTERFACE);
                                    return S_OK;
                                }
                                Windows::UI::Composition::ContainerVisual visual{ nullptr };
                                {
                                    std::scoped_lock lock{ s_hostsMutex };
                                    auto& state = s_hosts[host.get()];
                                    state.CompositionController = compositionController;
                                    state.Controller = controller;
                                    state.Core = core;
                                    visual = state.Visual;
                                }
                                const auto target = compositionController->put_RootVisualTarget(reinterpret_cast<IUnknown*>(winrt::get_abi(visual)));
                                if (FAILED(target))
                                {
                                    host->_LogFailure(L"workspace_native_webview_visual_target_failed", target);
                                    return S_OK;
                                }
                                EventRegistrationToken token{};
                                std::ignore = controller->add_GotFocus(
                                    Callback<ICoreWebView2FocusChangedEventHandler>([weak](ICoreWebView2Controller*, IUnknown*) -> HRESULT {
                                        if (const auto activeHost = weak.lock())
                                        {
                                            Json::Value payload{ Json::objectValue };
                                            terminal::workspacechat::AddDiagnosticTextFields(payload, "url", activeHost->_url.c_str());
                                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_got_focus", payload);
                                        }
                                        return S_OK;
                                    }).Get(), &token);
                                std::ignore = controller->add_LostFocus(
                                    Callback<ICoreWebView2FocusChangedEventHandler>([weak](ICoreWebView2Controller*, IUnknown*) -> HRESULT {
                                        if (const auto activeHost = weak.lock())
                                        {
                                            Json::Value payload{ Json::objectValue };
                                            terminal::workspacechat::AddDiagnosticTextFields(payload, "url", activeHost->_url.c_str());
                                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_lost_focus", payload);
                                        }
                                        return S_OK;
                                    }).Get(), &token);
                                std::ignore = core->add_NavigationStarting(
                                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                                        [weak](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*) -> HRESULT {
                                            if (const auto activeHost = weak.lock())
                                            {
                                                Json::Value payload{ Json::objectValue };
                                                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", activeHost->_url.c_str());
                                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_navigation_starting", payload);
                                            }
                                            return S_OK;
                                        }).Get(), &token);
                                std::ignore = core->add_NavigationCompleted(
                                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                        [weak](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                            if (const auto activeHost = weak.lock())
                                            {
                                                BOOL succeeded{};
                                                COREWEBVIEW2_WEB_ERROR_STATUS error{};
                                                if (args)
                                                {
                                                    std::ignore = args->get_IsSuccess(&succeeded);
                                                    std::ignore = args->get_WebErrorStatus(&error);
                                                }
                                                Json::Value payload{ Json::objectValue };
                                                payload["success"] = succeeded != FALSE;
                                                payload["webErrorStatus"] = static_cast<int>(error);
                                                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", activeHost->_url.c_str());
                                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_navigation_completed", payload);
                                            }
                                            return S_OK;
                                        }).Get(), &token);
                                std::ignore = core->add_ProcessFailed(
                                    Callback<ICoreWebView2ProcessFailedEventHandler>(
                                        [weak](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                                            if (const auto activeHost = weak.lock())
                                            {
                                                COREWEBVIEW2_PROCESS_FAILED_KIND kind{};
                                                if (args)
                                                {
                                                    std::ignore = args->get_ProcessFailedKind(&kind);
                                                }
                                                Json::Value payload{ Json::objectValue };
                                                payload["kind"] = static_cast<int>(kind);
                                                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", activeHost->_url.c_str());
                                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_process_failed", payload);
                                            }
                                            return S_OK;
                                        }).Get(), &token);
                                std::ignore = controller->put_IsVisible(TRUE);
                                if (host->_coreReady)
                                {
                                    host->_coreReady(core.Get());
                                }
                                host->_Resize();
                                core->Navigate(host->_url.c_str());
                                return S_OK;
                            }).Get());
                }).Get());
        if (FAILED(request))
        {
            _LogFailure(L"workspace_native_webview_environment_request_failed", request);
        }
    }

    void WorkspaceNativeHwndWebViewHost::_Resize()
    {
        if (_closed || !_layout || !_layout.XamlRoot())
        {
            return;
        }
        const auto scale = _layout.XamlRoot().RasterizationScale();
        const auto width = std::max(0L, gsl::narrow_cast<LONG>(std::lround(_layout.ActualWidth() * scale)));
        const auto height = std::max(0L, gsl::narrow_cast<LONG>(std::lround(_layout.ActualHeight() * scale)));
        std::scoped_lock lock{ s_hostsMutex };
        if (const auto found = s_hosts.find(this); found != s_hosts.end())
        {
            if (found->second.Visual)
            {
                found->second.Visual.Size({ static_cast<float>(width), static_cast<float>(height) });
            }
            if (found->second.Controller)
            {
                std::ignore = found->second.Controller->put_Bounds(RECT{ 0, 0, width, height });
            }
        }
        Json::Value payload{ Json::objectValue };
        payload["width"] = static_cast<Json::Int>(width);
        payload["height"] = static_cast<Json::Int>(height);
        terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_resized", payload);
    }

    void WorkspaceNativeHwndWebViewHost::_SendMouse(const unsigned int kind, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& args)
    {
        ComPtr<ICoreWebView2CompositionController> compositionController;
        ComPtr<ICoreWebView2Controller> controller;
        {
            std::scoped_lock lock{ s_hostsMutex };
            if (const auto found = s_hosts.find(this); found != s_hosts.end())
            {
                compositionController = found->second.CompositionController;
                controller = found->second.Controller;
            }
        }
        if (!compositionController || !_layout || !_layout.XamlRoot())
        {
            return;
        }
        const auto point = args.GetCurrentPoint(_layout).Position();
        const auto scale = _layout.XamlRoot().RasterizationScale();
        const POINT physicalPoint{ gsl::narrow_cast<LONG>(std::lround(point.X * scale)), gsl::narrow_cast<LONG>(std::lround(point.Y * scale)) };
        const auto result = compositionController->SendMouseInput(static_cast<COREWEBVIEW2_MOUSE_EVENT_KIND>(kind), COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE, 0, physicalPoint);
        if (FAILED(result))
        {
            _LogFailure(L"workspace_native_webview_mouse_failed", result);
            return;
        }
        if (kind == COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN)
        {
            Json::Value payload{ Json::objectValue };
            payload["x"] = static_cast<Json::Int>(physicalPoint.x);
            payload["y"] = static_cast<Json::Int>(physicalPoint.y);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_mouse_down", payload);
            if (controller)
            {
                std::ignore = controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
        }
    }

    void WorkspaceNativeHwndWebViewHost::Show()
    {
        if (_closed)
        {
            return;
        }
        _Resize();
        std::scoped_lock lock{ s_hostsMutex };
        if (const auto found = s_hosts.find(this); found != s_hosts.end() && found->second.Controller)
        {
            std::ignore = found->second.Controller->put_IsVisible(TRUE);
        }
        Json::Value payload{ Json::objectValue };
        terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_shown", payload);
    }

    void WorkspaceNativeHwndWebViewHost::Hide()
    {
        if (_closed)
        {
            return;
        }
        std::scoped_lock lock{ s_hostsMutex };
        if (const auto found = s_hosts.find(this); found != s_hosts.end() && found->second.Controller)
        {
            std::ignore = found->second.Controller->put_IsVisible(FALSE);
        }
        Json::Value payload{ Json::objectValue };
        terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_hidden", payload);
    }

    void WorkspaceNativeHwndWebViewHost::Focus()
    {
        ComPtr<ICoreWebView2Controller> controller;
        {
            std::scoped_lock lock{ s_hostsMutex };
            if (const auto found = s_hosts.find(this); found != s_hosts.end())
            {
                controller = found->second.Controller;
            }
        }
        if (controller)
        {
            const auto result = controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            if (FAILED(result))
            {
                _LogFailure(L"workspace_native_webview_focus_failed", result);
            }
        }
    }

    void WorkspaceNativeHwndWebViewHost::Close() noexcept
    {
        if (_closed)
        {
            return;
        }
        _closed = true;
        HostState state;
        {
            std::scoped_lock lock{ s_hostsMutex };
            if (const auto found = s_hosts.find(this); found != s_hosts.end())
            {
                state = std::move(found->second);
                s_hosts.erase(found);
            }
        }
        // Controller shutdown can synchronously dispatch callbacks. Do it
        // after releasing the state lock so the workspace close button stays responsive.
        if (state.CompositionController)
        {
            std::ignore = state.CompositionController->put_RootVisualTarget(nullptr);
        }
        if (state.Controller)
        {
            state.Controller->Close();
        }
        if (_layout && state.Visual)
        {
            Windows::UI::Xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(_layout, nullptr);
        }
        Json::Value payload{ Json::objectValue };
        terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_close_requested", payload);
    }

    void WorkspaceNativeHwndWebViewHost::_LogFailure(const std::wstring_view eventName, const HRESULT result) const
    {
        Json::Value payload{ Json::objectValue };
        payload["hresult"] = static_cast<int>(result);
        terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(eventName, payload);
    }
}
