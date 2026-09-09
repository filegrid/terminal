// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "TerminalPage.h"
#include <winrt/Microsoft.Web.WebView2.Core.h>
#include "../../../../src/core/chat/WorkspaceDiagnosticLog.h"

namespace winrt::TerminalApp::implementation
{
    namespace
    {
        Windows::UI::Xaml::UIElement _CreateWorkspaceCommandWebView(const winrt::hstring& url)
        {
            auto webHost = Windows::UI::Xaml::Controls::Grid{};
            auto webView = Microsoft::UI::Xaml::Controls::WebView2{};
            webView.HorizontalAlignment(Windows::UI::Xaml::HorizontalAlignment::Stretch);
            webView.VerticalAlignment(Windows::UI::Xaml::VerticalAlignment::Stretch);
            // Tab owns the outer command host. Preserve its focus target so
            // Tab::Focus can forward activation to this WebView instead of to
            // the hidden terminal pane that was used to create the node tab.
            webHost.Tag(webView);
            auto status = Windows::UI::Xaml::Controls::TextBlock{};
            status.Visibility(Windows::UI::Xaml::Visibility::Collapsed);
            status.HorizontalAlignment(Windows::UI::Xaml::HorizontalAlignment::Center);
            status.VerticalAlignment(Windows::UI::Xaml::VerticalAlignment::Center);
            webHost.Children().Append(webView);
            webHost.Children().Append(status);
            // This follows the workspace-manager WebView2 host: initialize
            // asynchronously, navigate only after CoreWebView2 is ready, and
            // retain a visible failure status instead of a blank command tab.
            webView.CoreWebView2Initialized([webView, status, url](auto&&, const auto& args) {
                if (SUCCEEDED(args.Exception()))
                {
                    const auto core = webView.CoreWebView2();
                    core.NewWindowRequested([](auto&&, const auto& newWindowArgs) {
                        Json::Value payload{ Json::objectValue };
                        payload["userInitiated"] = newWindowArgs.IsUserInitiated();
                        terminal::workspacechat::AddDiagnosticTextFields(payload, "uri", newWindowArgs.Uri().c_str());
                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_webview_new_window_requested", payload);
                    });
                    core.NavigationStarting([](auto&&, const auto& navigationArgs) {
                        Json::Value payload{ Json::objectValue };
                        terminal::workspacechat::AddDiagnosticTextFields(payload, "uri", navigationArgs.Uri().c_str());
                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_webview_navigation_starting", payload);
                    });
                    core.ProcessFailed([](auto&&, const auto& processArgs) {
                        Json::Value payload{ Json::objectValue };
                        payload["kind"] = static_cast<int>(processArgs.ProcessFailedKind());
                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_webview_process_failed", payload);
                    });
                    core.Navigate(url);
                }
                else
                {
                    status.Visibility(Windows::UI::Xaml::Visibility::Collapsed);
                }
            });
            webView.Loaded([webView, status](auto&&, auto&&) -> winrt::fire_and_forget {
                try
                {
                    co_await webView.EnsureCoreWebView2Async();
                }
                catch (const winrt::hresult_error&)
                {
                    status.Visibility(Windows::UI::Xaml::Visibility::Collapsed);
                }
            });
            webView.NavigationCompleted([status](auto&&, const auto& args) {
                Json::Value payload{ Json::objectValue };
                payload["success"] = args.IsSuccess();
                payload["webErrorStatus"] = static_cast<int>(args.WebErrorStatus());
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_webview_navigation_completed", payload);
                if (args.IsSuccess())
                {
                    status.Visibility(Windows::UI::Xaml::Visibility::Collapsed);
                }
                else
                {
                    status.Visibility(Windows::UI::Xaml::Visibility::Collapsed);
                }
            });
            return webHost;
        }
    }

    void TerminalPage::ConfigureTerminalContentWrapper(const Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace)
    {
        _ConfigureTerminalContentWrapper(workspace);
    }

