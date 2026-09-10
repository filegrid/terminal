// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "TerminalPage.h"
#include "WorkspaceNativeHwndWebViewHost.h"

namespace winrt::TerminalApp::implementation
{
    namespace
    {
        std::pair<Windows::UI::Xaml::UIElement, std::shared_ptr<WorkspaceNativeHwndWebViewHost>> _CreateWorkspaceCommandWebView(const winrt::hstring& url, const HWND parentWindow)
        {
            auto layout = Windows::UI::Xaml::Controls::Grid{};
            layout.Background(Windows::UI::Xaml::Media::SolidColorBrush{ Windows::UI::Colors::Transparent() });
            auto nativeWebView = WorkspaceNativeHwndWebViewHost::Create(layout, parentWindow, url);
            return { layout, std::move(nativeWebView) };
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
                tab->SetTerminalContentWebView(winrt::hstring{ commands.front().WebUrl }, _hostingHwnd.value_or(nullptr));
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
            std::vector<std::shared_ptr<WorkspaceNativeHwndWebViewHost>> nativeWebViews;
            panes.reserve(launches.size());
            roots.reserve(launches.size());
            titles.reserve(launches.size());
            icons.reserve(launches.size());
            nativeWebViews.reserve(launches.size());
            for (size_t commandIndex = 0; commandIndex < launches.size(); ++commandIndex)
            {
                const auto& launch = launches[commandIndex];
                const auto& command = commands[commandIndex];
                if (command.WindowType == WorkspaceNodeCommand::Type::WebView)
                {
                    panes.emplace_back(nullptr);
                    auto [root, nativeWebView] = _CreateWorkspaceCommandWebView(winrt::hstring{ command.WebUrl }, _hostingHwnd.value_or(nullptr));
                    roots.emplace_back(std::move(root));
                    nativeWebViews.emplace_back(std::move(nativeWebView));
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
                    nativeWebViews.emplace_back(nullptr);
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
                nativeWebViews.emplace_back(nullptr);
            }
            if (roots.size() > 1)
            {
                const auto placement = node.MultiWindowPreference.TabPlacement;
                const auto iconButtons = placement != WorkspaceTabPlacement::TopLeft;
                tab->SetTerminalContentTabHost(std::move(panes),
                                               std::move(roots),
                                               std::move(titles),
                                               std::move(icons),
                                               std::move(nativeWebViews),
                                               iconButtons,
                                               placement == WorkspaceTabPlacement::BottomRight);
            }
        }
    }
}
