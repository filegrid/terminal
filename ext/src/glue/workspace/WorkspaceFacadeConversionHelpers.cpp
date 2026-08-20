    namespace
    {
        WorkspaceFlyoutState _fromCoreFlyoutState(const terminal::workspace::WorkspaceFlyoutState& state);
        WorkspaceOpenPlan _fromCoreOpenPlan(const terminal::workspace::WorkspaceOpenPlan& plan);
        WorkspaceEditorSavePlan _fromCoreEditorSavePlan(const terminal::workspace::WorkspaceEditorSavePlan& plan);
        WorkspaceEditorDefinitionAddResult _fromCoreEditorDefinitionAddResult(const terminal::workspace::WorkspaceEditorDefinitionAddResult& result);
        WorkspaceEditorNodeAddResult _fromCoreEditorNodeAddResult(const terminal::workspace::WorkspaceEditorNodeAddResult& result);

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
            core.ProcessId = window.ProcessId;
            core.ProcessName = window.ProcessName;
            core.WindowName = window.WindowName;
            core.WorkspaceId = window.WorkspaceId;
            return core;
        }

        WorkspaceStateWindow _fromCoreWindow(const terminal::workspace::WorkspaceStateWindow& window)
        {
            WorkspaceStateWindow wrapped;
            wrapped.WindowId = window.WindowId;
            wrapped.ProcessId = window.ProcessId;
            wrapped.ProcessName = window.ProcessName;
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

        terminal::workspace::WorkspaceNodeLaunchResolutionPlanInput _toCoreNodeLaunchResolutionPlanInput(const WorkspaceNodeLaunchResolutionPlanInput& input)
        {
            return terminal::workspace::WorkspaceNodeLaunchResolutionPlanInput{
                .PersistedNode = input.PersistedNode ? std::optional<terminal::workspace::WorkspaceNode>{ _toCoreNode(*input.PersistedNode) } : std::nullopt,
                .ObservedStartupAction = input.ObservedStartupAction,
                .ObservedWorkingDirectory = input.ObservedWorkingDirectory,
                .TrackedWorkingDirectory = input.TrackedWorkingDirectory,
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

        terminal::workspace::WorkspaceCapturedNodeInput _toCoreCapturedNodeInput(const WorkspaceCapturedNodeInput& input)
        {
            return terminal::workspace::WorkspaceCapturedNodeInput{
                .LaunchInput = _toCoreNodeLaunchResolutionInput(input.LaunchInput),
                .CaptureState = _toCoreLiveTabCaptureState(input.CaptureState),
            };
        }

        terminal::workspace::WorkspaceCapturedNodePlanInput _toCoreCapturedNodePlanInput(const WorkspaceCapturedNodePlanInput& input)
        {
            return terminal::workspace::WorkspaceCapturedNodePlanInput{
                .PersistedNode = input.PersistedNode ? std::optional<terminal::workspace::WorkspaceNode>{ _toCoreNode(*input.PersistedNode) } : std::nullopt,
                .ProfileSource = input.ProfileSource,
                .ProfileCommandline = input.ProfileCommandline,
                .TerminalCommandline = input.TerminalCommandline,
                .TerminalStartingDirectory = input.TerminalStartingDirectory,
                .ObservedStartupAction = input.ObservedStartupAction,
                .ObservedWorkingDirectory = input.ObservedWorkingDirectory,
                .TrackedWorkingDirectory = input.TrackedWorkingDirectory,
                .ObservedOperatingSystem = input.ObservedOperatingSystem,
                .ObservedShellType = input.ObservedShellType,
                .RuntimeStartupAction = input.RuntimeStartupAction,
                .RuntimeExplicitCommandline = input.RuntimeExplicitCommandline,
                .RuntimeStartingDirectory = input.RuntimeStartingDirectory,
                .RuntimeOperatingSystem = input.RuntimeOperatingSystem,
                .RuntimeShellType = input.RuntimeShellType,
                .LiveTabTitle = input.LiveTabTitle,
                .StartupTabTitle = input.StartupTabTitle,
                .GeneratedNodeName = input.GeneratedNodeName,
                .ProfileGuid = input.ProfileGuid,
                .ProfileName = input.ProfileName,
                .ShowInputPanel = input.ShowInputPanel,
                .TabColor = input.TabColor,
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

        LoadedWorkspaceFlyoutState _fromCoreLoadedFlyoutState(const terminal::workspace::LoadedWorkspaceFlyoutState& state)
        {
            LoadedWorkspaceFlyoutState wrapped;
            wrapped.Manager = _fromCoreManager(state.Manager);
            wrapped.FlyoutState = _fromCoreFlyoutState(state.FlyoutState);
            return wrapped;
        }

        LoadedWorkspaceOpenState _fromCoreLoadedOpenState(const terminal::workspace::LoadedWorkspaceOpenState& state)
        {
            LoadedWorkspaceOpenState wrapped;
            wrapped.Manager = _fromCoreManager(state.Manager);
            wrapped.OpenPlan = _fromCoreOpenPlan(state.OpenPlan);
            return wrapped;
        }

        PersistedWorkspaceRename _fromCorePersistedWorkspaceRename(const terminal::workspace::PersistedWorkspaceRename& state)
        {
            PersistedWorkspaceRename wrapped;
            wrapped.Manager = _fromCoreManager(state.Manager);
            wrapped.ResolvedWorkspaceName = state.ResolvedWorkspaceName;
            return wrapped;
        }

        PersistedWorkspaceEditorSave _fromCorePersistedWorkspaceEditorSave(const terminal::workspace::PersistedWorkspaceEditorSave& state)
        {
            PersistedWorkspaceEditorSave wrapped;
            wrapped.Manager = _fromCoreManager(state.Manager);
            wrapped.SavePlan = _fromCoreEditorSavePlan(state.SavePlan);
            return wrapped;
        }

        WorkspaceFlyoutState _fromCoreFlyoutState(const terminal::workspace::WorkspaceFlyoutState& state)
        {
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

        WorkspaceOpenPlan _fromCoreOpenPlan(const terminal::workspace::WorkspaceOpenPlan& plan)
        {
            WorkspaceOpenPlan wrapped;
            wrapped.Disposition = static_cast<WorkspaceOpenDisposition>(plan.Disposition);
            wrapped.TargetWorkspace = _fromCoreWorkspace(plan.TargetWorkspace);
            wrapped.ExistingWindowId = plan.ExistingWindowId;
            wrapped.ConfirmSaveCurrentWorkspace = plan.ConfirmSaveCurrentWorkspace;
            wrapped.PendingNodeIds = plan.PendingNodeIds;
            wrapped.PendingNodeInputVisibility = plan.PendingNodeInputVisibility;
            return wrapped;
        }

        WorkspaceEditorSavePlan _fromCoreEditorSavePlan(const terminal::workspace::WorkspaceEditorSavePlan& plan)
        {
            return WorkspaceEditorSavePlan{
                .ResolvedCurrentWorkspaceId = plan.ResolvedCurrentWorkspaceId,
                .CurrentWorkspaceExists = plan.CurrentWorkspaceExists,
                .LastOpenedWorkspaceExists = plan.LastOpenedWorkspaceExists,
                .SelectedWorkspaceIndex = plan.SelectedWorkspaceIndex,
            };
        }

        WorkspaceEditorDefinitionAddResult _fromCoreEditorDefinitionAddResult(const terminal::workspace::WorkspaceEditorDefinitionAddResult& result)
        {
            return WorkspaceEditorDefinitionAddResult{
                .AddedWorkspaceIndex = result.AddedWorkspaceIndex,
            };
        }

        WorkspaceEditorNodeAddResult _fromCoreEditorNodeAddResult(const terminal::workspace::WorkspaceEditorNodeAddResult& result)
        {
            return WorkspaceEditorNodeAddResult{
                .Added = result.Added,
            };
        }

        WorkspaceCurrentIdChangePlan _fromCoreCurrentIdChangePlan(const terminal::workspace::WorkspaceCurrentIdChangePlan& plan)
        {
            return WorkspaceCurrentIdChangePlan{
                .LastWorkspaceId = plan.LastWorkspaceId,
                .ResetSaveBaseline = plan.ResetSaveBaseline,
                .StartHeartbeat = plan.StartHeartbeat,
            };
        }

        WorkspaceWindowRefreshPlan _fromCoreWindowRefreshPlan(const terminal::workspace::WorkspaceWindowRefreshPlan& plan)
        {
            return WorkspaceWindowRefreshPlan{
                .SkipRefresh = plan.SkipRefresh,
                .ClearPendingWorkspaceLaunch = plan.ClearPendingWorkspaceLaunch,
                .Refreshed = plan.Refreshed,
                .ProcessId = plan.ProcessId,
                .WorkspaceId = plan.WorkspaceId,
            };
        }

        terminal::workspace::WorkspaceNodeRuntimeRegistrationInput _toCoreRuntimeRegistrationInput(const WorkspaceNodeRuntimeRegistrationInput& input)
        {
            return terminal::workspace::WorkspaceNodeRuntimeRegistrationInput{
                .WorkspaceNodeId = input.WorkspaceNodeId,
                .PendingStartupAction = input.PendingStartupAction,
                .StartingDirectory = input.StartingDirectory,
                .ProfileSource = input.ProfileSource,
                .ProfileCommandline = input.ProfileCommandline,
                .TerminalCommandline = input.TerminalCommandline,
                .CurrentWorkspaceId = input.CurrentWorkspaceId,
                .SelectedWorkspace = input.SelectedWorkspace ? std::optional<terminal::workspace::Workspace>{ _toCoreWorkspace(*input.SelectedWorkspace) } : std::nullopt,
            };
        }

        WorkspaceNodeRuntimeStatePlan _fromCoreRuntimeStatePlan(const terminal::workspace::WorkspaceNodeRuntimeStatePlan& plan)
        {
            return WorkspaceNodeRuntimeStatePlan{
                .WorkspaceNodeId = plan.WorkspaceNodeId,
                .StartupAction = plan.StartupAction,
                .ExplicitCommandline = plan.ExplicitCommandline,
                .StartingDirectory = plan.StartingDirectory,
                .OperatingSystem = plan.OperatingSystem,
                .ShellType = plan.ShellType,
                .IsSshTransport = plan.IsSshTransport,
                .HasSshTtyOption = plan.HasSshTtyOption,
                .DeferredStartupInputs = plan.DeferredStartupInputs,
                .StartupInputPending = plan.StartupInputPending,
                .StartupInputDispatched = plan.StartupInputDispatched,
                .SkipPendingStartupSendInput = plan.SkipPendingStartupSendInput,
                .HasRuntimeState = plan.HasRuntimeState,
            };
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