    void TerminalPage::_ConfigureTerminalContentWrapper(const Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace)
    {
        _terminalContentWorkspace = workspace;

        // Wrapper is below a first-level node Tab. Resolve the runtime node
        // id and consume only that node's Commands; commands never enter
        // TerminalPage::_tabs.
        using namespace Microsoft::Terminal::Settings::Model::implementation;
        WorkspaceManager manager;
        for (const auto& publicTab : _tabs)
        {
            const auto tab = _GetTabImpl(publicTab);
            if (!tab || !tab->IsWorkspaceNodeTab())
            {
                continue;
            }
            const auto nodeId = _ResolveLiveCurrentWorkspaceNodeId(tab);
            const auto nodeIndex = FindWorkspaceNodeIndexById(workspace, nodeId.c_str());
            if (!nodeIndex)
            {
                continue;
            }
            const auto& node = workspace.Nodes.at(*nodeIndex);
            const auto commands = node.Commands.empty() ?
                                      std::vector<WorkspaceNodeCommand>{ WorkspaceNodeCommand{ node.Id + L":legacy-command", node.Icon, node.Name, node.StartupAction } } :
                                      node.Commands;
            if (commands.size() == 1 && commands.front().WindowType == WorkspaceNodeCommand::Type::WebView)
            {
                tab->SetTerminalContentWebView(winrt::hstring{ commands.front().WebUrl });
                continue;
            }
            if (node.MultiWindowPreference.DisplayMode != WorkspaceWindowDisplayMode::Tab)
            {
                continue;
            }
            const auto launches = manager.BuildNodeCommandLaunches(workspace, *nodeIndex, _settings);
            if (launches.size() < 2)
            {
                continue;
            }

            std::vector<std::shared_ptr<Pane>> panes;
            std::vector<Windows::UI::Xaml::UIElement> roots;
            std::vector<winrt::hstring> titles;
            std::vector<winrt::hstring> icons;
            panes.reserve(launches.size());
            roots.reserve(launches.size());
            titles.reserve(launches.size());
            icons.reserve(launches.size());
            for (size_t commandIndex = 0; commandIndex < launches.size(); ++commandIndex)
            {
                const auto& launch = launches[commandIndex];
                const auto& command = commands[commandIndex];
                if (command.WindowType == WorkspaceNodeCommand::Type::WebView)
                {
                    panes.emplace_back(nullptr);
                    roots.emplace_back(_CreateWorkspaceCommandWebView(winrt::hstring{ command.WebUrl }));
                    titles.emplace_back(!command.Name.empty() ? winrt::hstring{ command.Name } : winrt::hstring{ L"WebView" });
                    icons.emplace_back(!command.Icon.empty() ? command.Icon : node.Icon);
                    continue;
                }
                if (commandIndex == 0)
                {
                    auto pane = tab->GetRootPane();
                    panes.emplace_back(pane);
                    roots.emplace_back(pane->GetRootElement());
                    titles.emplace_back(launch.TerminalArgs.TabTitle());
                    icons.emplace_back(!command.Icon.empty() ? command.Icon : node.Icon);
                    continue;
                }
                // Command panes belong to the current first-level node Tab.
                // They must not consume the pending registration queue, which
                // is reserved for the first-level workspace nodes.
                auto pane = _MakeTerminalPane(launch.TerminalArgs,
                                               nullptr,
                                               nullptr,
                                               false,
                                               launch.TerminalArgs.StartingDirectory());
                if (!pane)
                {
                    continue;
                }
                if (!launch.StartupInput.empty())
                {
                    if (const auto control = pane->GetTerminalControl())
                    {
                        // Each command window owns its startup input. Do not
                        // route it through a workspace-wide queue or another
                        // pane's lifecycle.
                        const auto sent = std::make_shared<bool>(false);
                        const auto sendWhenConnected = [weakControl{ winrt::make_weak(control) }, input{ launch.StartupInput }, sent](const auto& sender, const auto&) {
                            if (*sent)
                            {
                                return;
                            }
                            const auto coreState = sender.template try_as<winrt::Microsoft::Terminal::Control::ICoreState>();
                            using winrt::Microsoft::Terminal::TerminalConnection::ConnectionState;
                            if (!coreState || coreState.ConnectionState() < ConnectionState::Connected || coreState.ConnectionState() >= ConnectionState::Closed)
                            {
                                return;
                            }
                            if (const auto strongControl = weakControl.get())
                            {
                                strongControl.SendInput(winrt::hstring{ input });
                                *sent = true;
                            }
                        };
                        control.ConnectionStateChanged(sendWhenConnected);
                        sendWhenConnected(control, nullptr);
                    }
                }
                panes.emplace_back(std::move(pane));
                roots.emplace_back(panes.back()->GetRootElement());
                titles.emplace_back(launch.TerminalArgs.TabTitle());
                icons.emplace_back(!command.Icon.empty() ? command.Icon : node.Icon);
            }
            if (roots.size() > 1)
            {
                const auto placement = node.MultiWindowPreference.TabPlacement;
                const auto iconButtons = placement != WorkspaceTabPlacement::TopLeft;
                tab->SetTerminalContentTabHost(std::move(panes),
                                               std::move(roots),
                                               std::move(titles),
                                               std::move(icons),
                                               iconButtons,
                                               placement == WorkspaceTabPlacement::BottomRight);
            }
        }
    }
}
