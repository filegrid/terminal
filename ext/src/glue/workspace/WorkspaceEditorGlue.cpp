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

    void TerminalPage::_AddWorkspaceDefinition(const std::optional<size_t> templateIndex)
    {
        auto& workspaces = _workspaceExtension->WorkspaceEditorManager().Workspaces();

        Microsoft::Terminal::Settings::Model::implementation::Workspace workspace;
        if (templateIndex.has_value() && *templateIndex < workspaces.size())
        {
            workspace = workspaces.at(*templateIndex);
        }
        workspace.Name = RS_fmt(L"WorkspaceGeneratedName", workspaces.size() + 1).c_str();
        workspace.Id = workspace.Name;
        if (workspace.BackgroundColor.empty())
        {
            workspace.BackgroundColor = Microsoft::Terminal::Settings::Model::implementation::PickUnusedWorkspaceColor(workspaces);
        }
        workspaces.emplace_back(std::move(workspace));

        _workspaceExtension->WorkspaceDefinitionsDirty() = true;
        _SetSelectedWorkspaceIndex(workspaces.size() - 1);
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
        auto* workspace = _SelectedWorkspaceForEditing();
        if (workspace == nullptr)
        {
            return;
        }

        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode node = workspace->NewNodeDefaults;
        node.Name = RS_fmt(L"WorkspaceEditor_NodeGeneratedName", workspace->Nodes.size() + 1).c_str();
        node.Id = node.Name;
        if (node.ProfileGuid.empty())
        {
            node.ProfileGuid = Utils::GuidToString(_settings.GlobalSettings().DefaultProfile());
            if (const auto profile = _settings.FindProfile(_settings.GlobalSettings().DefaultProfile()))
            {
                node.ProfileName = profile.Name().empty() ? profile.Source().c_str() : profile.Name().c_str();
            }
        }
        workspace->Nodes.emplace_back(std::move(node));
        // Colors are configuration, not runtime inference. Assign one as soon
        // as the node is created so the saved definition is immediately usable.
        EnsureWorkspaceNodeTabColors(*workspace, _settings);
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

        const auto selectedWorkspaceId = _SelectedWorkspaceId();
        const auto previousNavSelection = _workspaceExtension->WorkspaceManagerNavSelection();
        auto manager = _workspaceExtension->WorkspaceDefinitionsDirty() ? _workspaceExtension->WorkspaceEditorManager() : Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        const auto removalPlan = Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceDefinitionRemoval(manager,
                                                                                                                         workspaceId,
                                                                                                                         selectedWorkspaceId,
                                                                                                                         _currentWorkspaceId.c_str(),
                                                                                                                         _workspaceExtension->WorkspaceEditorSelectedIndex(),
                                                                                                                         previousNavSelection,
                                                       L"");
        if (!removalPlan.has_value())
        {
            return false;
        }

        if (!manager.Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return false;
        }

        _workspaceExtension->WorkspaceEditorManager() = manager;
        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _workspaceExtension->WorkspaceEditorEditMode() = true;
        _workspaceExtension->WorkspaceEditorSelectedIndex() = removalPlan->SelectedWorkspaceIndex;
        _workspaceExtension->WorkspaceManagerNavSelection() = removalPlan->NavSelection;

        if (removalPlan->RemovedCurrentWorkspace)
        {
            CurrentWorkspaceId(winrt::hstring{});
        }


        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();

        if (removalPlan->RemovedCurrentWorkspace)
        {
            CloseWindowRequested.raise(*this, nullptr);
            return true;
        }

        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }

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

        _workspaceExtension->WorkspaceEditorManager() = persistedSave->Manager;
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


        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _workspaceExtension->WorkspaceEditorEditMode() = true;
        _workspaceExtension->WorkspaceEditorSelectedIndex() = persistedSave->SavePlan.SelectedWorkspaceIndex;
        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();

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

        const auto selectedWorkspaceId = _SelectedWorkspaceId();
        const auto previousNavSelection = _workspaceExtension->WorkspaceManagerNavSelection();
        auto manager = _workspaceExtension->WorkspaceDefinitionsDirty() ? _workspaceExtension->WorkspaceEditorManager() : Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        const auto removalPlan = Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceNodeRemoval(manager,
                                                                                                                    workspaceId,
                                                                                                                    nodeId,
                                                                                                                    selectedWorkspaceId,
                                                                                                                    _currentWorkspaceId.c_str(),
                                                                                                                    _workspaceExtension->WorkspaceEditorSelectedIndex(),
                                                                                                                    previousNavSelection,
                                                  L"");
        if (removalPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeMutationDisposition::NotFound)
        {
            return WorkspaceNodeRemoveResult::NotFound;
        }

        if (!manager.Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return WorkspaceNodeRemoveResult::SaveFailed;
        }

        _workspaceExtension->WorkspaceEditorManager() = manager;
        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _workspaceExtension->WorkspaceEditorEditMode() = true;
        _workspaceExtension->WorkspaceEditorSelectedIndex() = removalPlan.SelectedWorkspaceIndex;
        _workspaceExtension->WorkspaceManagerNavSelection() = removalPlan.NavSelection;


        if (removalPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeMutationDisposition::RemovedWorkspace)
        {
            if (removalPlan.RemovedCurrentWorkspace)
            {
                CurrentWorkspaceId(winrt::hstring{});
            }
        }

        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();
        if (removalPlan.RemovedCurrentWorkspace)
        {
            CloseWindowRequested.raise(*this, nullptr);
            return WorkspaceNodeRemoveResult::RemovedWorkspace;
        }

        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }

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
            _workspaceManagerContent->UpdateContent(_BuildWorkspaceManagerContent());
        }
    }
