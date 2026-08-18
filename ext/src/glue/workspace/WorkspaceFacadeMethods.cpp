    namespace
    {
        terminal::workspace::WorkspaceNode _toCoreNode(const WorkspaceNode& node)
        {
            terminal::workspace::WorkspaceNode core;
            core.Id = node.Id;
            core.Name = node.Name;
            core.ConnectionRef = node.ConnectionRef;
            core.ProfileGuid = node.ProfileGuid;
            core.ProfileName = node.ProfileName;
            core.Icon = node.Icon;
            core.TabColor = node.TabColor;
            core.ShowTab = node.ShowTab;
            core.StartupDirectory = node.StartupDirectory;
            core.StartupAction = node.StartupAction;
            core.OperatingSystem = node.OperatingSystem;
            core.ShellType = node.ShellType;
            core.ShowInputPanel = node.ShowInputPanel;
            core.UseNodeNameAsTabTitle = node.UseNodeNameAsTabTitle;
            return core;
        }

        WorkspaceNode _fromCoreNode(const terminal::workspace::WorkspaceNode& node)
        {
            WorkspaceNode wrapped;
            wrapped.Id = node.Id;
            wrapped.Name = node.Name;
            wrapped.ConnectionRef = node.ConnectionRef;
            wrapped.ProfileGuid = node.ProfileGuid;
            wrapped.ProfileName = node.ProfileName;
            wrapped.Icon = node.Icon;
            wrapped.TabColor = node.TabColor;
            wrapped.ShowTab = node.ShowTab;
            wrapped.StartupDirectory = node.StartupDirectory;
            wrapped.StartupAction = node.StartupAction;
            wrapped.OperatingSystem = node.OperatingSystem;
            wrapped.ShellType = node.ShellType;
            wrapped.ShowInputPanel = node.ShowInputPanel;
            wrapped.UseNodeNameAsTabTitle = node.UseNodeNameAsTabTitle;
            return wrapped;
        }

        terminal::workspace::Workspace _toCoreWorkspace(const Workspace& workspace)
        {
            terminal::workspace::Workspace core;
            core.Id = workspace.Id;
            core.Name = workspace.Name;
            core.Description = workspace.Description;
            core.BackgroundColor = workspace.BackgroundColor;
            core.Icon = workspace.Icon;
            core.Locked = workspace.Locked;
            core.NewNodeDefaults = _toCoreNode(workspace.NewNodeDefaults);
            core.TabOrder = workspace.TabOrder;
            core.Nodes.reserve(workspace.Nodes.size());
            for (const auto& node : workspace.Nodes)
            {
                core.Nodes.emplace_back(_toCoreNode(node));
            }
            return core;
        }

        Workspace _fromCoreWorkspace(const terminal::workspace::Workspace& workspace)
        {
            Workspace wrapped;
            wrapped.Id = workspace.Id;
            wrapped.Name = workspace.Name;
            wrapped.Description = workspace.Description;
            wrapped.BackgroundColor = workspace.BackgroundColor;
            wrapped.Icon = workspace.Icon;
            wrapped.Locked = workspace.Locked;
            wrapped.NewNodeDefaults = _fromCoreNode(workspace.NewNodeDefaults);
            wrapped.TabOrder = workspace.TabOrder;
            wrapped.Nodes.reserve(workspace.Nodes.size());
            for (const auto& node : workspace.Nodes)
            {
                wrapped.Nodes.emplace_back(_fromCoreNode(node));
            }
            return wrapped;
        }

        terminal::workspace::WorkspaceStateWindow _toCoreWindow(const WorkspaceStateWindow& window)
        {
            terminal::workspace::WorkspaceStateWindow core;
            core.WindowId = window.WindowId;
            core.WindowName = window.WindowName;
            core.WorkspaceId = window.WorkspaceId;
            return core;
        }

        WorkspaceStateWindow _fromCoreWindow(const terminal::workspace::WorkspaceStateWindow& window)
        {
            WorkspaceStateWindow wrapped;
            wrapped.WindowId = window.WindowId;
            wrapped.WindowName = window.WindowName;
            wrapped.WorkspaceId = window.WorkspaceId;
            return wrapped;
        }

        terminal::workspace::WorkspaceRuntimeMetadata _toCoreRuntimeMetadata(const WorkspaceRuntimeMetadata& metadata)
        {
            return terminal::workspace::WorkspaceRuntimeMetadata{
                .OperatingSystem = metadata.OperatingSystem,
                .ShellType = metadata.ShellType,
            };
        }

        WorkspaceRuntimeMetadata _fromCoreRuntimeMetadata(const terminal::workspace::WorkspaceRuntimeMetadata& metadata)
        {
            return WorkspaceRuntimeMetadata{
                .OperatingSystem = metadata.OperatingSystem,
                .ShellType = metadata.ShellType,
            };
        }

        terminal::workspace::WorkspaceRuntimeLaunchState _toCoreRuntimeLaunchState(const WorkspaceRuntimeLaunchState& state)
        {
            return terminal::workspace::WorkspaceRuntimeLaunchState{
                .ExplicitCommandline = state.ExplicitCommandline,
                .StartingDirectory = state.StartingDirectory,
                .OperatingSystem = state.OperatingSystem,
                .ShellType = state.ShellType,
                .IsSshTransport = state.IsSshTransport,
                .HasSshTtyOption = state.HasSshTtyOption,
            };
        }

        WorkspaceRuntimeLaunchState _fromCoreRuntimeLaunchState(const terminal::workspace::WorkspaceRuntimeLaunchState& state)
        {
            return WorkspaceRuntimeLaunchState{
                .ExplicitCommandline = state.ExplicitCommandline,
                .StartingDirectory = state.StartingDirectory,
                .OperatingSystem = state.OperatingSystem,
                .ShellType = state.ShellType,
                .IsSshTransport = state.IsSshTransport,
                .HasSshTtyOption = state.HasSshTtyOption,
            };
        }

        terminal::workspace::WorkspaceNodeLaunchResolutionInput _toCoreNodeLaunchResolutionInput(const WorkspaceNodeLaunchResolutionInput& input)
        {
            return terminal::workspace::WorkspaceNodeLaunchResolutionInput{
                .PersistedNode = input.PersistedNode ? std::optional<terminal::workspace::WorkspaceNode>{ _toCoreNode(*input.PersistedNode) } : std::nullopt,
                .ObservedStartupAction = input.ObservedStartupAction,
                .ObservedWorkingDirectory = input.ObservedWorkingDirectory,
                .ObservedOperatingSystem = input.ObservedOperatingSystem,
                .ObservedShellType = input.ObservedShellType,
                .RuntimeStartupAction = input.RuntimeStartupAction,
                .RuntimeExplicitCommandline = input.RuntimeExplicitCommandline,
                .RuntimeStartingDirectory = input.RuntimeStartingDirectory,
                .RuntimeOperatingSystem = input.RuntimeOperatingSystem,
                .RuntimeShellType = input.RuntimeShellType,
                .ProfileSource = input.ProfileSource,
                .ProfileCommandline = input.ProfileCommandline,
                .TerminalCommandline = input.TerminalCommandline,
                .TerminalStartingDirectory = input.TerminalStartingDirectory,
            };
        }

        WorkspaceNodeLaunchResolution _fromCoreNodeLaunchResolution(const terminal::workspace::WorkspaceNodeLaunchResolution& resolution)
        {
            return WorkspaceNodeLaunchResolution{
                .StartupAction = resolution.StartupAction,
                .StartingDirectory = resolution.StartingDirectory,
                .OperatingSystem = resolution.OperatingSystem,
                .ShellType = resolution.ShellType,
            };
        }

        terminal::workspace::WorkspaceNodeLaunchResolution _toCoreNodeLaunchResolution(const WorkspaceNodeLaunchResolution& resolution)
        {
            return terminal::workspace::WorkspaceNodeLaunchResolution{
                .StartupAction = resolution.StartupAction,
                .StartingDirectory = resolution.StartingDirectory,
                .OperatingSystem = resolution.OperatingSystem,
                .ShellType = resolution.ShellType,
            };
        }

        terminal::workspace::WorkspaceTrackedDirectoryInput _toCoreTrackedDirectoryInput(const WorkspaceTrackedDirectoryInput& input)
        {
            return terminal::workspace::WorkspaceTrackedDirectoryInput{
                .ReportedWorkingDirectory = input.ReportedWorkingDirectory,
                .ProcessWorkingDirectory = input.ProcessWorkingDirectory,
                .RuntimeStartingDirectory = input.RuntimeStartingDirectory,
                .RuntimeOperatingSystem = input.RuntimeOperatingSystem,
                .RuntimeShellType = input.RuntimeShellType,
                .IsSshTransport = input.IsSshTransport,
            };
        }

        terminal::workspace::WorkspaceLiveTabSnapshot _toCoreLiveTabSnapshot(const WorkspaceLiveTabSnapshot& tab)
        {
            return terminal::workspace::WorkspaceLiveTabSnapshot{
                .LoadsWorkspaceNode = tab.LoadsWorkspaceNode,
                .RuntimeNodeId = tab.RuntimeNodeId,
            };
        }

        terminal::workspace::WorkspaceLiveTabCaptureState _toCoreLiveTabCaptureState(const WorkspaceLiveTabCaptureState& state)
        {
            return terminal::workspace::WorkspaceLiveTabCaptureState{
                .PersistedNode = state.PersistedNode ? std::optional<terminal::workspace::WorkspaceNode>{ _toCoreNode(*state.PersistedNode) } : std::nullopt,
                .LiveTabTitle = state.LiveTabTitle,
                .StartupTabTitle = state.StartupTabTitle,
                .GeneratedNodeName = state.GeneratedNodeName,
                .ProfileGuid = state.ProfileGuid,
                .ProfileName = state.ProfileName,
                .LaunchResolution = _toCoreNodeLaunchResolution(state.LaunchResolution),
                .ShowInputPanel = state.ShowInputPanel,
                .TabColor = state.TabColor,
            };
        }

        terminal::workspace::WorkspaceManager _toCoreManager(const WorkspaceManager& manager)
        {
            terminal::workspace::WorkspaceManager core;
            std::vector<terminal::workspace::Workspace> workspaces;
            workspaces.reserve(manager.Workspaces().size());
            for (const auto& workspace : manager.Workspaces())
            {
                workspaces.emplace_back(_toCoreWorkspace(workspace));
            }
            core.SetWorkspaces(std::move(workspaces));
            return core;
        }

        WorkspaceManager _fromCoreManager(const terminal::workspace::WorkspaceManager& manager)
        {
            WorkspaceManager wrapped;
            std::vector<Workspace> workspaces;
            workspaces.reserve(manager.Workspaces().size());
            for (const auto& workspace : manager.Workspaces())
            {
                workspaces.emplace_back(_fromCoreWorkspace(workspace));
            }
            wrapped.SetWorkspaces(std::move(workspaces));
            return wrapped;
        }

        terminal::workspace::WorkspaceStateManager _toCoreStateManager(const WorkspaceStateManager& manager)
        {
            terminal::workspace::WorkspaceStateManager core;
            std::vector<terminal::workspace::WorkspaceStateWindow> windows;
            windows.reserve(manager.Windows().size());
            for (const auto& window : manager.Windows())
            {
                windows.emplace_back(_toCoreWindow(window));
            }
            core.SetWindows(std::move(windows));
            return core;
        }

        WorkspaceStateManager _fromCoreStateManager(const terminal::workspace::WorkspaceStateManager& manager)
        {
            WorkspaceStateManager wrapped;
            std::vector<WorkspaceStateWindow> windows;
            windows.reserve(manager.Windows().size());
            for (const auto& window : manager.Windows())
            {
                windows.emplace_back(_fromCoreWindow(window));
            }
            wrapped.SetWindows(std::move(windows));
            return wrapped;
        }
    }

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

    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromProfile(const std::wstring_view source)
    {
        return _fromCoreRuntimeMetadata(terminal::workspace::InferWorkspaceRuntimeMetadataFromProfile(source));
    }

    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromCommandline(const std::wstring_view value)
    {
        return _fromCoreRuntimeMetadata(terminal::workspace::InferWorkspaceRuntimeMetadataFromCommandline(value));
    }

    bool IsWorkspaceSshCommandline(const std::wstring_view value)
    {
        return terminal::workspace::IsWorkspaceSshCommandline(value);
    }

    bool HasWorkspaceSshTtyOption(const std::wstring_view commandline)
    {
        return terminal::workspace::HasWorkspaceSshTtyOption(commandline);
    }

    bool IsWorkspaceSshTransport(const std::wstring_view profileSource,
                                 const std::wstring_view profileCommandline,
                                 const std::wstring_view commandline)
    {
        return terminal::workspace::IsWorkspaceSshTransport(profileSource, profileCommandline, commandline);
    }

    WorkspaceRuntimeLaunchState PrepareWorkspaceRuntimeLaunchState(const std::wstring_view startingDirectory,
                                                                   const std::wstring_view profileSource,
                                                                   const std::wstring_view profileCommandline,
                                                                   const std::wstring_view commandline)
    {
        return _fromCoreRuntimeLaunchState(terminal::workspace::PrepareWorkspaceRuntimeLaunchState(startingDirectory,
                                                                                                    profileSource,
                                                                                                    profileCommandline,
                                                                                                    commandline));
    }

    WorkspaceNodeLaunchResolution ResolveWorkspaceNodeLaunchResolution(const WorkspaceNodeLaunchResolutionInput& input)
    {
        return _fromCoreNodeLaunchResolution(terminal::workspace::ResolveWorkspaceNodeLaunchResolution(_toCoreNodeLaunchResolutionInput(input)));
    }

    std::wstring ResolveTrackedWorkspaceDirectory(const WorkspaceTrackedDirectoryInput& input)
    {
        return terminal::workspace::ResolveTrackedWorkspaceDirectory(_toCoreTrackedDirectoryInput(input));
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

    std::optional<winrt::Windows::UI::Color> ResolveWorkspaceNodeTabColor(const Workspace& workspace,
                                                                           const size_t nodeIndex,
                                                                           const Model::CascadiaSettings& settings)
    {
        if (nodeIndex >= workspace.Nodes.size())
        {
            return std::nullopt;
        }

        const auto& node = workspace.Nodes.at(nodeIndex);
        if (const auto explicitColor = _parseColor(node.TabColor))
        {
            return explicitColor;
        }

        const auto profile = _resolveNodeProfile(node, settings);
        const auto profileKey = _resolveNodeProfileKey(node, profile, settings);

        size_t occurrenceIndex = 0;
        for (size_t i = 0; i < nodeIndex; ++i)
        {
            const auto& candidate = workspace.Nodes.at(i);
            const auto candidateProfile = _resolveNodeProfile(candidate, settings);
            if (_resolveNodeProfileKey(candidate, candidateProfile, settings) == profileKey)
            {
                ++occurrenceIndex;
            }
        }

        return _resolveDuplicateNodeColor(_resolveBaseNodeColor(node, profile, settings), occurrenceIndex);
    }

    namespace
    {
        struct ResolvedWorkspaceStartupNode
        {
            size_t NodeIndex{};
            Model::Profile Profile;
        };

        std::vector<ResolvedWorkspaceStartupNode> _resolveWorkspaceStartupNodes(const Workspace& workspace,
                                                                                const Model::CascadiaSettings& settings)
        {
            const auto orderedNodeIds = VisibleWorkspaceNodeIds(workspace);
            std::vector<ResolvedWorkspaceStartupNode> resolvedNodes;
            resolvedNodes.reserve(orderedNodeIds.size());
            for (const auto& nodeId : orderedNodeIds)
            {
                const auto nodeIndex = FindWorkspaceNodeIndexById(workspace, nodeId);
                if (!nodeIndex)
                {
                    continue;
                }

                const auto profile = _resolveNodeProfile(workspace.Nodes.at(*nodeIndex), settings);
                if (!profile)
                {
                    continue;
                }

                resolvedNodes.emplace_back(ResolvedWorkspaceStartupNode{
                    .NodeIndex = *nodeIndex,
                    .Profile = profile,
                });
            }
            return resolvedNodes;
        }
    }

    WorkspaceStartupState ResolveWorkspaceStartupState(const Workspace& workspace, const Model::CascadiaSettings& settings)
    {
        WorkspaceStartupState state;
        const auto resolvedNodes = _resolveWorkspaceStartupNodes(workspace, settings);
        state.PendingNodeIds.reserve(resolvedNodes.size());
        state.PendingNodeInputVisibility.reserve(resolvedNodes.size());
        for (const auto& resolvedNode : resolvedNodes)
        {
            const auto& node = workspace.Nodes.at(resolvedNode.NodeIndex);
            state.PendingNodeIds.emplace_back(node.Id);
            state.PendingNodeInputVisibility.emplace_back(node.ShowInputPanel);
        }
        return state;
    }

    WorkspaceOpenPlan PrepareWorkspaceForOpen(const std::wstring_view workspaceId,
                                              const bool openInNewWindow,
                                              const std::wstring_view currentWorkspaceId,
                                              const bool currentWorkspaceNeedsSave,
                                              const WorkspaceManager& manager,
                                              const WorkspaceStateManager& stateManager)
    {
        const auto plan = terminal::workspace::PrepareWorkspaceForOpen(workspaceId,
                                                                       openInNewWindow,
                                                                       currentWorkspaceId,
                                                                       currentWorkspaceNeedsSave,
                                                                       _toCoreManager(manager),
                                                                       _toCoreStateManager(stateManager));
        return WorkspaceOpenPlan{
            .Disposition = static_cast<WorkspaceOpenDisposition>(plan.Disposition),
            .TargetWorkspace = _fromCoreWorkspace(plan.TargetWorkspace),
            .ExistingWindowId = plan.ExistingWindowId,
            .ConfirmSaveCurrentWorkspace = plan.ConfirmSaveCurrentWorkspace,
            .PendingNodeIds = std::move(plan.PendingNodeIds),
            .PendingNodeInputVisibility = std::move(plan.PendingNodeInputVisibility),
        };
    }

    WorkspaceOpenExecutionPlan ResolveWorkspaceOpenExecutionPlan(const WorkspaceOpenPlan& openPlan,
                                                                 const bool hasStartupActions,
                                                                 const bool hasTabsToReplace)
    {
        const auto plan = terminal::workspace::ResolveWorkspaceOpenExecutionPlan(terminal::workspace::WorkspaceOpenPlan{
                                                                                     .Disposition = static_cast<terminal::workspace::WorkspaceOpenDisposition>(openPlan.Disposition),
                                                                                     .TargetWorkspace = _toCoreWorkspace(openPlan.TargetWorkspace),
                                                                                     .ExistingWindowId = openPlan.ExistingWindowId,
                                                                                     .ConfirmSaveCurrentWorkspace = openPlan.ConfirmSaveCurrentWorkspace,
                                                                                     .PendingNodeIds = openPlan.PendingNodeIds,
                                                                                     .PendingNodeInputVisibility = openPlan.PendingNodeInputVisibility,
                                                                                 },
                                                                                 hasStartupActions,
                                                                                 hasTabsToReplace);
        return WorkspaceOpenExecutionPlan{
            .Disposition = static_cast<WorkspaceOpenExecutionDisposition>(plan.Disposition),
            .ExistingWindowId = plan.ExistingWindowId,
            .ConfirmSaveCurrentWorkspace = plan.ConfirmSaveCurrentWorkspace,
            .SetLastOpenedWorkspaceId = plan.SetLastOpenedWorkspaceId,
            .UpdatePendingWorkspaceLaunch = plan.UpdatePendingWorkspaceLaunch,
            .SetSaveBaseline = plan.SetSaveBaseline,
            .SetCurrentWorkspaceBeforeActions = plan.SetCurrentWorkspaceBeforeActions,
            .ReplacePendingNodeQueues = plan.ReplacePendingNodeQueues,
            .FocusActiveContentAfterActions = plan.FocusActiveContentAfterActions,
            .RemoveCapturedTabsAfterActions = plan.RemoveCapturedTabsAfterActions,
            .SetCurrentWorkspaceAfterActions = plan.SetCurrentWorkspaceAfterActions,
        };
    }

    WorkspaceSshStartupPlan PrepareSshStartupPlan(const std::wstring_view pendingStartupAction,
                                                  const std::wstring_view startingDirectory,
                                                  const std::wstring_view operatingSystem,
                                                  const std::wstring_view shellType,
                                                  const std::optional<WorkspaceNode>& workspaceNode)
    {
        const auto plan = terminal::workspace::PrepareSshStartupPlan(pendingStartupAction,
                                                                     startingDirectory,
                                                                     operatingSystem,
                                                                     shellType,
                                                                     workspaceNode ? std::optional<terminal::workspace::WorkspaceNode>{ _toCoreNode(*workspaceNode) } : std::nullopt);
        return WorkspaceSshStartupPlan{
            .StartupAction = std::move(plan.StartupAction),
            .StartingDirectory = std::move(plan.StartingDirectory),
            .OperatingSystem = std::move(plan.OperatingSystem),
            .ShellType = std::move(plan.ShellType),
            .DeferredStartupInputs = std::move(plan.DeferredStartupInputs),
            .StartupInputPending = plan.StartupInputPending,
            .StartupInputDispatched = plan.StartupInputDispatched,
        };
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceLiveTabCaptureState& state)
    {
        return _fromCoreNode(terminal::workspace::BuildWorkspaceCapturedNode(_toCoreLiveTabCaptureState(state)));
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

    void EnsureWorkspaceNodeTabColors(Workspace& workspace, const Model::CascadiaSettings& settings)
    {
        // Deliberately use a high-contrast palette at the workspace scope.
        // A color is never reused while an unused candidate exists, so a new
        // node and the "reselect" action both remain visually distinguishable.
        static constexpr std::array<std::wstring_view, 12> palette{
            L"#C50F1F", L"#0063B1", L"#0F7B0F", L"#CA5010",
            L"#8E562E", L"#744DA9", L"#038387", L"#881798",
            L"#498205", L"#515C6B", L"#567C73", L"#7A7574",
        };
        std::unordered_set<std::wstring> usedColors;
        usedColors.reserve(workspace.Nodes.size());
        for (size_t i = 0; i < workspace.Nodes.size(); ++i)
        {
            const auto& color = workspace.Nodes.at(i).TabColor;
            if (!color.empty())
            {
                usedColors.emplace(color);
                continue;
            }

            const auto paletteIt = std::find_if(palette.begin(), palette.end(), [&](const auto candidate) {
                return !usedColors.contains(std::wstring{ candidate });
            });
            if (paletteIt != palette.end())
            {
                workspace.Nodes.at(i).TabColor = std::wstring{ *paletteIt };
                usedColors.emplace(workspace.Nodes.at(i).TabColor);
            }
            else if (const auto fallback = ResolveWorkspaceNodeTabColor(workspace, i, settings))
            {
                // More nodes than palette entries: keep Terminal's derived
                // color as a deterministic fallback.
                workspace.Nodes.at(i).TabColor = _colorToString(*fallback);
            }
        }
    }

    std::vector<Model::ActionAndArgs> WorkspaceManager::BuildStartupActions(const Workspace& workspace, const Model::CascadiaSettings& settings) const
    {
        std::vector<Model::ActionAndArgs> actions;
        for (const auto& resolvedNode : _resolveWorkspaceStartupNodes(workspace, settings))
        {
            const auto nodeIndex = resolvedNode.NodeIndex;
            const auto& node = workspace.Nodes.at(nodeIndex);
            Model::NewTerminalArgs terminalArgs;

            if (!node.Name.empty())
            {
                terminalArgs.TabTitle(node.Name);
            }
            terminalArgs.SuppressApplicationTitle(node.UseNodeNameAsTabTitle);
            const auto profile = resolvedNode.Profile;

            const auto isSshTransport = _isSshTransportNode(node, profile);
            if (!node.StartupDirectory.empty() && !isSshTransport)
            {
                terminalArgs.StartingDirectory(node.StartupDirectory);
            }

            terminalArgs.Profile(Utils::GuidToString(profile.Guid()));
            if (const auto tabColor = ResolveWorkspaceNodeTabColor(workspace, nodeIndex, settings))
            {
                terminalArgs.TabColor(*tabColor);
            }

            Model::ActionAndArgs newTabAction;
            newTabAction.Action(ShortcutAction::NewTab);
            newTabAction.Args(Model::NewTabArgs{ terminalArgs });
            actions.emplace_back(std::move(newTabAction));

            auto startupInput = isSshTransport ? _buildSshStartupInput(node) : std::wstring{};
            startupInput = _appendStartupCommand(std::move(startupInput), node.StartupAction);
            if (!startupInput.empty())
            {
                Model::ActionAndArgs startupAction;
                startupAction.Action(ShortcutAction::SendInput);
                startupAction.Args(Model::SendInputArgs{ winrt::hstring{ startupInput } });
                actions.emplace_back(std::move(startupAction));
            }
        }

        return actions;
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

    std::optional<uint64_t> WorkspaceStateManager::FindOpenWorkspaceWindowId(const std::wstring_view workspaceId) const noexcept
    {
        return _toCoreStateManager(*this).FindOpenWorkspaceWindowId(workspaceId);
    }
