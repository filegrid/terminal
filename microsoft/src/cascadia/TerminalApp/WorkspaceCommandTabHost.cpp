// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "TerminalPage.h"

namespace winrt::TerminalApp::implementation
{
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
            std::vector<winrt::hstring> titles;
            std::vector<winrt::hstring> icons;
            panes.reserve(launches.size());
            titles.reserve(launches.size());
            icons.reserve(launches.size());
            const auto commands = node.Commands.empty() ?
                                      std::vector<WorkspaceNodeCommand>{ WorkspaceNodeCommand{ node.Id + L":legacy-command", node.Icon, node.Name, node.StartupAction } } :
                                      node.Commands;
            panes.emplace_back(tab->GetRootPane());
            titles.emplace_back(launches.front().TerminalArgs.TabTitle());
            icons.emplace_back(!commands.front().Icon.empty() ? commands.front().Icon : node.Icon);
            for (size_t commandIndex = 1; commandIndex < launches.size(); ++commandIndex)
            {
                const auto& launch = launches[commandIndex];
                auto pane = _MakeTerminalPane(launch.TerminalArgs);
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
                titles.emplace_back(launch.TerminalArgs.TabTitle());
                const auto& command = commands[commandIndex];
                icons.emplace_back(!command.Icon.empty() ? command.Icon : node.Icon);
            }
            if (panes.size() > 1)
            {
                const auto placement = node.MultiWindowPreference.TabPlacement;
                const auto iconButtons = placement != WorkspaceTabPlacement::TopLeft;
                tab->SetTerminalContentTabHost(std::move(panes),
                                               std::move(titles),
                                               std::move(icons),
                                               iconButtons,
                                               placement == WorkspaceTabPlacement::BottomRight);
            }
        }
    }
}
