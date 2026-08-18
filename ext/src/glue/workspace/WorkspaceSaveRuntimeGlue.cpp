    void TerminalPage::_ShowWorkspaceNameMenu()
    {
        if (!_tabRow)
        {
            return;
        }

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        // Use the same flyout contract as the New Tab split button: the visible
        // button is both the flyout owner and the ShowAt target. Do not add a
        // workspace-specific placement or offset.
        const auto button = _currentWorkspaceId.empty() ?
                                tabRowImpl->WorkspaceMenuButton() :
                                tabRowImpl->WorkspaceNameButton();
        if (_workspaceFlyout)
        {
            _workspaceFlyout.Hide();
            _workspaceFlyout = nullptr;
        }

        _workspaceFlyout = _CreateWorkspaceFlyout();
        button.Flyout(_workspaceFlyout);
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

        const auto persistedRename = ::terminal::workspace::PersistWorkspaceRename(_currentWorkspaceId.c_str(), newName.c_str());
        if (!persistedRename.has_value())
        {
            ActionSaveFailed(RS_(L"WorkspaceRenameFailed"));
            return;
        }

        _workspaceEditorManager = persistedRename->Manager;
        _workspaceDefinitionsDirty = false;
        CurrentWorkspaceId(winrt::hstring{ persistedRename->ResolvedWorkspaceName });
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
            return ::terminal::workspace::LoadResolvedWorkspaceDefinition(_currentWorkspaceId.c_str(), std::nullopt);
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
                const auto existingNode = _ResolveCurrentWorkspaceNode(tabImpl);

                const auto profile = _settings.GetProfileForArgs(*terminalArgs);
                const auto profileSource = profile ? std::wstring{ profile.Source().c_str() } : std::wstring{};
                const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
                const auto commandline = std::wstring{ terminalArgs->Commandline().c_str() };
                Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionInput resolutionInput;
                resolutionInput.PersistedNode = existingNode;
                resolutionInput.ProfileSource = profileSource;
                resolutionInput.ProfileCommandline = profileCommandline;
                resolutionInput.TerminalCommandline = commandline;
                resolutionInput.TerminalStartingDirectory = terminalArgs->StartingDirectory().c_str();
                if (const auto control = tabImpl->GetActiveTerminalControl())
                {
                    if (const auto captureState = _workspaceExtension->FindWorkspaceChatTerminalState(_WorkspaceChatStateKey(control)))
                    {
                        resolutionInput.ObservedStartupAction = terminal::workspacechat::ResolveCapturedStartupAction(*captureState);
                        resolutionInput.ObservedWorkingDirectory = terminal::workspacechat::ResolveCapturedWorkingDirectory(*captureState);
                        resolutionInput.ObservedOperatingSystem = terminal::workspacechat::ResolveCapturedOperatingSystem(*captureState);
                        resolutionInput.ObservedShellType = terminal::workspacechat::ResolveCapturedShellType(*captureState);
                    }

                    if (resolutionInput.ObservedWorkingDirectory.empty())
                    {
                        resolutionInput.ObservedWorkingDirectory = _ResolveTrackedTerminalWorkingDirectory(control);
                    }
                }
                if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tabImpl))
                {
                    resolutionInput.RuntimeStartupAction = runtimeState->StartupAction;
                    resolutionInput.RuntimeExplicitCommandline = runtimeState->ExplicitCommandline;
                    resolutionInput.RuntimeStartingDirectory = runtimeState->StartingDirectory;
                    resolutionInput.RuntimeOperatingSystem = runtimeState->OperatingSystem;
                    resolutionInput.RuntimeShellType = runtimeState->ShellType;
                }

                const auto resolution = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeLaunchResolution(resolutionInput);
                Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabCaptureState captureState;
                captureState.PersistedNode = existingNode;
                captureState.LiveTabTitle = tabImpl->Title().c_str();
                captureState.StartupTabTitle = terminalArgs->TabTitle().c_str();
                captureState.GeneratedNodeName = RS_fmt(L"WorkspaceEditor_NodeGeneratedName", nodeIndex + 1).c_str();
                captureState.ProfileGuid = terminalArgs->Profile().empty() ? ::Microsoft::Console::Utils::GuidToString(_settings.GlobalSettings().DefaultProfile()) : terminalArgs->Profile().c_str();
                captureState.ProfileName = profile ? std::wstring{ (profile.Name().empty() ? profile.Source() : profile.Name()).c_str() } : std::wstring{};
                captureState.LaunchResolution = resolution;
                captureState.ShowInputPanel = tabImpl->ShowWorkspaceInputPanel();
                if (const auto tabColor = tabImpl->GetTabColor())
                {
                    captureState.TabColor = _workspaceColorToString(*tabColor);
                }
                node = Microsoft::Terminal::Settings::Model::implementation::BuildWorkspaceCapturedNode(captureState);
                capturedNodes.emplace_back(std::move(node));
                ++nodeIndex;
            }
        }

        if (const auto preparedWorkspace = Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceForCapture(currentWorkspaceDefinition, std::move(capturedNodes)))
        {
            workspace = *preparedWorkspace;
            return true;
        }

        workspace.Nodes.clear();
        return false;
    }

    std::optional<size_t> TerminalPage::_GetWorkspaceBackedTabNodeIndex(const winrt::com_ptr<Tab>& tab) const
    {
        if (!tab)
        {
            return std::nullopt;
        }

        const auto selectedWorkspace = [&]() -> std::optional<Workspace> {
            if (const auto workspace = _SelectedWorkspaceForEditing();
                workspace && workspace->Id == _currentWorkspaceId.c_str())
            {
                return *workspace;
            }
            return std::nullopt;
        }();
        const auto workspaceDefinition = ::terminal::workspace::LoadResolvedWorkspaceDefinition(_currentWorkspaceId.c_str(), selectedWorkspace);
        std::vector<Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabSnapshot> tabs;
        tabs.reserve(_tabs.Size());
        size_t targetTabIndex = 0;
        bool foundTargetTab = false;
        for (const auto& candidate : _tabs)
        {
            Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabSnapshot snapshot;
            if (const auto tabImpl = _GetTabImpl(candidate))
            {
                snapshot.LoadsWorkspaceNode = _BuildWorkspaceNodeArgs(tabImpl).has_value();
                if (const auto state = _FindWorkspaceNodeRuntimeState(tabImpl))
                {
                    snapshot.RuntimeNodeId = state->WorkspaceNodeId;
                }
                if (tabImpl.get() == tab.get())
                {
                    targetTabIndex = tabs.size();
                    foundTargetTab = true;
                }
            }
            tabs.emplace_back(std::move(snapshot));
        }

        return foundTargetTab ? Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceBackedTabIndex(workspaceDefinition, tabs, targetTabIndex) :
                                std::nullopt;
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

        const auto selectedWorkspace = [&]() -> std::optional<Workspace> {
            if (const auto workspace = _SelectedWorkspaceForEditing();
                workspace && workspace->Id == _currentWorkspaceId.c_str())
            {
                return *workspace;
            }
            return std::nullopt;
        }();
        const auto workspaceDefinition = ::terminal::workspace::LoadResolvedWorkspaceDefinition(_currentWorkspaceId.c_str(), selectedWorkspace);
        std::vector<Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabSnapshot> tabs;
        tabs.reserve(_tabs.Size());
        size_t targetTabIndex = 0;
        bool foundTargetTab = false;
        for (const auto& candidate : _tabs)
        {
            Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabSnapshot snapshot;
            if (const auto tabImpl = _GetTabImpl(candidate))
            {
                snapshot.LoadsWorkspaceNode = _BuildWorkspaceNodeArgs(tabImpl).has_value();
                if (const auto state = _FindWorkspaceNodeRuntimeState(tabImpl))
                {
                    snapshot.RuntimeNodeId = state->WorkspaceNodeId;
                }
                if (tabImpl.get() == tab.get())
                {
                    targetTabIndex = tabs.size();
                    foundTargetTab = true;
                }
            }
            tabs.emplace_back(std::move(snapshot));
        }

        return foundTargetTab ? Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceBackedTabNode(workspaceDefinition, tabs, targetTabIndex) :
                                std::nullopt;
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

        const auto selectedWorkspace = [&]() -> std::optional<Workspace> {
            if (const auto workspace = _SelectedWorkspaceForEditing();
                workspace && workspace->Id == _currentWorkspaceId.c_str())
            {
                return *workspace;
            }
            return std::nullopt;
        }();
        const auto workspaceDefinition = ::terminal::workspace::LoadResolvedWorkspaceDefinition(_currentWorkspaceId.c_str(), selectedWorkspace);
        std::vector<Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabSnapshot> tabs;
        tabs.reserve(_tabs.Size());
        std::vector<winrt::com_ptr<Tab>> tabImpls;
        tabImpls.reserve(_tabs.Size());
        for (const auto& tab : _tabs)
        {
            Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabSnapshot snapshot;
            if (const auto tabImpl = _GetTabImpl(tab))
            {
                snapshot.LoadsWorkspaceNode = _BuildWorkspaceNodeArgs(tabImpl).has_value();
                if (const auto state = _FindWorkspaceNodeRuntimeState(tabImpl))
                {
                    snapshot.RuntimeNodeId = state->WorkspaceNodeId;
                }
                tabImpls.emplace_back(tabImpl);
            }
            else
            {
                tabImpls.emplace_back(nullptr);
            }
            tabs.emplace_back(std::move(snapshot));
        }

        if (const auto tabIndex = Microsoft::Terminal::Settings::Model::implementation::FindWorkspaceBackedTabSnapshotIndex(workspaceDefinition, tabs, nodeIndex);
            tabIndex.has_value() && tabIndex.value() < tabImpls.size())
        {
            return tabImpls.at(tabIndex.value());
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

    void TerminalPage::_ApplyWorkspaceNodeIcon(const size_t nodeIndex)
    {
        const auto* workspace = _SelectedWorkspaceForEditing();
        if (!workspace || workspace->Id != _currentWorkspaceId.c_str() || nodeIndex >= workspace->Nodes.size())
        {
            return;
        }

        if (const auto tabImpl = _GetWorkspaceBackedTabByNodeIndex(nodeIndex))
        {
            _UpdateTabIcon(*tabImpl);
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

        const auto persistedManager = ::terminal::workspace::PersistWorkspaceNodeInputVisibility(_workspaceEditorManager,
                                                                                                  _currentWorkspaceId.c_str(),
                                                                                                  *nodeIndex,
                                                                                                  showInputPanel);
        if (!persistedManager.has_value())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return;
        }

        _workspaceEditorManager = *persistedManager;
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

        const auto persistedManager = ::terminal::workspace::PersistWorkspaceNodeOrder(_currentWorkspaceId.c_str(), orderedNodeIds);
        if (!persistedManager.has_value())
        {
            ActionSaveFailed(RS_(L"WorkspaceSaveFailedWorkspacesFile"));
            return false;
        }

        _workspaceEditorManager = *persistedManager;
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

        auto persistedWorkspace = ::terminal::workspace::LoadPersistedWorkspaceForComparison(_currentWorkspaceId.c_str());
        if (persistedWorkspace.has_value())
        {
            EnsureWorkspaceNodeTabColors(*persistedWorkspace, _settings);
        }

        return Microsoft::Terminal::Settings::Model::implementation::IsWorkspaceDirty(currentWorkspace,
                                                                                      _currentWorkspaceId.c_str(),
                                                                                      _currentWorkspaceSaveBaseline,
                                                                                      persistedWorkspace);
    }
