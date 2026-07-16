    void TerminalPage::_ShowWorkspaceNameMenu()
    {
        if (!_tabRow)
        {
            return;
        }

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        const auto button = tabRowImpl->WorkspaceNameButton();
        if (_workspaceFlyout)
        {
            _workspaceFlyout.Hide();
            _workspaceFlyout = nullptr;
        }

        _workspaceFlyout = _CreateWorkspaceFlyout();
        _workspaceFlyout.ShowAt(button);
    }

    void TerminalPage::_BeginWorkspaceNameEdit()
    {
        if (!_tabRow || _CurrentWorkspaceLocked())
        {
            return;
        }

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        const auto button = tabRowImpl->WorkspaceNameButton();
        const auto editor = tabRowImpl->WorkspaceNameEditor();
        button.Visibility(WUX::Visibility::Collapsed);
        editor.Text(_currentWorkspaceId.empty() ? RS_(L"WorkspaceUnsavedName") : winrt::hstring{ _CurrentWorkspaceDisplayName() });
        editor.Visibility(WUX::Visibility::Visible);
        editor.Focus(FocusState::Programmatic);
        editor.SelectAll();
        _workspaceExtension->SetWorkspaceNamePressedEnter(false);
    }

    void TerminalPage::_CommitWorkspaceNameEdit()
    {
        if (!_tabRow)
        {
            return;
        }

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        const auto button = tabRowImpl->WorkspaceNameButton();
        const auto editor = tabRowImpl->WorkspaceNameEditor();
        const auto newName = editor.Text();
        editor.Visibility(WUX::Visibility::Collapsed);
        button.Visibility(WUX::Visibility::Visible);

        if (newName.empty())
        {
            return;
        }

        if (_currentWorkspaceId.empty())
        {
            _SaveCurrentWindowAsWorkspace(newName);
            return;
        }

        auto manager = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        auto& workspaces = manager.Workspaces();
        const auto it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == _currentWorkspaceId.c_str();
        });
        if (it == workspaces.end())
        {
            return;
        }

        it->Name = Microsoft::Terminal::Settings::Model::implementation::SanitizeWorkspaceDirectoryName(newName.c_str(), L"workspace");
        std::unordered_set<std::wstring> usedWorkspaceNames;
        for (auto& workspace : workspaces)
        {
            workspace.Name = _makeUniquePersistedName(workspace.Name, usedWorkspaceNames);
            workspace.Id = workspace.Name;
        }
        const auto resolvedWorkspaceName = it->Name;
        if (!manager.Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceRenameFailed"));
            return;
        }

        CurrentWorkspaceId(winrt::hstring{ resolvedWorkspaceName });
        _LoadWorkspaceEditorState();
        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();
        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }
    }

    void TerminalPage::_CancelWorkspaceNameEdit()
    {
        if (!_tabRow)
        {
            return;
        }

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        tabRowImpl->WorkspaceNameEditor().Visibility(WUX::Visibility::Collapsed);
        tabRowImpl->WorkspaceNameButton().Visibility(WUX::Visibility::Visible);
        _workspaceExtension->SetWorkspaceNamePressedEnter(false);
    }

    std::optional<NewTerminalArgs> TerminalPage::_BuildWorkspaceNodeArgs(const winrt::com_ptr<Tab>& tab) const
    {
        if (!tab)
        {
            return std::nullopt;
        }

        const auto actions = tab->BuildStartupActions(BuildStartupKind::Persist);
        if (actions.empty() || actions.front().Action() != ShortcutAction::NewTab)
        {
            return std::nullopt;
        }

        const auto newTabArgs = actions.front().Args().try_as<NewTabArgs>();
        if (!newTabArgs)
        {
            return std::nullopt;
        }

        const auto terminalArgs = newTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        if (!terminalArgs)
        {
            return std::nullopt;
        }

        return terminalArgs;
    }

    bool TerminalPage::_TryCaptureCurrentWorkspace(Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace) const
    {
        using Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode;

        std::vector<WorkspaceNode> capturedNodes;
        capturedNodes.reserve(_tabs.Size());

        const auto currentWorkspaceDefinition = [&]() -> std::optional<Workspace> {
            if (_currentWorkspaceId.empty())
            {
                return std::nullopt;
            }

            auto manager = WorkspaceManager::Load();
            if (const auto persistedWorkspace = manager.FindById(_currentWorkspaceId.c_str()))
            {
                return *persistedWorkspace;
            }

            return std::nullopt;
        }();

        uint32_t nodeIndex = 0;
        for (const auto& tab : _tabs)
        {
            if (const auto tabImpl = _GetTabImpl(tab))
            {
                const auto terminalArgs = _BuildWorkspaceNodeArgs(tabImpl);
                if (!terminalArgs)
                {
                    continue;
                }

                WorkspaceNode node;
                if (const auto existingNode = _ResolveCurrentWorkspaceNode(tabImpl))
                {
                    node = *existingNode;
                }
                else
                {
                    node.UseNodeNameAsTabTitle = true;
                    node.Name = tabImpl->Title().empty() ? terminalArgs->TabTitle().c_str() : tabImpl->Title().c_str();
                    if (node.Name.empty())
                    {
                        node.Name = RS_fmt(L"WorkspaceEditor_NodeGeneratedName", nodeIndex + 1).c_str();
                    }
                    node.Id = node.Name;
                }

                node.ProfileGuid = terminalArgs->Profile().empty() ? ::Microsoft::Console::Utils::GuidToString(_settings.GlobalSettings().DefaultProfile()) : terminalArgs->Profile().c_str();
                node.StartupDirectory = _ResolveWorkspaceNodeStartingDirectory(tabImpl, *terminalArgs);
                node.StartupAction = _ResolveWorkspaceNodeStartupAction(tabImpl, *terminalArgs);
                node.OperatingSystem = _ResolveWorkspaceNodeOperatingSystem(tabImpl, *terminalArgs);
                node.ShellType = _ResolveWorkspaceNodeShellType(tabImpl, *terminalArgs);
                node.ShowInputPanel = tabImpl->ShowWorkspaceInputPanel();

                if (const auto tabColor = tabImpl->GetTabColor())
                {
                    node.TabColor = _workspaceColorToString(*tabColor);
                }
                capturedNodes.emplace_back(std::move(node));
                ++nodeIndex;
            }
        }

        workspace.Nodes.clear();
        if (currentWorkspaceDefinition.has_value())
        {
            workspace = *currentWorkspaceDefinition;
            if (!ApplyVisibleWorkspaceNodeOrder(workspace, capturedNodes))
            {
                return false;
            }
        }
        else
        {
            workspace.Nodes = std::move(capturedNodes);
        }

        return !workspace.Nodes.empty();
    }

    std::optional<size_t> TerminalPage::_GetWorkspaceBackedTabNodeIndex(const winrt::com_ptr<Tab>& tab) const
    {
        if (!tab)
        {
            return std::nullopt;
        }

        const auto resolveWorkspace = [&]() -> std::optional<Workspace> {
            if (_currentWorkspaceId.empty())
            {
                return std::nullopt;
            }

            if (const auto selectedWorkspace = _SelectedWorkspaceForEditing();
                selectedWorkspace && selectedWorkspace->Id == _currentWorkspaceId.c_str())
            {
                return *selectedWorkspace;
            }

            auto manager = WorkspaceManager::Load();
            if (const auto persistedWorkspace = manager.FindById(_currentWorkspaceId.c_str()))
            {
                return *persistedWorkspace;
            }

            return std::nullopt;
        }();

        if (const auto state = _FindWorkspaceNodeRuntimeState(tab); state && resolveWorkspace.has_value())
        {
            if (const auto nodeIndex = _findWorkspaceNodeIndexById(*resolveWorkspace, state->WorkspaceNodeId))
            {
                return nodeIndex;
            }
        }

        size_t visibleNodeOrdinal = 0;
        for (const auto& candidate : _tabs)
        {
            if (const auto tabImpl = _GetTabImpl(candidate))
            {
                if (!_BuildWorkspaceNodeArgs(tabImpl))
                {
                    continue;
                }

                if (tabImpl.get() == tab.get())
                {
                    if (resolveWorkspace.has_value())
                    {
                        return _findWorkspaceVisibleNodeIndex(*resolveWorkspace, visibleNodeOrdinal);
                    }

                    return visibleNodeOrdinal;
                }

                ++visibleNodeOrdinal;
            }
        }

        return std::nullopt;
    }

    const TerminalPage::WorkspaceNodeRuntimeState* TerminalPage::_FindWorkspaceNodeRuntimeState(const TermControl& control) const
    {
        if (!control)
        {
            return nullptr;
        }

        return _workspaceExtension->FindWorkspaceNodeRuntimeState(control.ContentId());
    }

    const TerminalPage::WorkspaceNodeRuntimeState* TerminalPage::_FindWorkspaceNodeRuntimeState(const winrt::com_ptr<Tab>& tab) const
    {
        if (!tab)
        {
            return nullptr;
        }

        if (const auto activeControl = tab->GetActiveTerminalControl())
        {
            if (const auto state = _FindWorkspaceNodeRuntimeState(activeControl))
            {
                return state;
            }
        }

        if (const auto rootPane = tab->GetRootPane())
        {
            return rootPane->WalkTree([this](const auto& pane) -> const WorkspaceNodeRuntimeState* {
                if (const auto content = pane->GetContent().try_as<winrt::TerminalApp::TerminalPaneContent>())
                {
                    return _FindWorkspaceNodeRuntimeState(content.GetTermControl());
                }

                return nullptr;
            });
        }

        return nullptr;
    }

    std::optional<WorkspaceNode> TerminalPage::_ResolveCurrentWorkspaceNode(const winrt::com_ptr<Tab>& tab) const
    {
        if (_currentWorkspaceId.empty())
        {
            return std::nullopt;
        }

        const auto findNodeById = [&](std::wstring_view nodeId) -> std::optional<WorkspaceNode> {
            if (nodeId.empty())
            {
                return std::nullopt;
            }

            if (const auto workspace = _SelectedWorkspaceForEditing();
                workspace && workspace->Id == _currentWorkspaceId.c_str())
            {
                const auto it = std::find_if(workspace->Nodes.begin(), workspace->Nodes.end(), [&](const auto& node) {
                    return node.Id == nodeId;
                });
                if (it != workspace->Nodes.end())
                {
                    return *it;
                }
            }

            auto manager = WorkspaceManager::Load();
            if (const auto workspace = manager.FindById(_currentWorkspaceId.c_str()))
            {
                const auto it = std::find_if(workspace->Nodes.begin(), workspace->Nodes.end(), [&](const auto& node) {
                    return node.Id == nodeId;
                });
                if (it != workspace->Nodes.end())
                {
                    return *it;
                }
            }

            return std::nullopt;
        };

        if (const auto state = _FindWorkspaceNodeRuntimeState(tab))
        {
            if (const auto node = findNodeById(state->WorkspaceNodeId))
            {
                return node;
            }
        }

        const auto nodeIndex = _GetWorkspaceBackedTabNodeIndex(tab);
        if (!nodeIndex.has_value())
        {
            return std::nullopt;
        }

        if (const auto workspace = _SelectedWorkspaceForEditing();
            workspace && workspace->Id == _currentWorkspaceId.c_str() && *nodeIndex < workspace->Nodes.size())
        {
            return workspace->Nodes.at(*nodeIndex);
        }

        auto manager = WorkspaceManager::Load();
        if (const auto workspace = manager.FindById(_currentWorkspaceId.c_str());
            workspace && *nodeIndex < workspace->Nodes.size())
        {
            return workspace->Nodes.at(*nodeIndex);
        }

        return std::nullopt;
    }

    std::wstring TerminalPage::_ResolveLiveCurrentWorkspaceNodeId(const winrt::com_ptr<Tab>& tab) const
    {
        if (const auto state = _FindWorkspaceNodeRuntimeState(tab))
        {
            return state->WorkspaceNodeId;
        }

        if (const auto node = _ResolveCurrentWorkspaceNode(tab))
        {
            return node->Id;
        }

        return {};
    }

    winrt::com_ptr<Tab> TerminalPage::_GetWorkspaceBackedTabByNodeIndex(const size_t nodeIndex) const
    {
        if (const auto* workspace = _SelectedWorkspaceForEditing();
            workspace && workspace->Id == _currentWorkspaceId.c_str() && nodeIndex < workspace->Nodes.size())
        {
            if (const auto tab = _GetCurrentWorkspaceTabByNodeId(workspace->Nodes.at(nodeIndex).Id))
            {
                return tab;
            }
        }

        for (const auto& tab : _tabs)
        {
            if (const auto tabImpl = _GetTabImpl(tab))
            {
                if (const auto currentNodeIndex = _GetWorkspaceBackedTabNodeIndex(tabImpl);
                    currentNodeIndex.has_value() && currentNodeIndex.value() == nodeIndex)
                {
                    return tabImpl;
                }
            }
        }

        return nullptr;
    }

    winrt::com_ptr<Tab> TerminalPage::_GetCurrentWorkspaceTabByNodeId(const std::wstring_view nodeId) const
    {
        if (nodeId.empty())
        {
            return nullptr;
        }

        for (const auto& tab : _tabs)
        {
            if (const auto tabImpl = _GetTabImpl(tab))
            {
                if (_ResolveLiveCurrentWorkspaceNodeId(tabImpl) == nodeId)
                {
                    return tabImpl;
                }
            }
        }

        return nullptr;
    }

    void TerminalPage::_ApplyWorkspaceNodeInputVisibility(const size_t nodeIndex, const bool showInputPanel)
    {
        const auto* workspace = _SelectedWorkspaceForEditing();
        if (!workspace || workspace->Id != _currentWorkspaceId.c_str())
        {
            return;
        }

        if (const auto tabImpl = _GetWorkspaceBackedTabByNodeIndex(nodeIndex))
        {
            tabImpl->ShowWorkspaceInputPanel(showInputPanel);
        }
    }

    void TerminalPage::_ApplyWorkspaceNodeLoadState(const size_t nodeIndex)
    {
        const auto* workspace = _SelectedWorkspaceForEditing();
        if (!workspace || workspace->Id != _currentWorkspaceId.c_str() || nodeIndex >= workspace->Nodes.size())
        {
            return;
        }

        const auto& node = workspace->Nodes.at(nodeIndex);
        const auto existingTab = _GetCurrentWorkspaceTabByNodeId(node.Id);
        if (!_workspaceNodeLoadsTab(node))
        {
            if (existingTab)
            {
                existingTab->Close();
            }
            return;
        }

        if (existingTab)
        {
            return;
        }

        Workspace singleNodeWorkspace;
        singleNodeWorkspace.Id = workspace->Id;
        singleNodeWorkspace.Nodes.emplace_back(node);

        const auto actions = WorkspaceManager{}.BuildStartupActions(singleNodeWorkspace, _settings);
        if (actions.empty())
        {
            return;
        }

        _PreparePendingWorkspaceNodeInputVisibility(singleNodeWorkspace);
        _PreparePendingWorkspaceNodeIds(singleNodeWorkspace);
        auto clearPendingWorkspaceNodeState = wil::scope_exit([&]() noexcept {
            _workspaceExtension->ClearPendingWorkspaceNodeQueues();
        });

        for (size_t i = 0; i < actions.size(); ++i)
        {
            if (_ShouldSkipWorkspaceStartupAction(actions[i], actions, i))
            {
                continue;
            }

            _actionDispatch->DoAction(actions[i]);
        }
    }

    void TerminalPage::_ApplyWorkspaceNodeTabColor(const size_t nodeIndex)
    {
        const auto* workspace = _SelectedWorkspaceForEditing();
        if (!workspace || workspace->Id != _currentWorkspaceId.c_str() || nodeIndex >= workspace->Nodes.size())
        {
            return;
        }

        if (const auto tabImpl = _GetWorkspaceBackedTabByNodeIndex(nodeIndex))
        {
            if (const auto tabColor = ResolveWorkspaceNodeTabColor(*workspace, nodeIndex, _settings))
            {
                tabImpl->SetRuntimeTabColor(*tabColor);
            }
            else
            {
                tabImpl->ResetRuntimeTabColor();
            }
        }
    }

    void TerminalPage::_PersistWorkspaceNodeInputVisibilityFromTab(const winrt::com_ptr<Tab>& tab, const bool showInputPanel)
    {
        if (_currentWorkspaceId.empty())
        {
            return;
        }

        const auto nodeIndex = _GetWorkspaceBackedTabNodeIndex(tab);
        if (!nodeIndex.has_value())
        {
            return;
        }

        const auto applyVisibility = [&](WorkspaceManager& manager) {
            auto& workspaces = manager.Workspaces();
            const auto it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
                return workspace.Id == _currentWorkspaceId.c_str();
            });
            if (it != workspaces.end())
            {
                if (*nodeIndex < it->Nodes.size())
                {
                    it->Nodes.at(*nodeIndex).ShowInputPanel = showInputPanel;
                    return true;
                }
            }

            return false;
        };

        auto* manager = &_workspaceEditorManager;
        WorkspaceManager persistedManager;
        if (!applyVisibility(*manager))
        {
            persistedManager = WorkspaceManager::Load();
            if (!applyVisibility(persistedManager))
            {
                return;
            }

            manager = &persistedManager;
        }

        if (!manager->Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return;
        }

        _workspaceEditorManager = *manager;
        _workspaceDefinitionsDirty = false;
        _RefreshCurrentWorkspaceSaveBaseline();
        _LoadWorkspaceEditorState();
        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }
    }

    bool TerminalPage::_PersistCurrentWorkspaceTabOrder()
    {
        if (_currentWorkspaceId.empty())
        {
            return false;
        }

        std::vector<std::wstring> orderedNodeIds;
        orderedNodeIds.reserve(_tabs.Size());

        for (const auto& tab : _tabs)
        {
            if (const auto tabImpl = _GetTabImpl(tab))
            {
                if (const auto nodeId = _ResolveLiveCurrentWorkspaceNodeId(tabImpl); !nodeId.empty())
                {
                    orderedNodeIds.emplace_back(nodeId);
                }
            }
        }

        if (orderedNodeIds.empty())
        {
            return false;
        }

        auto manager = WorkspaceManager::Load();
        if (!manager.ReorderWorkspaceNodes(_currentWorkspaceId.c_str(), orderedNodeIds))
        {
            return false;
        }

        if (!manager.Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceSaveFailedWorkspacesFile"));
            return false;
        }

        _workspaceDefinitionsDirty = false;
        _RefreshCurrentWorkspaceSaveBaseline();
        _LoadWorkspaceEditorState();
        _UpdateWorkspaceTabRow();
        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }
        return true;
    }

    bool TerminalPage::_CurrentWorkspaceNeedsSave() const
    {
        Workspace currentWorkspace;
        if (!_TryCaptureCurrentWorkspace(currentWorkspace))
        {
            return false;
        }

        EnsureWorkspaceNodeTabColors(currentWorkspace, _settings);

        if (_currentWorkspaceId.empty())
        {
            return true;
        }

        if (_currentWorkspaceSaveBaseline.has_value())
        {
            return !_workspaceLayoutEquivalent(*_currentWorkspaceSaveBaseline, currentWorkspace);
        }

        auto manager = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        if (const auto workspace = manager.FindById(_currentWorkspaceId.c_str()))
        {
            auto persistedWorkspace = *workspace;
            EnsureWorkspaceNodeTabColors(persistedWorkspace, _settings);
            return !_workspaceLayoutEquivalent(persistedWorkspace, currentWorkspace);
        }

        return true;
    }
