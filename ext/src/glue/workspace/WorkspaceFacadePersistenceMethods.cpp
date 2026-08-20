    bool IsWorkspaceLocked(const std::wstring_view workspaceId)
    {
        return terminal::workspace::IsWorkspaceLocked(workspaceId);
    }

    bool SetWorkspaceLocked(WorkspaceManager& manager, const std::wstring_view workspaceId, const bool locked)
    {
        auto coreManager = _toCoreManager(manager);
        if (!terminal::workspace::SetWorkspaceLocked(coreManager, workspaceId, locked))
        {
            return false;
        }
        manager = _fromCoreManager(coreManager);
        return true;
    }

    bool PersistWorkspaceLockedState(const std::wstring_view workspaceId, const bool locked)
    {
        return terminal::workspace::PersistWorkspaceLockedState(workspaceId, locked);
    }

    bool SetWorkspaceNodeInputVisibility(WorkspaceManager& manager, const std::wstring_view workspaceId, const size_t nodeIndex, const bool showInputPanel)
    {
        auto coreManager = _toCoreManager(manager);
        if (!terminal::workspace::SetWorkspaceNodeInputVisibility(coreManager, workspaceId, nodeIndex, showInputPanel))
        {
            return false;
        }
        manager = _fromCoreManager(coreManager);
        return true;
    }

    bool RemoveWorkspaceDefinition(WorkspaceManager& manager, const std::wstring_view workspaceId, size_t* removedWorkspaceIndex)
    {
        auto coreManager = _toCoreManager(manager);
        if (!terminal::workspace::RemoveWorkspaceDefinition(coreManager, workspaceId, removedWorkspaceIndex))
        {
            return false;
        }
        manager = _fromCoreManager(coreManager);
        return true;
    }

    WorkspaceNodeMutationResult RemoveWorkspaceNode(WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view nodeId)
    {
        auto coreManager = _toCoreManager(manager);
        const auto result = terminal::workspace::RemoveWorkspaceNode(coreManager, workspaceId, nodeId);
        manager = _fromCoreManager(coreManager);
        return WorkspaceNodeMutationResult{
            .Disposition = static_cast<WorkspaceNodeMutationDisposition>(result.Disposition),
            .WorkspaceIndex = result.WorkspaceIndex,
            .NodeIndex = result.NodeIndex,
        };
    }

    void FinalizeWorkspaceManagerNames(WorkspaceManager& manager)
    {
        auto coreManager = _toCoreManager(manager);
        terminal::workspace::FinalizeWorkspaceManagerNames(coreManager);
        manager = _fromCoreManager(coreManager);
    }

    std::optional<std::wstring> RenameWorkspace(WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view newName)
    {
        auto coreManager = _toCoreManager(manager);
        const auto resolvedName = terminal::workspace::RenameWorkspace(coreManager, workspaceId, newName);
        manager = _fromCoreManager(coreManager);
        return resolvedName;
    }

    std::optional<uint64_t> FindPersistedOpenWorkspaceWindowId(const std::wstring_view workspaceId)
    {
        return terminal::workspace::FindPersistedOpenWorkspaceWindowId(workspaceId);
    }

    std::optional<PersistedWorkspaceRename> PersistWorkspaceRename(const std::wstring_view workspaceId, const std::wstring_view newName)
    {
        const auto result = terminal::workspace::PersistWorkspaceRename(workspaceId, newName);
        return result ? std::optional<PersistedWorkspaceRename>{ _fromCorePersistedWorkspaceRename(*result) } : std::nullopt;
    }

    std::optional<PersistedWorkspaceEditorSave> PersistWorkspaceEditorState(const WorkspaceManager& editorManager,
                                                                            const std::wstring_view currentWorkspaceId,
                                                                            const std::wstring_view lastOpenedWorkspaceId,
                                                                            const size_t fallbackSelectedWorkspaceIndex)
    {
        const auto result = terminal::workspace::PersistWorkspaceEditorState(_toCoreManager(editorManager),
                                                                             currentWorkspaceId,
                                                                             lastOpenedWorkspaceId,
                                                                             fallbackSelectedWorkspaceIndex);
        return result ? std::optional<PersistedWorkspaceEditorSave>{ _fromCorePersistedWorkspaceEditorSave(*result) } : std::nullopt;
    }

    WorkspaceCurrentIdChangePlan PrepareWorkspaceCurrentIdChange(const std::wstring_view previousWorkspaceId,
                                                                 const std::wstring_view nextWorkspaceId,
                                                                 const std::wstring_view lastWorkspaceId,
                                                                 const std::wstring_view currentBaselineWorkspaceId)
    {
        return _fromCoreCurrentIdChangePlan(terminal::workspace::PrepareWorkspaceCurrentIdChange(previousWorkspaceId,
                                                                                                 nextWorkspaceId,
                                                                                                 lastWorkspaceId,
                                                                                                 currentBaselineWorkspaceId));
    }

    WorkspaceWindowRefreshPlan PrepareWorkspaceWindowRefresh(const std::uint64_t windowId,
                                                             const std::wstring_view currentWorkspaceId)
    {
        return _fromCoreWindowRefreshPlan(terminal::workspace::PrepareWorkspaceWindowRefresh(windowId, currentWorkspaceId));
    }

    WorkspaceWindowRefreshPlan RefreshWorkspaceWindowState(const std::uint64_t windowId,
                                                           const std::wstring_view currentWorkspaceId)
    {
        return _fromCoreWindowRefreshPlan(terminal::workspace::RefreshWorkspaceWindowState(windowId, currentWorkspaceId));
    }

    void RemovePersistedWorkspaceWindowState(const uint64_t windowId)
    {
        std::ignore = terminal::workspace::WorkspaceStateManager::RemoveRuntimeWindowState(windowId);
    }

    void RefreshPersistedWorkspaceWindowState(const uint64_t windowId,
                                              const std::wstring_view processName,
                                              const std::wstring_view workspaceId)
    {
        std::ignore = terminal::workspace::WorkspaceStateManager::RefreshRuntimeWindowState(windowId, processName, workspaceId);
    }

    std::optional<WorkspaceEditorDefinitionAddResult> AddWorkspaceDefinition(WorkspaceManager& manager,
                                                                             const std::wstring_view generatedName,
                                                                             const std::optional<size_t> templateIndex)
    {
        auto coreManager = _toCoreManager(manager);
        const auto result = terminal::workspace::AddWorkspaceDefinition(coreManager, generatedName, templateIndex);
        manager = _fromCoreManager(coreManager);
        return result ? std::optional<WorkspaceEditorDefinitionAddResult>{ _fromCoreEditorDefinitionAddResult(*result) } : std::nullopt;
    }

    WorkspaceEditorNodeAddResult AddWorkspaceNode(WorkspaceManager& manager,
                                                  const size_t workspaceIndex,
                                                  const std::wstring_view generatedName,
                                                  const std::wstring_view defaultProfileGuid,
                                                  const std::wstring_view defaultProfileName)
    {
        auto coreManager = _toCoreManager(manager);
        const auto result = terminal::workspace::AddWorkspaceNode(coreManager,
                                                                  workspaceIndex,
                                                                  generatedName,
                                                                  defaultProfileGuid,
                                                                  defaultProfileName);
        manager = _fromCoreManager(coreManager);
        return _fromCoreEditorNodeAddResult(result);
    }

    std::optional<WorkspaceManager> PersistWorkspaceNodeInputVisibility(const WorkspaceManager& preferredManager,
                                                                        const std::wstring_view workspaceId,
                                                                        const size_t nodeIndex,
                                                                        const bool showInputPanel)
    {
        const auto result = terminal::workspace::PersistWorkspaceNodeInputVisibility(_toCoreManager(preferredManager),
                                                                                     workspaceId,
                                                                                     nodeIndex,
                                                                                     showInputPanel);
        return result ? std::optional<WorkspaceManager>{ _fromCoreManager(*result) } : std::nullopt;
    }

    std::optional<WorkspaceManager> PersistWorkspaceNodeOrder(const std::wstring_view workspaceId,
                                                              const std::vector<std::wstring>& orderedNodeIds)
    {
        const auto result = terminal::workspace::PersistWorkspaceNodeOrder(workspaceId, orderedNodeIds);
        return result ? std::optional<WorkspaceManager>{ _fromCoreManager(*result) } : std::nullopt;
    }
