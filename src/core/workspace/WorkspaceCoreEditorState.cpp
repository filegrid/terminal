    WorkspaceEditorLoadState LoadWorkspaceEditorState(const std::wstring_view selectedWorkspaceId,
                                                      const std::wstring_view currentWorkspaceId,
                                                      const size_t fallbackIndex)
    {
        auto manager = WorkspaceManager::Load();
        return WorkspaceEditorLoadState{
            .Manager = manager,
            .SelectedWorkspaceIndex = ResolveWorkspaceEditorSelectedIndex(manager, selectedWorkspaceId, currentWorkspaceId, fallbackIndex),
        };
    }

    WorkspaceEditorSavePlan PrepareWorkspaceEditorForSave(WorkspaceManager& editedManager,
                                                          const WorkspaceManager& persistedManager,
                                                          const std::wstring_view currentWorkspaceId,
                                                          const std::wstring_view lastOpenedWorkspaceId,
                                                          const size_t fallbackSelectedWorkspaceIndex)
    {
        WorkspaceEditorSavePlan plan;

        std::optional<size_t> currentWorkspaceIndex;
        if (!currentWorkspaceId.empty())
        {
            const auto& persistedWorkspaces = persistedManager.Workspaces();
            if (const auto currentIt = std::find_if(persistedWorkspaces.begin(), persistedWorkspaces.end(), [&](const auto& workspace) {
                    return workspace.Id == currentWorkspaceId;
                });
                currentIt != persistedWorkspaces.end())
            {
                currentWorkspaceIndex = gsl::narrow_cast<size_t>(std::distance(persistedWorkspaces.begin(), currentIt));
            }
        }

        FinalizeWorkspaceManagerNames(editedManager);

        if (currentWorkspaceIndex.has_value() && *currentWorkspaceIndex < editedManager.Workspaces().size())
        {
            plan.CurrentWorkspaceExists = true;
            plan.ResolvedCurrentWorkspaceId = editedManager.Workspaces().at(*currentWorkspaceIndex).Id;
        }
        else
        {
            plan.CurrentWorkspaceExists = currentWorkspaceId.empty();
        }

        plan.LastOpenedWorkspaceExists = lastOpenedWorkspaceId.empty() || editedManager.FindById(lastOpenedWorkspaceId) != nullptr;
        if (!editedManager.Workspaces().empty())
        {
            plan.SelectedWorkspaceIndex = std::min(fallbackSelectedWorkspaceIndex, editedManager.Workspaces().size() - 1);
        }
        return plan;
    }

    std::optional<WorkspaceEditorDefinitionRemovalPlan> PrepareWorkspaceDefinitionRemoval(WorkspaceManager& manager,
                                                                                          const std::wstring_view workspaceId,
                                                                                          const std::wstring_view selectedWorkspaceId,
                                                                                          const std::wstring_view currentWorkspaceId,
                                                                                          const size_t fallbackSelectedWorkspaceIndex,
                                                                                          const int32_t previousNavSelection,
                                                                                          const std::wstring_view lastOpenedWorkspaceId)
    {
        size_t removedWorkspaceIndex = 0;
        if (!RemoveWorkspaceDefinition(manager, workspaceId, &removedWorkspaceIndex))
        {
            return std::nullopt;
        }

        WorkspaceEditorDefinitionRemovalPlan plan;
        plan.RemovedCurrentWorkspace = workspaceId == currentWorkspaceId;
        plan.LastOpenedWorkspaceExists = lastOpenedWorkspaceId.empty() || manager.FindById(lastOpenedWorkspaceId) != nullptr;
        plan.NavSelection = ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(previousNavSelection,
                                                                                    selectedWorkspaceId,
                                                                                    workspaceId,
                                                                                    removedWorkspaceIndex,
                                                                                    manager.Workspaces().size());
        if (!manager.Workspaces().empty())
        {
            plan.SelectedWorkspaceIndex = ResolveWorkspaceEditorSelectedIndex(manager,
                                                                              selectedWorkspaceId,
                                                                              currentWorkspaceId,
                                                                              fallbackSelectedWorkspaceIndex);
        }

        return plan;
    }

    WorkspaceEditorNodeRemovalPlan PrepareWorkspaceNodeRemoval(WorkspaceManager& manager,
                                                               const std::wstring_view workspaceId,
                                                               const std::wstring_view nodeId,
                                                               const std::wstring_view selectedWorkspaceId,
                                                               const std::wstring_view currentWorkspaceId,
                                                               const size_t fallbackSelectedWorkspaceIndex,
                                                               const int32_t previousNavSelection,
                                                               const std::wstring_view lastOpenedWorkspaceId)
    {
        const auto mutation = RemoveWorkspaceNode(manager, workspaceId, nodeId);

        WorkspaceEditorNodeRemovalPlan plan;
        plan.Disposition = mutation.Disposition;
        if (mutation.Disposition == WorkspaceNodeMutationDisposition::NotFound)
        {
            return plan;
        }

        plan.RemovedCurrentWorkspace = workspaceId == currentWorkspaceId;
        plan.LastOpenedWorkspaceExists = lastOpenedWorkspaceId.empty() || manager.FindById(lastOpenedWorkspaceId) != nullptr;
        if (!manager.Workspaces().empty())
        {
            plan.SelectedWorkspaceIndex = ResolveWorkspaceEditorSelectedIndex(manager,
                                                                              selectedWorkspaceId,
                                                                              currentWorkspaceId,
                                                                              fallbackSelectedWorkspaceIndex);
        }

        if (mutation.Disposition == WorkspaceNodeMutationDisposition::RemovedWorkspace)
        {
            plan.NavSelection = ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(previousNavSelection,
                                                                                        selectedWorkspaceId,
                                                                                        workspaceId,
                                                                                        mutation.WorkspaceIndex,
                                                                                        manager.Workspaces().size());
        }
        else if (previousNavSelection >= _workspaceManagerWorkspaceSelectionBase)
        {
            plan.NavSelection = ResolveWorkspaceManagerNavSelectionAfterNodeRemoval(previousNavSelection,
                                                                                   selectedWorkspaceId,
                                                                                   workspaceId,
                                                                                   plan.SelectedWorkspaceIndex,
                                                                                   mutation.NodeIndex);
        }
        else
        {
            plan.NavSelection = previousNavSelection;
        }

        return plan;
    }

    WorkspaceCurrentState ResolveWorkspaceCurrentState(const std::wstring_view currentWorkspaceId,
                                                       const WorkspaceManager& manager,
                                                       const std::wstring_view defaultDisplayName,
                                                       const std::wstring_view unsavedTabRowName)
    {
        WorkspaceCurrentState state;
        if (currentWorkspaceId.empty())
        {
            state.DisplayName = std::wstring{ defaultDisplayName };
            state.TabRowName = std::wstring{ unsavedTabRowName };
            return state;
        }

        state.DisplayName = std::wstring{ currentWorkspaceId };
        state.TabRowName = state.DisplayName;
        if (const auto workspace = manager.FindById(currentWorkspaceId))
        {
            state.Exists = true;
            state.DisplayName = workspace->Name;
            state.TabRowName = workspace->Name;
            state.BackgroundColor = workspace->BackgroundColor;
            state.Icon = workspace->Icon;
            state.Locked = workspace->Locked;
        }

        return state;
    }

    WorkspaceStartupState PrepareWorkspaceStartupState(const std::wstring_view workspaceId, const WorkspaceManager& manager)
    {
        WorkspaceStartupState state;
        if (const auto workspace = manager.FindById(workspaceId))
        {
            for (const auto& nodeId : VisibleWorkspaceNodeIds(*workspace))
            {
                const auto nodeIndex = FindWorkspaceNodeIndexById(*workspace, nodeId);
                if (!nodeIndex)
                {
                    continue;
                }
                const auto& node = workspace->Nodes.at(*nodeIndex);
                const auto commandCount = std::max<size_t>(1, node.Commands.size());
                for (size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
                {
                    state.PendingNodeIds.emplace_back(nodeId);
                    state.PendingNodeInputVisibility.emplace_back(node.ShowInputPanel);
                }
            }
        }
        return state;
    }

    LoadedWorkspaceFlyoutState LoadWorkspaceFlyoutState(const std::wstring_view currentWorkspaceId)
    {
        auto manager = WorkspaceManager::Load();
        const auto windowState = WorkspaceStateManager::LoadRuntime();
        return LoadedWorkspaceFlyoutState{
            .Manager = manager,
            .FlyoutState = BuildWorkspaceFlyoutState(currentWorkspaceId, manager, windowState),
        };
    }

    LoadedWorkspaceOpenState LoadWorkspaceOpenState(const std::wstring_view workspaceId,
                                                    const bool openInNewWindow,
                                                    const std::wstring_view currentWorkspaceId,
                                                    const bool currentWorkspaceNeedsSave)
    {
        auto manager = WorkspaceManager::Load();
        const auto windowState = WorkspaceStateManager::LoadRuntime();
        return LoadedWorkspaceOpenState{
            .Manager = manager,
            .OpenPlan = PrepareWorkspaceForOpen(workspaceId,
                                                openInNewWindow,
                                                currentWorkspaceId,
                                                currentWorkspaceNeedsSave,
                                                manager,
                                                windowState),
        };
    }

    LoadedWorkspaceOpenExecutionState LoadWorkspaceOpenExecutionState(const std::wstring_view workspaceId,
                                                                      const bool openInNewWindow,
                                                                      const std::wstring_view currentWorkspaceId,
                                                                      const bool currentWorkspaceNeedsSave,
                                                                      const bool hasTabsToReplace)
    {
        auto loadedState = LoadWorkspaceOpenState(workspaceId,
                                                  openInNewWindow,
                                                  currentWorkspaceId,
                                                  currentWorkspaceNeedsSave);
        WorkspaceManager targetWorkspaceManager;
        targetWorkspaceManager.SetWorkspaces({ loadedState.OpenPlan.TargetWorkspace });
        const auto startupState = PrepareWorkspaceStartupState(loadedState.OpenPlan.TargetWorkspace.Id,
                                                               targetWorkspaceManager);
        const auto executionPlan = ResolveWorkspaceOpenExecutionPlan(loadedState.OpenPlan,
                                                                     !startupState.PendingNodeIds.empty(),
                                                                     hasTabsToReplace);
        return LoadedWorkspaceOpenExecutionState{
            .Manager = std::move(loadedState.Manager),
            .OpenPlan = std::move(loadedState.OpenPlan),
            .StartupState = startupState,
            .ExecutionPlan = executionPlan,
        };
    }

    std::optional<Workspace> LoadWorkspaceDefinition(const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            return std::nullopt;
        }

        const auto manager = WorkspaceManager::Load();
        if (const auto workspace = manager.FindById(workspaceId))
        {
            return *workspace;
        }

        return std::nullopt;
    }

    std::optional<Workspace> LoadResolvedWorkspaceDefinition(const std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace)
    {
        const auto manager = WorkspaceManager::Load();
        return ResolveWorkspaceDefinition(currentWorkspaceId, selectedWorkspace, manager);
    }

    std::optional<WorkspaceNode> LoadResolvedWorkspaceNode(const std::wstring_view currentWorkspaceId,
                                                           const std::optional<Workspace>& selectedWorkspace,
                                                           const std::wstring_view nodeId)
    {
        const auto manager = WorkspaceManager::Load();
        return ResolveCurrentWorkspaceNode(currentWorkspaceId, selectedWorkspace, manager, nodeId);
    }

    WorkspaceCurrentState LoadCurrentWorkspaceState(const std::wstring_view currentWorkspaceId,
                                                    const std::wstring_view defaultDisplayName,
                                                    const std::wstring_view unsavedTabRowName)
    {
        const auto manager = WorkspaceManager::Load();
        return ResolveWorkspaceCurrentState(currentWorkspaceId, manager, defaultDisplayName, unsavedTabRowName);
    }

    WorkspaceStartupState LoadWorkspaceStartupState(const std::wstring_view workspaceId)
    {
        const auto manager = WorkspaceManager::Load();
        return PrepareWorkspaceStartupState(workspaceId, manager);
    }

    WorkspaceFlyoutState BuildWorkspaceFlyoutState(const std::wstring_view currentWorkspaceId,
                                                   const WorkspaceManager& manager,
                                                   const WorkspaceStateManager& stateManager)
    {
        WorkspaceFlyoutState state;
        state.Entries.reserve(manager.Workspaces().size());
        for (const auto& workspace : manager.Workspaces())
        {
            state.Entries.emplace_back(WorkspaceFlyoutEntry{
                .Definition = workspace,
                .IsOpen = stateManager.FindOpenWorkspaceWindowId(workspace.Id).has_value(),
            });
        }
        state.CurrentWorkspaceExists = currentWorkspaceId.empty() || manager.FindById(currentWorkspaceId) != nullptr;
        return state;
    }
