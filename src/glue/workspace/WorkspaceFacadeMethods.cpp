    std::filesystem::path WorkspaceManager::DefaultPath()
    {
        return terminal::workspace::WorkspaceManager::DefaultPath();
    }

    WorkspaceManager WorkspaceManager::Load()
    {
        return _fromCoreManager(terminal::workspace::WorkspaceManager::Load());
    }

    WorkspaceManager WorkspaceManager::LoadFromPath(const std::filesystem::path& path)
    {
        return _fromCoreManager(terminal::workspace::WorkspaceManager::LoadFromPath(path));
    }

    bool WorkspaceManager::Save() const
    {
        return _toCoreManager(*this).Save();
    }

    bool WorkspaceManager::SaveToPath(const std::filesystem::path& path) const
    {
        return _toCoreManager(*this).SaveToPath(path);
    }

    const Workspace* WorkspaceManager::FindById(const std::wstring_view id) const noexcept
    {
        const auto it = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == id;
        });
        return it == _workspaces.end() ? nullptr : &*it;
    }

    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes)
    {
        auto coreWorkspace = _toCoreWorkspace(workspace);
        std::vector<terminal::workspace::WorkspaceNode> coreNodes;
        coreNodes.reserve(orderedVisibleNodes.size());
        for (const auto& node : orderedVisibleNodes)
        {
            coreNodes.emplace_back(_toCoreNode(node));
        }

        if (!terminal::workspace::ApplyVisibleWorkspaceNodeOrder(coreWorkspace, coreNodes))
        {
            return false;
        }

        workspace = _fromCoreWorkspace(coreWorkspace);
        return true;
    }

    bool MoveWorkspaceManagerVisibleNode(Workspace& workspace, const size_t nodeIndex, const int offset)
    {
        auto coreWorkspace = _toCoreWorkspace(workspace);
        if (!terminal::workspace::MoveWorkspaceManagerVisibleNode(coreWorkspace, nodeIndex, offset))
        {
            return false;
        }

        workspace = _fromCoreWorkspace(coreWorkspace);
        return true;
    }

    bool ApplyWorkspaceManagerNodeTemplate(Workspace& workspace, const size_t templateIndex, const size_t newNodeIndex)
    {
        auto coreWorkspace = _toCoreWorkspace(workspace);
        if (!terminal::workspace::ApplyWorkspaceManagerNodeTemplate(coreWorkspace, templateIndex, newNodeIndex))
        {
            return false;
        }

        workspace = _fromCoreWorkspace(coreWorkspace);
        return true;
    }

    std::optional<Workspace> PrepareWorkspaceForCapture(const std::optional<Workspace>& currentWorkspaceDefinition,
                                                        std::vector<WorkspaceNode> capturedNodes)
    {
        std::vector<terminal::workspace::WorkspaceNode> coreNodes;
        coreNodes.reserve(capturedNodes.size());
        for (const auto& node : capturedNodes)
        {
            coreNodes.emplace_back(_toCoreNode(node));
        }

        const auto workspace = terminal::workspace::PrepareWorkspaceForCapture(currentWorkspaceDefinition ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*currentWorkspaceDefinition) } : std::nullopt,
                                                                               std::move(coreNodes));
        return workspace ? std::optional<Workspace>{ _fromCoreWorkspace(*workspace) } : std::nullopt;
    }

    std::optional<Workspace> ResolveWorkspaceDefinition(const std::wstring_view currentWorkspaceId,
                                                        const std::optional<Workspace>& selectedWorkspace,
                                                        const WorkspaceManager& manager)
    {
        const auto workspace = terminal::workspace::ResolveWorkspaceDefinition(currentWorkspaceId,
                                                                               selectedWorkspace ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*selectedWorkspace) } : std::nullopt,
                                                                               _toCoreManager(manager));
        return workspace ? std::optional<Workspace>{ _fromCoreWorkspace(*workspace) } : std::nullopt;
    }

    bool WorkspaceNodeLoadsTab(const WorkspaceNode& node) noexcept
    {
        return terminal::workspace::WorkspaceNodeLoadsTab(_toCoreNode(node));
    }

    std::optional<WorkspaceNode> FindWorkspaceNodeById(const Workspace& workspace, const std::wstring_view nodeId)
    {
        const auto node = terminal::workspace::FindWorkspaceNodeById(_toCoreWorkspace(workspace), nodeId);
        return node ? std::optional<WorkspaceNode>{ _fromCoreNode(*node) } : std::nullopt;
    }

    std::optional<WorkspaceNode> FindWorkspaceNodeById(const WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view nodeId)
    {
        const auto node = terminal::workspace::FindWorkspaceNodeById(_toCoreManager(manager), workspaceId, nodeId);
        return node ? std::optional<WorkspaceNode>{ _fromCoreNode(*node) } : std::nullopt;
    }

    std::optional<WorkspaceNode> ResolveCurrentWorkspaceNode(const std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace,
                                                             const WorkspaceManager& manager,
                                                             const std::wstring_view nodeId)
    {
        const auto node = terminal::workspace::ResolveCurrentWorkspaceNode(currentWorkspaceId,
                                                                           selectedWorkspace ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*selectedWorkspace) } : std::nullopt,
                                                                           _toCoreManager(manager),
                                                                           nodeId);
        return node ? std::optional<WorkspaceNode>{ _fromCoreNode(*node) } : std::nullopt;
    }

    std::optional<size_t> FindWorkspaceNodeIndexById(const Workspace& workspace, const std::wstring_view nodeId)
    {
        return terminal::workspace::FindWorkspaceNodeIndexById(_toCoreWorkspace(workspace), nodeId);
    }

    std::optional<size_t> FindWorkspaceVisibleNodeIndex(const Workspace& workspace, const size_t visibleOrdinal)
    {
        return terminal::workspace::FindWorkspaceVisibleNodeIndex(_toCoreWorkspace(workspace), visibleOrdinal);
    }

    std::optional<size_t> ResolveWorkspaceBackedNodeIndex(const std::optional<Workspace>& workspaceDefinition,
                                                          const std::wstring_view runtimeNodeId,
                                                          const std::optional<size_t> visibleOrdinal)
    {
        return terminal::workspace::ResolveWorkspaceBackedNodeIndex(workspaceDefinition ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*workspaceDefinition) } : std::nullopt,
                                                                    runtimeNodeId,
                                                                    visibleOrdinal);
    }

    std::optional<WorkspaceNode> ResolveWorkspaceBackedNode(const std::optional<Workspace>& workspaceDefinition,
                                                            const std::wstring_view runtimeNodeId,
                                                            const std::optional<size_t> visibleOrdinal)
    {
        const auto node = terminal::workspace::ResolveWorkspaceBackedNode(workspaceDefinition ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*workspaceDefinition) } : std::nullopt,
                                                                          runtimeNodeId,
                                                                          visibleOrdinal);
        return node ? std::optional<WorkspaceNode>{ _fromCoreNode(*node) } : std::nullopt;
    }

    std::vector<std::wstring> VisibleWorkspaceNodeIds(const Workspace& workspace)
    {
        return terminal::workspace::VisibleWorkspaceNodeIds(_toCoreWorkspace(workspace));
    }

    std::vector<bool> VisibleWorkspaceNodeInputVisibility(const Workspace& workspace)
    {
        return terminal::workspace::VisibleWorkspaceNodeInputVisibility(_toCoreWorkspace(workspace));
    }

    int32_t WorkspaceManagerNavSelectionForWorkspace(const size_t workspaceIndex) noexcept
    {
        return terminal::workspace::WorkspaceManagerNavSelectionForWorkspace(workspaceIndex);
    }

    int32_t WorkspaceManagerNavSelectionForWorkspaceNode(const size_t workspaceIndex, const size_t nodeIndex) noexcept
    {
        return terminal::workspace::WorkspaceManagerNavSelectionForWorkspaceNode(workspaceIndex, nodeIndex);
    }

    std::optional<size_t> ResolveWorkspaceIndexFromManagerNavSelection(const int32_t navSelection) noexcept
    {
        return terminal::workspace::ResolveWorkspaceIndexFromManagerNavSelection(navSelection);
    }

    std::optional<size_t> ResolveWorkspaceNodeIndexFromManagerNavSelection(const int32_t navSelection) noexcept
    {
        return terminal::workspace::ResolveWorkspaceNodeIndexFromManagerNavSelection(navSelection);
    }

    int32_t ResolveWorkspaceManagerNavSelectionForEditor(const size_t workspaceCount, const size_t selectedWorkspaceIndex) noexcept
    {
        return terminal::workspace::ResolveWorkspaceManagerNavSelectionForEditor(workspaceCount, selectedWorkspaceIndex);
    }

    int32_t ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(const int32_t previousNavSelection,
                                                                     const std::wstring_view selectedWorkspaceId,
                                                                     const std::wstring_view removedWorkspaceId,
                                                                     const size_t removedWorkspaceIndex,
                                                                     const size_t remainingWorkspaceCount) noexcept
    {
        return terminal::workspace::ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(previousNavSelection,
                                                                                             selectedWorkspaceId,
                                                                                             removedWorkspaceId,
                                                                                             removedWorkspaceIndex,
                                                                                             remainingWorkspaceCount);
    }

    int32_t ResolveWorkspaceManagerNavSelectionAfterNodeRemoval(const int32_t previousNavSelection,
                                                                const std::wstring_view selectedWorkspaceId,
                                                                const std::wstring_view workspaceId,
                                                                const size_t selectedWorkspaceIndex,
                                                                const size_t removedNodeIndex) noexcept
    {
        return terminal::workspace::ResolveWorkspaceManagerNavSelectionAfterNodeRemoval(previousNavSelection,
                                                                                        selectedWorkspaceId,
                                                                                        workspaceId,
                                                                                        selectedWorkspaceIndex,
                                                                                        removedNodeIndex);
    }

    size_t ResolveWorkspaceEditorSelectedIndex(const WorkspaceManager& manager,
                                               const std::wstring_view selectedWorkspaceId,
                                               const std::wstring_view currentWorkspaceId,
                                               const size_t fallbackIndex) noexcept
    {
        return terminal::workspace::ResolveWorkspaceEditorSelectedIndex(_toCoreManager(manager),
                                                                        selectedWorkspaceId,
                                                                        currentWorkspaceId,
                                                                        fallbackIndex);
    }

    bool IsWorkspaceDirty(const Workspace& capturedWorkspace,
                          const std::wstring_view currentWorkspaceId,
                          const std::optional<Workspace>& baselineWorkspace,
                          const std::optional<Workspace>& persistedWorkspace)
    {
        return terminal::workspace::IsWorkspaceDirty(_toCoreWorkspace(capturedWorkspace),
                                                     currentWorkspaceId,
                                                     baselineWorkspace ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*baselineWorkspace) } : std::nullopt,
                                                     persistedWorkspace ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*persistedWorkspace) } : std::nullopt);
    }

    bool WorkspaceManager::ReorderWorkspaceNodes(const std::wstring_view workspaceId, const std::vector<std::wstring>& orderedNodeIds)
    {
        auto core = _toCoreManager(*this);
        if (!core.ReorderWorkspaceNodes(workspaceId, orderedNodeIds))
        {
            return false;
        }

        SetWorkspaces(_fromCoreManager(core).Workspaces());
        return true;
    }

    std::vector<Workspace>& WorkspaceManager::Workspaces() noexcept
    {
        return _workspaces;
    }

    const std::vector<Workspace>& WorkspaceManager::Workspaces() const noexcept
    {
        return _workspaces;
    }

    void WorkspaceManager::SetWorkspaces(std::vector<Workspace> workspaces)
    {
        _workspaces = std::move(workspaces);
    }

    std::wstring SanitizeWorkspaceDirectoryName(const std::wstring_view value, const std::wstring_view fallback) noexcept
    {
        return terminal::workspace::SanitizeWorkspaceDirectoryName(value, fallback);
    }

    std::wstring NormalizeWorkspaceColor(const std::wstring_view color) noexcept
    {
        return terminal::workspace::NormalizeWorkspaceColor(color);
    }

    std::wstring PickUnusedWorkspaceColor(const std::vector<Workspace>& workspaces)
    {
        std::vector<terminal::workspace::Workspace> coreWorkspaces;
        coreWorkspaces.reserve(workspaces.size());
        for (const auto& workspace : workspaces)
        {
            coreWorkspaces.emplace_back(_toCoreWorkspace(workspace));
        }
        return terminal::workspace::PickUnusedWorkspaceColor(coreWorkspaces);
    }

    std::wstring MakeUniquePersistedName(const std::wstring_view baseName, std::unordered_set<std::wstring>& usedNames)
    {
        return terminal::workspace::MakeUniquePersistedName(baseName, usedNames);
    }

    void NormalizeWorkspacePersistableNames(Workspace& workspace)
    {
        auto coreWorkspace = _toCoreWorkspace(workspace);
        terminal::workspace::NormalizeWorkspacePersistableNames(coreWorkspace);
        workspace = _fromCoreWorkspace(coreWorkspace);
    }

    bool WorkspaceNodeEquivalent(const WorkspaceNode& lhs, const WorkspaceNode& rhs)
    {
        return terminal::workspace::WorkspaceNodeEquivalent(_toCoreNode(lhs), _toCoreNode(rhs));
    }

    bool WorkspaceLayoutEquivalent(const Workspace& lhs, const Workspace& rhs)
    {
        return terminal::workspace::WorkspaceLayoutEquivalent(_toCoreWorkspace(lhs), _toCoreWorkspace(rhs));
    }

    WorkspaceSavePlan PrepareWorkspaceForSave(const Workspace& capturedWorkspace,
                                              const std::vector<Workspace>& existingWorkspaces,
                                              const std::wstring_view targetWorkspaceId,
                                              const std::wstring_view explicitWorkspaceName,
                                              const std::wstring_view fallbackWindowName,
                                              const std::wstring_view fallbackTargetName,
                                              const std::wstring_view fallbackSingleTabTitle,
                                              const std::wstring_view generatedFallbackName)
    {
        std::vector<terminal::workspace::Workspace> coreExistingWorkspaces;
        coreExistingWorkspaces.reserve(existingWorkspaces.size());
        for (const auto& workspace : existingWorkspaces)
        {
            coreExistingWorkspaces.emplace_back(_toCoreWorkspace(workspace));
        }

        const auto plan = terminal::workspace::PrepareWorkspaceForSave(_toCoreWorkspace(capturedWorkspace),
                                                                       coreExistingWorkspaces,
                                                                       targetWorkspaceId,
                                                                       explicitWorkspaceName,
                                                                       fallbackWindowName,
                                                                       fallbackTargetName,
                                                                       fallbackSingleTabTitle,
                                                                       generatedFallbackName);

        WorkspaceSavePlan wrapped;
        wrapped.Workspaces.reserve(plan.Workspaces.size());
        for (const auto& workspace : plan.Workspaces)
        {
            wrapped.Workspaces.emplace_back(_fromCoreWorkspace(workspace));
        }
        wrapped.SavedWorkspace = _fromCoreWorkspace(plan.SavedWorkspace);
        wrapped.SavedWorkspaceIndex = plan.SavedWorkspaceIndex;
        return wrapped;
    }

    std::wstring ResolveWorkspaceSaveTargetId(const std::wstring_view currentWorkspaceId, const std::wstring_view lastWorkspaceId, const WorkspaceManager& manager)
    {
        return terminal::workspace::ResolveWorkspaceSaveTargetId(currentWorkspaceId, lastWorkspaceId, _toCoreManager(manager));
    }

    std::wstring ResolveWorkspaceSaveTargetName(const std::wstring_view currentWorkspaceId, const std::wstring_view lastWorkspaceId, const WorkspaceManager& manager)
    {
        return terminal::workspace::ResolveWorkspaceSaveTargetName(currentWorkspaceId, lastWorkspaceId, _toCoreManager(manager));
    }

    std::wstring SuggestWorkspaceSaveName(const std::wstring_view resolvedTargetName,
                                          const std::wstring_view windowName,
                                          const std::wstring_view singleTabTitle,
                                          const size_t workspaceCount,
                                          const std::wstring_view generatedFallbackName)
    {
        return terminal::workspace::SuggestWorkspaceSaveName(resolvedTargetName, windowName, singleTabTitle, workspaceCount, generatedFallbackName);
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceLiveTabCaptureState& state)
    {
        return _fromCoreNode(terminal::workspace::BuildWorkspaceCapturedNode(_toCoreLiveTabCaptureState(state)));
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceCapturedNodeInput& input)
    {
        return _fromCoreNode(terminal::workspace::BuildWorkspaceCapturedNode(_toCoreCapturedNodeInput(input)));
    }

    WorkspaceCapturedNodeInput BuildWorkspaceCapturedNodeInput(const WorkspaceCapturedNodePlanInput& input)
    {
        const auto coreInput = terminal::workspace::BuildWorkspaceCapturedNodeInput(_toCoreCapturedNodePlanInput(input));
        return WorkspaceCapturedNodeInput{
            .LaunchInput = WorkspaceNodeLaunchResolutionInput{
                .PersistedNode = coreInput.LaunchInput.PersistedNode ? std::optional<WorkspaceNode>{ _fromCoreNode(*coreInput.LaunchInput.PersistedNode) } : std::nullopt,
                .ObservedStartupAction = coreInput.LaunchInput.ObservedStartupAction,
                .ObservedWorkingDirectory = coreInput.LaunchInput.ObservedWorkingDirectory,
                .ObservedOperatingSystem = coreInput.LaunchInput.ObservedOperatingSystem,
                .ObservedShellType = coreInput.LaunchInput.ObservedShellType,
                .RuntimeStartupAction = coreInput.LaunchInput.RuntimeStartupAction,
                .RuntimeExplicitCommandline = coreInput.LaunchInput.RuntimeExplicitCommandline,
                .RuntimeStartingDirectory = coreInput.LaunchInput.RuntimeStartingDirectory,
                .RuntimeOperatingSystem = coreInput.LaunchInput.RuntimeOperatingSystem,
                .RuntimeShellType = coreInput.LaunchInput.RuntimeShellType,
                .ProfileSource = coreInput.LaunchInput.ProfileSource,
                .ProfileCommandline = coreInput.LaunchInput.ProfileCommandline,
                .TerminalCommandline = coreInput.LaunchInput.TerminalCommandline,
                .TerminalStartingDirectory = coreInput.LaunchInput.TerminalStartingDirectory,
            },
            .CaptureState = WorkspaceLiveTabCaptureState{
                .PersistedNode = coreInput.CaptureState.PersistedNode ? std::optional<WorkspaceNode>{ _fromCoreNode(*coreInput.CaptureState.PersistedNode) } : std::nullopt,
                .LiveTabTitle = coreInput.CaptureState.LiveTabTitle,
                .StartupTabTitle = coreInput.CaptureState.StartupTabTitle,
                .GeneratedNodeName = coreInput.CaptureState.GeneratedNodeName,
                .ProfileGuid = coreInput.CaptureState.ProfileGuid,
                .ProfileName = coreInput.CaptureState.ProfileName,
                .LaunchResolution = _fromCoreNodeLaunchResolution(coreInput.CaptureState.LaunchResolution),
                .ShowInputPanel = coreInput.CaptureState.ShowInputPanel,
                .TabColor = coreInput.CaptureState.TabColor,
            },
        };
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceCapturedNodePlanInput& input)
    {
        return _fromCoreNode(terminal::workspace::BuildWorkspaceCapturedNode(_toCoreCapturedNodePlanInput(input)));
    }

    std::optional<size_t> ResolveWorkspaceBackedTabIndex(const std::optional<Workspace>& workspaceDefinition,
                                                         const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                         const size_t targetTabIndex)
    {
        std::vector<terminal::workspace::WorkspaceLiveTabSnapshot> coreTabs;
        coreTabs.reserve(tabs.size());
        for (const auto& tab : tabs)
        {
            coreTabs.emplace_back(_toCoreLiveTabSnapshot(tab));
        }

        return terminal::workspace::ResolveWorkspaceBackedTabIndex(workspaceDefinition ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*workspaceDefinition) } : std::nullopt,
                                                                   coreTabs,
                                                                   targetTabIndex);
    }

    std::optional<WorkspaceNode> ResolveWorkspaceBackedTabNode(const std::optional<Workspace>& workspaceDefinition,
                                                               const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                               const size_t targetTabIndex)
    {
        std::vector<terminal::workspace::WorkspaceLiveTabSnapshot> coreTabs;
        coreTabs.reserve(tabs.size());
        for (const auto& tab : tabs)
        {
            coreTabs.emplace_back(_toCoreLiveTabSnapshot(tab));
        }

        const auto node = terminal::workspace::ResolveWorkspaceBackedTabNode(workspaceDefinition ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*workspaceDefinition) } : std::nullopt,
                                                                             coreTabs,
                                                                             targetTabIndex);
        return node ? std::optional<WorkspaceNode>{ _fromCoreNode(*node) } : std::nullopt;
    }

    std::optional<size_t> FindWorkspaceBackedTabSnapshotIndex(const std::optional<Workspace>& workspaceDefinition,
                                                              const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                              const size_t nodeIndex)
    {
        std::vector<terminal::workspace::WorkspaceLiveTabSnapshot> coreTabs;
        coreTabs.reserve(tabs.size());
        for (const auto& tab : tabs)
        {
            coreTabs.emplace_back(_toCoreLiveTabSnapshot(tab));
        }

        return terminal::workspace::FindWorkspaceBackedTabSnapshotIndex(workspaceDefinition ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*workspaceDefinition) } : std::nullopt,
                                                                        coreTabs,
                                                                        nodeIndex);
    }

    std::optional<size_t> LoadResolvedWorkspaceBackedTabIndex(const std::wstring_view currentWorkspaceId,
                                                              const std::optional<Workspace>& selectedWorkspace,
                                                              const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                              const size_t targetTabIndex)
    {
        return ResolveWorkspaceBackedTabIndex(LoadResolvedWorkspaceDefinition(currentWorkspaceId, selectedWorkspace),
                                              tabs,
                                              targetTabIndex);
    }

    std::optional<WorkspaceNode> LoadResolvedWorkspaceBackedTabNode(const std::wstring_view currentWorkspaceId,
                                                                    const std::optional<Workspace>& selectedWorkspace,
                                                                    const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                                    const size_t targetTabIndex)
    {
        return ResolveWorkspaceBackedTabNode(LoadResolvedWorkspaceDefinition(currentWorkspaceId, selectedWorkspace),
                                             tabs,
                                             targetTabIndex);
    }

    std::optional<size_t> FindResolvedWorkspaceBackedTabSnapshotIndex(const std::wstring_view currentWorkspaceId,
                                                                      const std::optional<Workspace>& selectedWorkspace,
                                                                      const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                                      const size_t nodeIndex)
    {
        return FindWorkspaceBackedTabSnapshotIndex(LoadResolvedWorkspaceDefinition(currentWorkspaceId, selectedWorkspace),
                                                   tabs,
                                                   nodeIndex);
    }

    std::filesystem::path WorkspaceStateManager::DefaultPath()
    {
        return terminal::workspace::WorkspaceStateManager::DefaultPath();
    }

    WorkspaceStateManager WorkspaceStateManager::Load()
    {
        return _fromCoreStateManager(terminal::workspace::WorkspaceStateManager::Load());
    }

    WorkspaceStateManager WorkspaceStateManager::LoadFromPath(const std::filesystem::path& path)
    {
        return _fromCoreStateManager(terminal::workspace::WorkspaceStateManager::LoadFromPath(path));
    }

    WorkspaceStateManager WorkspaceStateManager::LoadRuntime()
    {
        return _fromCoreStateManager(terminal::workspace::WorkspaceStateManager::LoadRuntime());
    }

    uint64_t WorkspaceStateManager::RuntimeHeartbeatIntervalMs() noexcept
    {
        return terminal::workspace::WorkspaceStateManager::RuntimeHeartbeatIntervalMs();
    }

    bool WorkspaceStateManager::RemoveRuntimeWindowState(const uint64_t windowId)
    {
        return terminal::workspace::WorkspaceStateManager::RemoveRuntimeWindowState(windowId);
    }

    bool WorkspaceStateManager::RefreshRuntimeWindowState(const uint64_t windowId,
                                                          const std::wstring_view windowName,
                                                          const std::wstring_view workspaceId)
    {
        return terminal::workspace::WorkspaceStateManager::RefreshRuntimeWindowState(windowId, windowName, workspaceId);
    }

    bool WorkspaceStateManager::Save() const
    {
        return _toCoreStateManager(*this).Save();
    }

    bool WorkspaceStateManager::SaveToPath(const std::filesystem::path& path) const
    {
        return _toCoreStateManager(*this).SaveToPath(path);
    }

    const std::vector<WorkspaceStateWindow>& WorkspaceStateManager::Windows() const noexcept
    {
        return _windows;
    }

    void WorkspaceStateManager::SetWindows(std::vector<WorkspaceStateWindow> windows)
    {
        _windows = std::move(windows);
    }

    void WorkspaceStateManager::UpsertWindow(WorkspaceStateWindow window)
    {
        const auto it = std::find_if(_windows.begin(), _windows.end(), [&](const auto& candidate) {
            return candidate.WindowId == window.WindowId;
        });

        if (it != _windows.end())
        {
            *it = std::move(window);
        }
        else
        {
            _windows.emplace_back(std::move(window));
        }
    }

    void WorkspaceStateManager::RemoveWindow(const uint64_t windowId) noexcept
    {
        _windows.erase(std::remove_if(_windows.begin(), _windows.end(), [&](const auto& window) {
            return window.WindowId == windowId;
        }), _windows.end());
    }

    void WorkspaceStateManager::UpdateWindowState(const uint64_t windowId, const std::wstring_view windowName, const std::wstring_view workspaceId)
    {
        auto core = _toCoreStateManager(*this);
        core.UpdateWindowState(windowId, windowName, workspaceId);
        SetWindows(_fromCoreStateManager(core).Windows());
    }

    bool WorkspaceStateManager::HasOpenWorkspace(const std::wstring_view workspaceId) const noexcept
    {
        return _toCoreStateManager(*this).HasOpenWorkspace(workspaceId);
    }

    std::optional<uint64_t> WorkspaceStateManager::FindOpenWorkspaceWindowId(const std::wstring_view workspaceId) const noexcept
    {
        return _toCoreStateManager(*this).FindOpenWorkspaceWindowId(workspaceId);
    }
