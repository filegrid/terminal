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

    void TerminalPage::_AddWorkspaceDefinition()
    {
        auto& workspaces = _workspaceExtension->WorkspaceEditorManager().Workspaces();

        Microsoft::Terminal::Settings::Model::implementation::Workspace workspace;
        workspace.Name = RS_fmt(L"WorkspaceGeneratedName", workspaces.size() + 1).c_str();
        workspace.Id = workspace.Name;
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

        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode node;
        node.Name = RS_fmt(L"WorkspaceEditor_NodeGeneratedName", workspace->Nodes.size() + 1).c_str();
        node.Id = node.Name;
        node.ProfileGuid = Utils::GuidToString(_settings.GlobalSettings().DefaultProfile());
        workspace->Nodes.emplace_back(std::move(node));
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
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return false;
        }

        const auto removedWorkspaceIndex = gsl::narrow_cast<size_t>(std::distance(workspaces.begin(), workspaceIt));
        const auto removedCurrentWorkspace = workspaceId == _currentWorkspaceId.c_str();
        workspaces.erase(workspaceIt);

        if (!manager.Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return false;
        }

        _workspaceExtension->WorkspaceEditorManager() = manager;
        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _workspaceExtension->WorkspaceEditorEditMode() = true;
        _LoadWorkspaceEditorState();

        if (previousNavSelection >= 1000)
        {
            if (workspaces.empty())
            {
                _workspaceExtension->WorkspaceManagerNavSelection() = 0;
            }
            else if (selectedWorkspaceId == workspaceId)
            {
                const auto newWorkspaceIndex = std::min(removedWorkspaceIndex, workspaces.size() - 1);
                _workspaceExtension->WorkspaceManagerNavSelection() = 1000 + gsl::narrow_cast<int32_t>(newWorkspaceIndex * 100);
            }
            else
            {
                const auto previousWorkspaceIndex = gsl::narrow_cast<size_t>((previousNavSelection - 1000) / 100);
                if (previousWorkspaceIndex > removedWorkspaceIndex)
                {
                    _workspaceExtension->WorkspaceManagerNavSelection() -= 100;
                }
            }
        }

        if (removedCurrentWorkspace)
        {
            CurrentWorkspaceId(winrt::hstring{});
        }

        const auto state = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
        if (const auto lastWorkspaceId = state.LastOpenedWorkspaceId(); !lastWorkspaceId.empty() &&
            _workspaceExtension->WorkspaceEditorManager().FindById(lastWorkspaceId) == nullptr)
        {
            state.LastOpenedWorkspaceId(L"");
            state.Flush();
        }

        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();

        if (removedCurrentWorkspace)
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
        const auto persistedManager = WorkspaceManager::Load();
        std::optional<size_t> currentWorkspaceIndex;
        if (!_currentWorkspaceId.empty())
        {
            const auto& persistedWorkspaces = persistedManager.Workspaces();
            const auto currentIt = std::find_if(persistedWorkspaces.begin(), persistedWorkspaces.end(), [&](const auto& workspace) {
                return workspace.Name == _currentWorkspaceId.c_str();
            });
            if (currentIt != persistedWorkspaces.end())
            {
                currentWorkspaceIndex = gsl::narrow_cast<size_t>(std::distance(persistedWorkspaces.begin(), currentIt));
            }
        }

        std::unordered_set<std::wstring> usedWorkspaceNames;
        for (auto& workspace : _workspaceExtension->WorkspaceEditorManager().Workspaces())
        {
            _NormalizeWorkspacePersistableNames(workspace);
            workspace.Name = _makeUniquePersistedName(workspace.Name, usedWorkspaceNames);
            workspace.Id = workspace.Name;
            EnsureWorkspaceNodeTabColors(workspace, _settings);
        }

        if (!_workspaceExtension->WorkspaceEditorManager().Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return false;
        }

        if (currentWorkspaceIndex.has_value() && *currentWorkspaceIndex < _workspaceExtension->WorkspaceEditorManager().Workspaces().size())
        {
            CurrentWorkspaceId(winrt::hstring{ _workspaceExtension->WorkspaceEditorManager().Workspaces().at(*currentWorkspaceIndex).Name });
        }
        else if (!_currentWorkspaceId.empty())
        {
            CurrentWorkspaceId(winrt::hstring{});
        }

        const auto state = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
        if (const auto lastWorkspaceId = state.LastOpenedWorkspaceId(); !lastWorkspaceId.empty() &&
            _workspaceExtension->WorkspaceEditorManager().FindById(lastWorkspaceId) == nullptr)
        {
            state.LastOpenedWorkspaceId(L"");
            state.Flush();
        }

        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _workspaceExtension->WorkspaceEditorEditMode() = true;
        _LoadWorkspaceEditorState();
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
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return WorkspaceNodeRemoveResult::NotFound;
        }

        auto& nodes = workspaceIt->Nodes;
        const auto nodeIt = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
            return node.Id == nodeId;
        });
        if (nodeIt == nodes.end())
        {
            return WorkspaceNodeRemoveResult::NotFound;
        }

        if (nodes.size() == 1)
        {
            return _RemoveWorkspaceDefinitionById(workspaceId) ? WorkspaceNodeRemoveResult::RemovedWorkspace :
                                                                 WorkspaceNodeRemoveResult::SaveFailed;
        }

        const auto removedNodeIndex = gsl::narrow_cast<size_t>(std::distance(nodes.begin(), nodeIt));
        nodes.erase(nodeIt);

        if (!manager.Save())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_SaveFailed"));
            return WorkspaceNodeRemoveResult::SaveFailed;
        }

        _workspaceExtension->WorkspaceEditorManager() = manager;
        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _LoadWorkspaceEditorState();

        if (selectedWorkspaceId == workspaceId && previousNavSelection >= 1000)
        {
            const auto selectedWorkspace = _SelectedWorkspaceForEditing();
            const auto workspaceSubSelection = (previousNavSelection - 1000) % 100;
            if (selectedWorkspace && workspaceSubSelection >= 10)
            {
                const auto selectedNodeIndex = gsl::narrow_cast<size_t>(workspaceSubSelection - 10);
                if (selectedNodeIndex == removedNodeIndex)
                {
                    if (removedNodeIndex > 0)
                    {
                        _workspaceExtension->WorkspaceManagerNavSelection() = 1000 + gsl::narrow_cast<int32_t>(_workspaceExtension->WorkspaceEditorSelectedIndex() * 100) + 10 + gsl::narrow_cast<int32_t>(removedNodeIndex - 1);
                    }
                    else
                    {
                        _workspaceExtension->WorkspaceManagerNavSelection() = 1000 + gsl::narrow_cast<int32_t>(_workspaceExtension->WorkspaceEditorSelectedIndex() * 100);
                    }
                }
                else if (selectedNodeIndex > removedNodeIndex)
                {
                    _workspaceExtension->WorkspaceManagerNavSelection() = previousNavSelection - 1;
                }
            }
        }

        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();
        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }

        return WorkspaceNodeRemoveResult::RemovedNode;
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
