    WorkspaceEditorLoadState LoadWorkspaceEditorState(const std::wstring_view selectedWorkspaceId,
                                                      const std::wstring_view currentWorkspaceId,
                                                      const size_t fallbackIndex)
    {
        const auto state = terminal::workspace::LoadWorkspaceEditorState(selectedWorkspaceId, currentWorkspaceId, fallbackIndex);
        return WorkspaceEditorLoadState{
            .Manager = _fromCoreManager(state.Manager),
            .SelectedWorkspaceIndex = state.SelectedWorkspaceIndex,
        };
    }

    WorkspaceEditorSavePlan PrepareWorkspaceEditorForSave(WorkspaceManager& editedManager,
                                                          const WorkspaceManager& persistedManager,
                                                          const std::wstring_view currentWorkspaceId,
                                                          const std::wstring_view lastOpenedWorkspaceId,
                                                          const size_t fallbackSelectedWorkspaceIndex)
    {
        auto coreEditedManager = _toCoreManager(editedManager);
        const auto plan = terminal::workspace::PrepareWorkspaceEditorForSave(coreEditedManager,
                                                                             _toCoreManager(persistedManager),
                                                                             currentWorkspaceId,
                                                                             lastOpenedWorkspaceId,
                                                                             fallbackSelectedWorkspaceIndex);
        editedManager = _fromCoreManager(coreEditedManager);
        return WorkspaceEditorSavePlan{
            .ResolvedCurrentWorkspaceId = std::move(plan.ResolvedCurrentWorkspaceId),
            .CurrentWorkspaceExists = plan.CurrentWorkspaceExists,
            .LastOpenedWorkspaceExists = plan.LastOpenedWorkspaceExists,
            .SelectedWorkspaceIndex = plan.SelectedWorkspaceIndex,
        };
    }

    std::optional<WorkspaceEditorDefinitionRemovalPlan> PrepareWorkspaceDefinitionRemoval(WorkspaceManager& manager,
                                                                                          const std::wstring_view workspaceId,
                                                                                          const std::wstring_view selectedWorkspaceId,
                                                                                          const std::wstring_view currentWorkspaceId,
                                                                                          const size_t fallbackSelectedWorkspaceIndex,
                                                                                          const int32_t previousNavSelection,
                                                                                          const std::wstring_view lastOpenedWorkspaceId)
    {
        auto coreManager = _toCoreManager(manager);
        const auto plan = terminal::workspace::PrepareWorkspaceDefinitionRemoval(coreManager,
                                                                                 workspaceId,
                                                                                 selectedWorkspaceId,
                                                                                 currentWorkspaceId,
                                                                                 fallbackSelectedWorkspaceIndex,
                                                                                 previousNavSelection,
                                                                                 lastOpenedWorkspaceId);
        manager = _fromCoreManager(coreManager);
        if (!plan.has_value())
        {
            return std::nullopt;
        }

        return WorkspaceEditorDefinitionRemovalPlan{
            .NavSelection = plan->NavSelection,
            .RemovedCurrentWorkspace = plan->RemovedCurrentWorkspace,
            .LastOpenedWorkspaceExists = plan->LastOpenedWorkspaceExists,
            .SelectedWorkspaceIndex = plan->SelectedWorkspaceIndex,
        };
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
        auto coreManager = _toCoreManager(manager);
        const auto plan = terminal::workspace::PrepareWorkspaceNodeRemoval(coreManager,
                                                                           workspaceId,
                                                                           nodeId,
                                                                           selectedWorkspaceId,
                                                                           currentWorkspaceId,
                                                                           fallbackSelectedWorkspaceIndex,
                                                                           previousNavSelection,
                                                                           lastOpenedWorkspaceId);
        manager = _fromCoreManager(coreManager);
        return WorkspaceEditorNodeRemovalPlan{
            .Disposition = static_cast<WorkspaceNodeMutationDisposition>(plan.Disposition),
            .NavSelection = plan.NavSelection,
            .RemovedCurrentWorkspace = plan.RemovedCurrentWorkspace,
            .LastOpenedWorkspaceExists = plan.LastOpenedWorkspaceExists,
            .SelectedWorkspaceIndex = plan.SelectedWorkspaceIndex,
        };
    }

    WorkspaceCurrentState ResolveWorkspaceCurrentState(const std::wstring_view currentWorkspaceId,
                                                       const WorkspaceManager& manager,
                                                       const std::wstring_view defaultDisplayName,
                                                       const std::wstring_view unsavedTabRowName)
    {
        const auto state = terminal::workspace::ResolveWorkspaceCurrentState(currentWorkspaceId,
                                                                             _toCoreManager(manager),
                                                                             defaultDisplayName,
                                                                             unsavedTabRowName);
        return WorkspaceCurrentState{
            .Exists = state.Exists,
            .DisplayName = std::move(state.DisplayName),
            .TabRowName = std::move(state.TabRowName),
            .BackgroundColor = std::move(state.BackgroundColor),
            .Icon = std::move(state.Icon),
            .Locked = state.Locked,
        };
    }

    WorkspaceStartupState PrepareWorkspaceStartupState(const std::wstring_view workspaceId, const WorkspaceManager& manager)
    {
        const auto state = terminal::workspace::PrepareWorkspaceStartupState(workspaceId, _toCoreManager(manager));
        return WorkspaceStartupState{
            .PendingNodeIds = std::move(state.PendingNodeIds),
            .PendingNodeInputVisibility = std::move(state.PendingNodeInputVisibility),
        };
    }

    WorkspaceFlyoutState BuildWorkspaceFlyoutState(const std::wstring_view currentWorkspaceId,
                                                   const WorkspaceManager& manager,
                                                   const WorkspaceStateManager& stateManager)
    {
        const auto state = terminal::workspace::BuildWorkspaceFlyoutState(currentWorkspaceId, _toCoreManager(manager), _toCoreStateManager(stateManager));
        WorkspaceFlyoutState wrapped;
        wrapped.CurrentWorkspaceExists = state.CurrentWorkspaceExists;
        wrapped.Entries.reserve(state.Entries.size());
        for (const auto& entry : state.Entries)
        {
            wrapped.Entries.emplace_back(WorkspaceFlyoutEntry{
                .Definition = _fromCoreWorkspace(entry.Definition),
                .IsOpen = entry.IsOpen,
            });
        }
        return wrapped;
    }

    LoadedWorkspaceFlyoutState LoadWorkspaceFlyoutState(const std::wstring_view currentWorkspaceId)
    {
        return _fromCoreLoadedFlyoutState(terminal::workspace::LoadWorkspaceFlyoutState(currentWorkspaceId));
    }

    LoadedWorkspaceOpenState LoadWorkspaceOpenState(const std::wstring_view workspaceId,
                                                    const bool openInNewWindow,
                                                    const std::wstring_view currentWorkspaceId,
                                                    const bool currentWorkspaceNeedsSave)
    {
        return _fromCoreLoadedOpenState(terminal::workspace::LoadWorkspaceOpenState(workspaceId,
                                                                                     openInNewWindow,
                                                                                     currentWorkspaceId,
                                                                                     currentWorkspaceNeedsSave));
    }

    LoadedWorkspaceOpenExecutionState LoadWorkspaceOpenExecutionState(const std::wstring_view workspaceId,
                                                                      const bool openInNewWindow,
                                                                      const std::wstring_view currentWorkspaceId,
                                                                      const bool currentWorkspaceNeedsSave,
                                                                      const bool hasTabsToReplace,
                                                                      const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
    {
        (void)settings;
        const auto loadedState = terminal::workspace::LoadWorkspaceOpenExecutionState(workspaceId,
                                                                                      openInNewWindow,
                                                                                      currentWorkspaceId,
                                                                                      currentWorkspaceNeedsSave,
                                                                                      hasTabsToReplace);
        return LoadedWorkspaceOpenExecutionState{
            .Manager = _fromCoreManager(loadedState.Manager),
            .OpenPlan = _fromCoreOpenPlan(loadedState.OpenPlan),
            .StartupState = WorkspaceStartupState{
                .PendingNodeIds = loadedState.StartupState.PendingNodeIds,
                .PendingNodeInputVisibility = loadedState.StartupState.PendingNodeInputVisibility,
            },
            .ExecutionPlan = WorkspaceOpenExecutionPlan{
                .Disposition = static_cast<WorkspaceOpenExecutionDisposition>(loadedState.ExecutionPlan.Disposition),
                .ExistingWindowId = loadedState.ExecutionPlan.ExistingWindowId,
                .ConfirmSaveCurrentWorkspace = loadedState.ExecutionPlan.ConfirmSaveCurrentWorkspace,
                .SetLastOpenedWorkspaceId = loadedState.ExecutionPlan.SetLastOpenedWorkspaceId,
                .UpdatePendingWorkspaceLaunch = loadedState.ExecutionPlan.UpdatePendingWorkspaceLaunch,
                .SetSaveBaseline = loadedState.ExecutionPlan.SetSaveBaseline,
                .SetCurrentWorkspaceBeforeActions = loadedState.ExecutionPlan.SetCurrentWorkspaceBeforeActions,
                .ReplacePendingNodeQueues = loadedState.ExecutionPlan.ReplacePendingNodeQueues,
                .FocusActiveContentAfterActions = loadedState.ExecutionPlan.FocusActiveContentAfterActions,
                .RemoveCapturedTabsAfterActions = loadedState.ExecutionPlan.RemoveCapturedTabsAfterActions,
                .SetCurrentWorkspaceAfterActions = loadedState.ExecutionPlan.SetCurrentWorkspaceAfterActions,
            },
        };
    }

    std::optional<Workspace> LoadWorkspaceDefinition(const std::wstring_view workspaceId)
    {
        const auto workspace = terminal::workspace::LoadWorkspaceDefinition(workspaceId);
        return workspace ? std::optional<Workspace>{ _fromCoreWorkspace(*workspace) } : std::nullopt;
    }

    std::optional<Workspace> LoadResolvedWorkspaceDefinition(const std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace)
    {
        const auto workspace = terminal::workspace::LoadResolvedWorkspaceDefinition(currentWorkspaceId,
                                                                                    selectedWorkspace ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*selectedWorkspace) } : std::nullopt);
        return workspace ? std::optional<Workspace>{ _fromCoreWorkspace(*workspace) } : std::nullopt;
    }

    std::optional<WorkspaceNode> LoadResolvedWorkspaceNode(const std::wstring_view currentWorkspaceId,
                                                           const std::optional<Workspace>& selectedWorkspace,
                                                           const std::wstring_view nodeId)
    {
        const auto node = terminal::workspace::LoadResolvedWorkspaceNode(currentWorkspaceId,
                                                                         selectedWorkspace ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*selectedWorkspace) } : std::nullopt,
                                                                         nodeId);
        return node ? std::optional<WorkspaceNode>{ _fromCoreNode(*node) } : std::nullopt;
    }

    WorkspaceCurrentState LoadCurrentWorkspaceState(const std::wstring_view currentWorkspaceId,
                                                    const std::wstring_view defaultDisplayName,
                                                    const std::wstring_view unsavedTabRowName)
    {
        const auto state = terminal::workspace::LoadCurrentWorkspaceState(currentWorkspaceId, defaultDisplayName, unsavedTabRowName);
        return WorkspaceCurrentState{
            .Exists = state.Exists,
            .DisplayName = state.DisplayName,
            .TabRowName = state.TabRowName,
            .BackgroundColor = state.BackgroundColor,
            .Icon = state.Icon,
            .Locked = state.Locked,
        };
    }

    WorkspaceSaveTargetState LoadWorkspaceSaveTargetState(const std::wstring_view currentWorkspaceId,
                                                          const std::wstring_view lastWorkspaceId)
    {
        const auto manager = WorkspaceManager::Load();
        return WorkspaceSaveTargetState{
            .Id = ResolveWorkspaceSaveTargetId(currentWorkspaceId, lastWorkspaceId, manager),
            .Name = ResolveWorkspaceSaveTargetName(currentWorkspaceId, lastWorkspaceId, manager),
        };
    }

    WorkspaceStartupState LoadWorkspaceStartupState(const std::wstring_view workspaceId)
    {
        const auto state = terminal::workspace::LoadWorkspaceStartupState(workspaceId);
        return WorkspaceStartupState{
            .PendingNodeIds = std::move(state.PendingNodeIds),
            .PendingNodeInputVisibility = std::move(state.PendingNodeInputVisibility),
        };
    }
