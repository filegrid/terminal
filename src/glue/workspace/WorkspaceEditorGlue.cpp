    Microsoft::Terminal::Settings::Model::implementation::Workspace* TerminalPage::_SelectedWorkspaceForEditing() noexcept
    {
        auto& workspaces = _workspaceExtension->WorkspaceEditorManager().Workspaces();
        if (_workspaceExtension->WorkspaceEditorSelectedIndex() >= workspaces.size())
        {
            return nullptr;
        }

        return &workspaces.at(_workspaceExtension->WorkspaceEditorSelectedIndex());
    }

    const Microsoft::Terminal::Settings::Model::implementation::Workspace* TerminalPage::_SelectedWorkspaceForEditing() const noexcept
    {
        const auto& workspaces = _workspaceExtension->WorkspaceEditorManager().Workspaces();
        if (_workspaceExtension->WorkspaceEditorSelectedIndex() >= workspaces.size())
        {
            return nullptr;
        }

        return &workspaces.at(_workspaceExtension->WorkspaceEditorSelectedIndex());
    }

    Microsoft::Terminal::Settings::Model::implementation::Workspace* TerminalPage::_SelectedCurrentWorkspaceForEditingPtr() noexcept
    {
        if (auto* workspace = _SelectedWorkspaceForEditing(); workspace && workspace->Id == _currentWorkspaceId.c_str())
        {
            return workspace;
        }

        return nullptr;
    }

    const Microsoft::Terminal::Settings::Model::implementation::Workspace* TerminalPage::_SelectedCurrentWorkspaceForEditingPtr() const noexcept
    {
        if (const auto* workspace = _SelectedWorkspaceForEditing(); workspace && workspace->Id == _currentWorkspaceId.c_str())
        {
            return workspace;
        }

        return nullptr;
    }

    void TerminalPage::_ApplyWorkspaceEditorState(const Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager,
                                                  const size_t selectedWorkspaceIndex,
                                                  const std::optional<int32_t> navSelection)
    {
        _workspaceExtension->WorkspaceEditorManager() = manager;
        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _workspaceExtension->WorkspaceEditorEditMode() = true;
        _workspaceExtension->WorkspaceEditorSelectedIndex() = selectedWorkspaceIndex;
        if (navSelection.has_value())
        {
            _workspaceExtension->WorkspaceManagerNavSelection() = *navSelection;
        }
    }

    Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager TerminalPage::_LoadWorkspaceEditorMutationManager() const
    {
        return _workspaceExtension->WorkspaceDefinitionsDirty() ?
                   _workspaceExtension->WorkspaceEditorManager() :
                   Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
    }

    bool TerminalPage::_TryPersistWorkspaceEditorMutation(Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager)
    {
        if (manager.Save())
        {
            return true;
        }

        ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
        return false;
    }

    std::optional<Microsoft::Terminal::Settings::Model::implementation::WorkspaceEditorDefinitionRemovalPlan> TerminalPage::_PrepareWorkspaceDefinitionRemovalPlan(Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager,
                                                                                                                                                                      const std::wstring_view workspaceId) const
    {
        return Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceDefinitionRemoval(manager,
                                                                                                       workspaceId,
                                                                                                       _SelectedWorkspaceId(),
                                                                                                       _currentWorkspaceId.c_str(),
                                                                                                       _workspaceExtension->WorkspaceEditorSelectedIndex(),
                                                                                                       _workspaceExtension->WorkspaceManagerNavSelection(),
                                                                                                       L"");
    }

    Microsoft::Terminal::Settings::Model::implementation::WorkspaceEditorNodeRemovalPlan TerminalPage::_PrepareWorkspaceNodeRemovalPlan(Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager,
                                                                                                                                         const std::wstring_view workspaceId,
                                                                                                                                         const std::wstring_view nodeId) const
    {
        return Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceNodeRemoval(manager,
                                                                                                 workspaceId,
                                                                                                 nodeId,
                                                                                                 _SelectedWorkspaceId(),
                                                                                                 _currentWorkspaceId.c_str(),
                                                                                                 _workspaceExtension->WorkspaceEditorSelectedIndex(),
                                                                                                 _workspaceExtension->WorkspaceManagerNavSelection(),
                                                                                                 L"");
    }

    void TerminalPage::_FinalizeWorkspaceEditorMutationUi(const bool removedCurrentWorkspace)
    {
        if (removedCurrentWorkspace)
        {
            CurrentWorkspaceId(winrt::hstring{});
        }

        _RefreshWorkspaceChrome();

        if (removedCurrentWorkspace)
        {
            CloseWindowRequested.raise(*this, nullptr);
            return;
        }

        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }
    }

    void TerminalPage::_AddWorkspaceDefinition(const std::optional<size_t> templateIndex)
    {
        auto& manager = _workspaceExtension->WorkspaceEditorManager();
        const auto generatedName = std::wstring{ RS_fmt(L"WorkspaceGeneratedName", manager.Workspaces().size() + 1).c_str() };
        const auto addedWorkspace = ::terminal::workspace::AddWorkspaceDefinition(manager, generatedName, templateIndex);
        if (!addedWorkspace.has_value())
        {
            return;
        }

        _workspaceExtension->WorkspaceDefinitionsDirty() = true;
        _SetSelectedWorkspaceIndex(addedWorkspace->AddedWorkspaceIndex);
    }

    void TerminalPage::_DeleteSelectedWorkspaceDefinition()
    {
        if (const auto workspace = _SelectedWorkspaceForEditing())
        {
            _RemoveWorkspaceDefinitionById(workspace->Id);
        }
    }

    void TerminalPage::_AddWorkspaceNode()
    {
        const auto workspaceIndex = _workspaceExtension->WorkspaceEditorSelectedIndex();
        auto& manager = _workspaceExtension->WorkspaceEditorManager();
        if (workspaceIndex >= manager.Workspaces().size())
        {
            return;
        }

        const auto& workspace = manager.Workspaces().at(workspaceIndex);
        const auto generatedName = std::wstring{ RS_fmt(L"WorkspaceEditor_NodeGeneratedName", workspace.Nodes.size() + 1).c_str() };
        auto defaultProfileGuid = std::wstring{};
        auto defaultProfileName = std::wstring{};
        if (const auto profile = _settings.FindProfile(_settings.GlobalSettings().DefaultProfile()))
        {
            defaultProfileGuid = Utils::GuidToString(_settings.GlobalSettings().DefaultProfile());
            defaultProfileName = profile.Name().empty() ? profile.Source().c_str() : profile.Name().c_str();
        }
        else
        {
            defaultProfileGuid = Utils::GuidToString(_settings.GlobalSettings().DefaultProfile());
        }

        const auto addResult = ::terminal::workspace::AddWorkspaceNode(manager,
                                                                       workspaceIndex,
                                                                       generatedName,
                                                                       defaultProfileGuid,
                                                                       defaultProfileName);
        if (!addResult.Added)
        {
            return;
        }

        // Colors are configuration, not runtime inference. Assign one as soon
        // as the node is created so the saved definition is immediately usable.
        EnsureWorkspaceNodeTabColors(manager.Workspaces().at(workspaceIndex), _settings);
        _workspaceExtension->WorkspaceDefinitionsDirty() = true;
    }

    void TerminalPage::_DeleteWorkspaceNode(const size_t nodeIndex)
    {
        auto* workspace = _SelectedWorkspaceForEditing();
        if (workspace == nullptr || nodeIndex >= workspace->Nodes.size())
        {
            return;
        }

        const auto workspaceId = workspace->Id;
        const auto nodeId = workspace->Nodes.at(nodeIndex).Id;
        if (workspaceId.empty() || nodeId.empty())
        {
            return;
        }

        if (workspaceId == _currentWorkspaceId.c_str())
        {
            if (const auto tab = _GetCurrentWorkspaceTabByNodeId(nodeId))
            {
                _RemoveWorkspaceNodeTab(*tab, workspaceId, nodeId);
                return;
            }
        }

        _RemoveWorkspaceNodeById(workspaceId, nodeId);
    }

    bool TerminalPage::_RemoveWorkspaceDefinitionById(const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            return false;
        }

        auto manager = _LoadWorkspaceEditorMutationManager();
        const auto removalPlan = _PrepareWorkspaceDefinitionRemovalPlan(manager, workspaceId);
        if (!removalPlan.has_value())
        {
            return false;
        }

        if (!_TryPersistWorkspaceEditorMutation(manager))
        {
            return false;
        }

        _CloseOpenWorkspaceWindow(workspaceId);
        _ApplyWorkspaceEditorState(manager, removalPlan->SelectedWorkspaceIndex, removalPlan->NavSelection);
        _FinalizeWorkspaceEditorMutationUi(removalPlan->RemovedCurrentWorkspace);
        return true;
    }

    bool TerminalPage::_SaveWorkspaceEditorState()
    {
        for (auto& workspace : _workspaceExtension->WorkspaceEditorManager().Workspaces())
        {
            EnsureWorkspaceNodeTabColors(workspace, _settings);
        }
        const auto persistedSave = ::terminal::workspace::PersistWorkspaceEditorState(_workspaceExtension->WorkspaceEditorManager(),
                                                                                      _currentWorkspaceId.c_str(),
                                                                  L"",
                                                                                      _workspaceExtension->WorkspaceEditorSelectedIndex());
        if (!persistedSave.has_value())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return false;
        }

        _ApplyWorkspaceEditorState(persistedSave->Manager, persistedSave->SavePlan.SelectedWorkspaceIndex);
        if (!_currentWorkspaceId.empty())
        {
            if (persistedSave->SavePlan.CurrentWorkspaceExists)
            {
                CurrentWorkspaceId(winrt::hstring{ persistedSave->SavePlan.ResolvedCurrentWorkspaceId });
            }
            else
            {
                CurrentWorkspaceId(winrt::hstring{});
            }
        }
        _RefreshWorkspaceChrome();

        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }

        return true;
    }

    TerminalPage::WorkspaceNodeRemoveResult TerminalPage::_RemoveWorkspaceNodeById(const std::wstring_view workspaceId, const std::wstring_view nodeId)
    {
        if (workspaceId.empty() || nodeId.empty())
        {
            return WorkspaceNodeRemoveResult::NotFound;
        }

        auto manager = _LoadWorkspaceEditorMutationManager();
        const auto removalPlan = _PrepareWorkspaceNodeRemovalPlan(manager, workspaceId, nodeId);
        if (removalPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeMutationDisposition::NotFound)
        {
            return WorkspaceNodeRemoveResult::NotFound;
        }

        if (!_TryPersistWorkspaceEditorMutation(manager))
        {
            return WorkspaceNodeRemoveResult::SaveFailed;
        }

        if (removalPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeMutationDisposition::RemovedWorkspace)
        {
            _CloseOpenWorkspaceWindow(workspaceId);
        }
        _ApplyWorkspaceEditorState(manager, removalPlan.SelectedWorkspaceIndex, removalPlan.NavSelection);
        _FinalizeWorkspaceEditorMutationUi(removalPlan.RemovedCurrentWorkspace);

        return removalPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeMutationDisposition::RemovedWorkspace ?
                   WorkspaceNodeRemoveResult::RemovedWorkspace :
                   WorkspaceNodeRemoveResult::RemovedNode;
    }

    TerminalPage::WorkspaceNodeRemoveResult TerminalPage::_RemoveWorkspaceNodeTab(const winrt::TerminalApp::Tab& tab,
                                                                                  const std::wstring_view workspaceId,
                                                                                  const std::wstring_view nodeId)
    {
        const auto result = _RemoveWorkspaceNodeById(workspaceId, nodeId);
        if (result == WorkspaceNodeRemoveResult::RemovedNode)
        {
            _RemoveTab(tab);
            _RefreshCurrentWorkspaceSaveBaseline();
        }
        return result;
    }

    void TerminalPage::_RebuildWorkspaceManagerTab()
    {
        if (_workspaceManagerContent)
        {
            _workspaceExtension->UpdateWorkspaceManagerPaneContent(_workspaceManagerContent, _BuildWorkspaceManagerContent());
        }
    }
