// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "../../../../src/core/chat/WorkspaceDiagnosticLog.h"

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include "../../../packages/Microsoft.Web.WebView2.1.0.1661.34/build/native/include/WebView2.h"
#include <wrl.h>

#include <filesystem>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::TerminalApp::implementation
{
    namespace
    {
        using ::Microsoft::WRL::Callback;
        using ::Microsoft::WRL::ComPtr;

        constexpr std::array<std::wstring_view, 3> WebViewDemoUrls{
            L"https://www.qq.com",
            L"http://127.0.0.1:8080/?folder=/home/coder/project",
            L"http://localhost:18080/"
        };

        std::wstring _NativeDemoUserDataDirectory()
        {
            std::wstring root(32768, L'\0');
            const auto length = GetEnvironmentVariableW(L"WT_PORTABLE_ROOT", root.data(), gsl::narrow_cast<DWORD>(root.size()));
            if (length > 0 && length < root.size())
            {
                root.resize(length);
                return root + L"\\webview\\native-hwnd-demo";
            }
            return std::filesystem::temp_directory_path().wstring() + L"\\WindowsTerminalNativeHwndWebViewDemo";
        }

        struct NativeHwndWebViewDemo final : std::enable_shared_from_this<NativeHwndWebViewDemo>
        {
            static void Show(const size_t urlIndex)
            {
                static std::vector<std::shared_ptr<NativeHwndWebViewDemo>> instances;
                auto instance = std::shared_ptr<NativeHwndWebViewDemo>{ new NativeHwndWebViewDemo{ urlIndex } };
                instances.emplace_back(instance);
                instance->_CreateWindow();
            }

            void _CreateWindow()
            {
                static std::once_flag classRegistered;
                std::call_once(classRegistered, [] {
                    WNDCLASSEXW windowClass{ sizeof(windowClass) };
                    windowClass.lpfnWndProc = _WindowProc;
                    windowClass.hInstance = GetModuleHandleW(nullptr);
                    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
                    windowClass.lpszClassName = L"WindowsTerminalNativeWebViewHostDemo";
                    RegisterClassExW(&windowClass);
                });
                _window = CreateWindowExW(0,
                                          L"WindowsTerminalNativeWebViewHostDemo",
                                          (std::wstring{ L"Native WebView2 Host Comparison — " } + std::wstring{ WebViewDemoUrls[_urlIndex] }).c_str(),
                                          WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                          CW_USEDEFAULT,
                                          CW_USEDEFAULT,
                                          1400,
                                          900,
                                          nullptr,
                                          nullptr,
                                          GetModuleHandleW(nullptr),
                                          this);
            }

        private:
            explicit NativeHwndWebViewDemo(const size_t urlIndex) :
                _urlIndex{ urlIndex }
            {
            }

            static LRESULT CALLBACK _WindowProc(const HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
            {
                if (message == WM_NCCREATE)
                {
                    const auto create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
                    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
                }
                const auto self = reinterpret_cast<NativeHwndWebViewDemo*>(GetWindowLongPtrW(window, GWLP_USERDATA));
                switch (message)
                {
                case WM_CREATE:
                    if (self)
                    {
                        self->_window = window;
                        self->_CreateChildWindows();
                        self->_InitializeWebViews();
                    }
                    return 0;
                case WM_SIZE:
                    if (self)
                    {
                        self->_Resize();
                    }
                    return 0;
                case WM_DESTROY:
                    if (self)
                    {
                        self->_Close();
                    }
                    return 0;
                default:
                    return DefWindowProcW(window, message, wParam, lParam);
                }
            }

            void _CreateChildWindows()
            {
                for (size_t index = 0; index < _hosts.size(); ++index)
                {
                    _hosts[index] = CreateWindowExW(WS_EX_CLIENTEDGE,
                                                     L"STATIC",
                                                     nullptr,
                                                     WS_CHILD | WS_VISIBLE,
                                                     0,
                                                     0,
                                                     1,
                                                     1,
                                                     _window,
                                                     nullptr,
                                                     GetModuleHandleW(nullptr),
                                                     nullptr);
                }
                _Resize();
            }

            void _InitializeWebViews()
            {
                const auto userDataDirectory = _NativeDemoUserDataDirectory();
                std::error_code error;
                std::filesystem::create_directories(userDataDirectory, error);
                Json::Value payload{ Json::objectValue };
                terminal::workspacechat::AddDiagnosticTextFields(payload, "userDataDirectory", userDataDirectory);
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_hwnd_demo_create_requested", payload);
                const auto result = CreateCoreWebView2EnvironmentWithOptions(
                    nullptr,
                    userDataDirectory.c_str(),
                    nullptr,
                    Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                        [self = shared_from_this()](const HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                            if (FAILED(result) || !environment)
                            {
                                self->_LogFailure(L"workspace_native_hwnd_demo_environment_failed", result, L"");
                                return S_OK;
                            }
                            self->_environment = environment;
                            self->_CreateController();
                            return S_OK;
                        }).Get());
                if (FAILED(result))
                {
                    _LogFailure(L"workspace_native_hwnd_demo_environment_request_failed", result, L"");
                }
            }

            void _CreateController()
            {
                const auto result = _environment->CreateCoreWebView2Controller(
                    _hosts[0],
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [self = shared_from_this()](const HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            const auto urlIndex = self->_urlIndex;
                            const auto url = WebViewDemoUrls[urlIndex];
                            if (FAILED(result) || !controller)
                            {
                                self->_LogFailure(L"workspace_native_hwnd_demo_controller_failed", result, url);
                                return S_OK;
                            }
                            self->_controllers[0] = controller;
                            if (FAILED(controller->get_CoreWebView2(&self->_cores[0])))
                            {
                                self->_LogFailure(L"workspace_native_hwnd_demo_core_failed", E_FAIL, url);
                                return S_OK;
                            }
                            self->_AttachDiagnostics(urlIndex);
                            EventRegistrationToken navigationToken{};
                            std::ignore = self->_cores[0]->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [self, urlIndex](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                        BOOL success{};
                                        COREWEBVIEW2_WEB_ERROR_STATUS status{};
                                        if (args)
                                        {
                                            std::ignore = args->get_IsSuccess(&success);
                                            std::ignore = args->get_WebErrorStatus(&status);
                                        }
                                        Json::Value payload{ Json::objectValue };
                                        payload["urlIndex"] = gsl::narrow_cast<int>(urlIndex);
                                        payload["success"] = success == TRUE;
                                        payload["webErrorStatus"] = static_cast<int>(status);
                                        terminal::workspacechat::AddDiagnosticTextFields(payload, "url", WebViewDemoUrls[urlIndex]);
                                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_hwnd_demo_navigation_completed", payload);
                                        return S_OK;
                                    }).Get(),
                                &navigationToken);
                            self->_ResizeOne(0);
                            self->_cores[0]->Navigate(url.data());
                            Json::Value payload{ Json::objectValue };
                            terminal::workspacechat::AddDiagnosticTextFields(payload, "url", url);
                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_hwnd_demo_ready", payload);
                            return S_OK;
                        }).Get());
                if (FAILED(result))
                {
                    _LogFailure(L"workspace_native_hwnd_demo_controller_request_failed", result, WebViewDemoUrls[_urlIndex]);
                }
            }

            void _AttachDiagnostics(const size_t urlIndex)
            {
                auto& core = _cores[0];
                EventRegistrationToken token{};
                std::ignore = core->add_NavigationStarting(
                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                        [urlIndex](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                            LPWSTR uri{};
                            BOOL redirected{};
                            BOOL userInitiated{};
                            BOOL cancelled{};
                            UINT64 navigationId{};
                            if (args)
                            {
                                std::ignore = args->get_Uri(&uri);
                                std::ignore = args->get_IsRedirected(&redirected);
                                std::ignore = args->get_IsUserInitiated(&userInitiated);
                                std::ignore = args->get_Cancel(&cancelled);
                                std::ignore = args->get_NavigationId(&navigationId);
                            }
                            Json::Value payload{ Json::objectValue };
                            payload["urlIndex"] = gsl::narrow_cast<int>(urlIndex);
                            payload["navigationId"] = Json::UInt64{ navigationId };
                            payload["isRedirected"] = redirected == TRUE;
                            payload["isUserInitiated"] = userInitiated == TRUE;
                            payload["isCancelled"] = cancelled == TRUE;
                            terminal::workspacechat::AddDiagnosticTextFields(payload, "uri", uri ? uri : L"");
                            if (uri)
                            {
                                CoTaskMemFree(uri);
                            }
                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_hwnd_demo_navigation_starting", payload);
                            return S_OK;
                        }).Get(),
                    &token);
                std::ignore = core->add_SourceChanged(
                    Callback<ICoreWebView2SourceChangedEventHandler>(
                        [urlIndex](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args) -> HRESULT {
                            LPWSTR source{};
                            BOOL isNewDocument{};
                            if (sender)
                            {
                                std::ignore = sender->get_Source(&source);
                            }
                            if (args)
                            {
                                std::ignore = args->get_IsNewDocument(&isNewDocument);
                            }
                            Json::Value payload{ Json::objectValue };
                            payload["urlIndex"] = gsl::narrow_cast<int>(urlIndex);
                            payload["isNewDocument"] = isNewDocument == TRUE;
                            terminal::workspacechat::AddDiagnosticTextFields(payload, "source", source ? source : L"");
                            if (source)
                            {
                                CoTaskMemFree(source);
                            }
                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_hwnd_demo_source_changed", payload);
                            return S_OK;
                        }).Get(),
                    &token);
                std::ignore = core->add_NewWindowRequested(
                    Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                        [urlIndex](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                            LPWSTR uri{};
                            BOOL userInitiated{};
                            BOOL handled{};
                            if (args)
                            {
                                std::ignore = args->get_Uri(&uri);
                                std::ignore = args->get_IsUserInitiated(&userInitiated);
                                std::ignore = args->get_Handled(&handled);
                            }
                            Json::Value payload{ Json::objectValue };
                            payload["urlIndex"] = gsl::narrow_cast<int>(urlIndex);
                            payload["isUserInitiated"] = userInitiated == TRUE;
                            payload["isHandledBeforeCallback"] = handled == TRUE;
                            terminal::workspacechat::AddDiagnosticTextFields(payload, "uri", uri ? uri : L"");
                            if (uri)
                            {
                                CoTaskMemFree(uri);
                            }
                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_hwnd_demo_new_window_requested", payload);
                            return S_OK;
                        }).Get(),
                    &token);
                std::ignore = core->add_ProcessFailed(
                    Callback<ICoreWebView2ProcessFailedEventHandler>(
                        [urlIndex](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                            COREWEBVIEW2_PROCESS_FAILED_KIND kind{};
                            COREWEBVIEW2_PROCESS_FAILED_REASON reason{};
                            int exitCode{};
                            LPWSTR processDescription{};
                            bool hasDetails{};
                            if (args)
                            {
                                std::ignore = args->get_ProcessFailedKind(&kind);
                                ComPtr<ICoreWebView2ProcessFailedEventArgs2> details;
                                if (SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&details))))
                                {
                                    hasDetails = true;
                                    std::ignore = details->get_Reason(&reason);
                                    std::ignore = details->get_ExitCode(&exitCode);
                                    std::ignore = details->get_ProcessDescription(&processDescription);
                                }
                            }
                            Json::Value payload{ Json::objectValue };
                            payload["urlIndex"] = gsl::narrow_cast<int>(urlIndex);
                            payload["processFailedKind"] = static_cast<int>(kind);
                            payload["hasProcessFailureDetails"] = hasDetails;
                            if (hasDetails)
                            {
                                payload["processFailedReason"] = static_cast<int>(reason);
                                payload["processExitCode"] = exitCode;
                                terminal::workspacechat::AddDiagnosticTextFields(payload, "processDescription", processDescription ? processDescription : L"");
                            }
                            if (processDescription)
                            {
                                CoTaskMemFree(processDescription);
                            }
                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_hwnd_demo_process_failed", payload);
                            return S_OK;
                        }).Get(),
                    &token);
            }

            void _Resize()
            {
                RECT client{};
                GetClientRect(_window, &client);
                const auto width = std::max<LONG>(1, (client.right - client.left) / gsl::narrow_cast<LONG>(_hosts.size()));
                const auto height = std::max<LONG>(1, client.bottom - client.top);
                for (size_t index = 0; index < _hosts.size(); ++index)
                {
                    SetWindowPos(_hosts[index], nullptr, gsl::narrow_cast<int>(index) * width, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
                    _ResizeOne(index);
                }
            }

            void _ResizeOne(const size_t index)
            {
                if (!_controllers[index] || !_hosts[index])
                {
                    return;
                }
                RECT bounds{};
                GetClientRect(_hosts[index], &bounds);
                std::ignore = _controllers[index]->put_Bounds(bounds);
            }

            void _Close() noexcept
            {
                for (auto& controller : _controllers)
                {
                    if (controller)
                    {
                        controller->Close();
                    }
                    controller.Reset();
                }
                for (auto& core : _cores)
                {
                    core.Reset();
                }
                _environment.Reset();
            }

            void _LogFailure(const std::wstring_view eventName, const HRESULT result, const std::wstring_view url) const
            {
                Json::Value payload{ Json::objectValue };
                payload["hresult"] = static_cast<int>(result);
                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", url);
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(eventName, payload);
            }

            HWND _window{};
            size_t _urlIndex{};
            std::array<HWND, 1> _hosts{};
            ComPtr<ICoreWebView2Environment> _environment;
            std::array<ComPtr<ICoreWebView2Controller>, 1> _controllers;
            std::array<ComPtr<ICoreWebView2>, 1> _cores;
        };
    }

    UIElement TerminalPage::_BuildWorkspaceXamlWebViewHostDemo(const size_t urlIndex)
    {
        auto root = Grid{};
        auto webView = Microsoft::UI::Xaml::Controls::WebView2{};
        webView.CoreWebView2Initialized([webView, urlIndex](auto&&, const auto& args) {
                Json::Value payload{ Json::objectValue };
                payload["initializationHresult"] = Json::Int{ args.Exception() };
                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", WebViewDemoUrls[urlIndex]);
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_xaml_demo_initialized", payload);
                if (SUCCEEDED(args.Exception()))
                {
                    webView.CoreWebView2().Navigate(WebViewDemoUrls[urlIndex].data());
                }
            });
        webView.Loaded([webView, urlIndex](auto&&, auto&&) -> winrt::fire_and_forget {
                try
                {
                    co_await webView.EnsureCoreWebView2Async();
                }
                catch (const winrt::hresult_error& error)
                {
                    Json::Value payload{ Json::objectValue };
                    terminal::workspacechat::AddDiagnosticTextFields(payload, "url", WebViewDemoUrls[urlIndex]);
                    terminal::workspacechat::AppendExceptionDiagnostic(payload, error);
                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_xaml_demo_ensure_exception", payload);
                }
        });
        root.Children().Append(webView);
        return root;
    }

    void TerminalPage::_ShowWorkspaceNativeWebViewHostDemo(const size_t urlIndex)
    {
        NativeHwndWebViewDemo::Show(urlIndex);
    }
}
