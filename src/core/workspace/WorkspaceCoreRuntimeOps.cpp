    WorkspaceOpenPlan PrepareWorkspaceForOpen(const std::wstring_view workspaceId,
                                              const bool openInNewWindow,
                                              const std::wstring_view currentWorkspaceId,
                                              const bool currentWorkspaceNeedsSave,
                                              const WorkspaceManager& manager,
                                              const WorkspaceStateManager& stateManager)
    {
        WorkspaceOpenPlan plan;
        if (workspaceId.empty())
        {
            return plan;
        }

        if (const auto existingWindowId = stateManager.FindOpenWorkspaceWindowId(workspaceId))
        {
            plan.Disposition = WorkspaceOpenDisposition::SummonExistingWindow;
            plan.ExistingWindowId = existingWindowId;
            return plan;
        }

        const auto workspace = manager.FindById(workspaceId);
        if (workspace == nullptr)
        {
            return plan;
        }

        plan.TargetWorkspace = *workspace;
        plan.PendingNodeIds = VisibleWorkspaceNodeIds(*workspace);
        plan.PendingNodeInputVisibility = VisibleWorkspaceNodeInputVisibility(*workspace);
        // Workspaces are immutable at runtime and always get their own window.
        // Retain the legacy parameters for the adapter ABI while intentionally
        // ignoring the old replace-current/save path.
        (void)openInNewWindow;
        (void)currentWorkspaceId;
        (void)currentWorkspaceNeedsSave;
        plan.ConfirmSaveCurrentWorkspace = false;
        plan.Disposition = WorkspaceOpenDisposition::OpenInNewWindow;
        return plan;
    }

    WorkspaceOpenExecutionPlan ResolveWorkspaceOpenExecutionPlan(const WorkspaceOpenPlan& openPlan,
                                                                 const bool hasStartupActions,
                                                                 const bool hasTabsToReplace)
    {
        WorkspaceOpenExecutionPlan plan;
        plan.ExistingWindowId = openPlan.ExistingWindowId;
        plan.ConfirmSaveCurrentWorkspace = openPlan.ConfirmSaveCurrentWorkspace;

        switch (openPlan.Disposition)
        {
        case WorkspaceOpenDisposition::SummonExistingWindow:
            plan.Disposition = WorkspaceOpenExecutionDisposition::SummonExistingWindow;
            return plan;
        case WorkspaceOpenDisposition::Missing:
            plan.Disposition = WorkspaceOpenExecutionDisposition::Missing;
            return plan;
        case WorkspaceOpenDisposition::OpenInNewWindow:
            if (!hasStartupActions)
            {
                plan.Disposition = WorkspaceOpenExecutionDisposition::NoStartupActions;
                return plan;
            }
            plan.Disposition = WorkspaceOpenExecutionDisposition::OpenInNewWindow;
            plan.UpdatePendingWorkspaceLaunch = true;
            return plan;
        case WorkspaceOpenDisposition::ReplaceCurrentWindow:
            // Compatibility for callers that still construct the old disposition:
            // route it to the same new-window behavior rather than replacing tabs.
            (void)hasTabsToReplace;
            plan.Disposition = hasStartupActions ? WorkspaceOpenExecutionDisposition::OpenInNewWindow :
                                                   WorkspaceOpenExecutionDisposition::NoStartupActions;
            plan.UpdatePendingWorkspaceLaunch = hasStartupActions;
            return plan;
        default:
            plan.Disposition = WorkspaceOpenExecutionDisposition::Missing;
            return plan;
        }
    }

    WorkspaceSshStartupPlan PrepareSshStartupPlan(const std::wstring_view pendingStartupAction,
                                                  const std::wstring_view startingDirectory,
                                                  const std::wstring_view operatingSystem,
                                                  const std::wstring_view shellType,
                                                  const std::optional<WorkspaceNode>& workspaceNode)
    {
        WorkspaceSshStartupPlan plan;
        plan.StartingDirectory = std::wstring{ startingDirectory };
        plan.OperatingSystem = std::wstring{ operatingSystem };
        plan.ShellType = std::wstring{ shellType };

        if (workspaceNode)
        {
            if (plan.StartingDirectory.empty())
            {
                plan.StartingDirectory = workspaceNode->StartupDirectory;
            }
            if (plan.OperatingSystem.empty())
            {
                plan.OperatingSystem = workspaceNode->OperatingSystem;
            }
            if (plan.ShellType.empty())
            {
                plan.ShellType = workspaceNode->ShellType;
            }

            plan.StartupAction = workspaceNode->StartupAction;
            plan.DeferredStartupInputs = _buildSshDeferredStartupInputs(*workspaceNode);
        }
        else
        {
            plan.StartupAction = std::wstring{ pendingStartupAction };
        }

        if (plan.DeferredStartupInputs.empty() && !pendingStartupAction.empty())
        {
            plan.DeferredStartupInputs.emplace_back(pendingStartupAction);
        }

        plan.StartupInputPending = !plan.DeferredStartupInputs.empty();
        return plan;
    }

    WorkspaceNodeRuntimeStatePlan PrepareWorkspaceNodeRuntimeState(const WorkspaceNodeRuntimeRegistrationInput& input)
    {
        WorkspaceNodeRuntimeStatePlan plan;
        plan.WorkspaceNodeId = input.WorkspaceNodeId;

        const auto runtimeLaunchState = PrepareWorkspaceRuntimeLaunchState(input.StartingDirectory,
                                                                           input.ProfileSource,
                                                                           input.ProfileCommandline,
                                                                           input.TerminalCommandline);
        plan.ExplicitCommandline = runtimeLaunchState.ExplicitCommandline;
        plan.StartingDirectory = runtimeLaunchState.StartingDirectory;
        plan.OperatingSystem = runtimeLaunchState.OperatingSystem;
        plan.ShellType = runtimeLaunchState.ShellType;
        plan.IsSshTransport = runtimeLaunchState.IsSshTransport;
        plan.HasSshTtyOption = runtimeLaunchState.HasSshTtyOption;

        if (!input.PendingStartupAction.empty())
        {
            if (plan.IsSshTransport)
            {
                const auto workspaceNode = input.SelectedWorkspace ? FindWorkspaceNodeById(*input.SelectedWorkspace, input.WorkspaceNodeId) :
                                                                    std::nullopt;
                const auto sshStartupPlan = PrepareSshStartupPlan(input.PendingStartupAction,
                                                                  plan.StartingDirectory,
                                                                  plan.OperatingSystem,
                                                                  plan.ShellType,
                                                                  workspaceNode);
                plan.StartupAction = sshStartupPlan.StartupAction;
                plan.StartingDirectory = sshStartupPlan.StartingDirectory;
                plan.OperatingSystem = sshStartupPlan.OperatingSystem;
                plan.ShellType = sshStartupPlan.ShellType;
                plan.DeferredStartupInputs = sshStartupPlan.DeferredStartupInputs;
                plan.StartupInputPending = sshStartupPlan.StartupInputPending;
                plan.StartupInputDispatched = sshStartupPlan.StartupInputDispatched;
                plan.SkipPendingStartupSendInput = sshStartupPlan.StartupInputPending;
            }
            else
            {
                plan.StartupAction = std::wstring{ input.PendingStartupAction };
                plan.DeferredStartupInputs = { plan.StartupAction };
                plan.StartupInputPending = !plan.StartupAction.empty();
                plan.StartupInputDispatched = false;
                plan.SkipPendingStartupSendInput = plan.StartupInputPending;
            }
        }

        plan.HasRuntimeState = !plan.WorkspaceNodeId.empty() ||
                               !plan.StartupAction.empty() ||
                               !plan.ExplicitCommandline.empty() ||
                               !plan.StartingDirectory.empty() ||
                               !plan.OperatingSystem.empty() ||
                               !plan.ShellType.empty();
        return plan;
    }

    bool IsWorkspaceLocked(const std::wstring_view workspaceId)
    {
        return !workspaceId.empty();
    }

    bool SetWorkspaceLocked(WorkspaceManager& manager, const std::wstring_view workspaceId, const bool locked)
    {
        if (const auto workspace = std::find_if(manager.Workspaces().begin(), manager.Workspaces().end(), [&](const auto& candidate) {
                return candidate.Id == workspaceId;
            });
            workspace != manager.Workspaces().end())
        {
            // A workspace is permanently locked outside the management tab.
            (void)locked;
            workspace->Locked = true;
            return true;
        }

        return false;
    }

    bool PersistWorkspaceLockedState(const std::wstring_view workspaceId, const bool locked)
    {
        if (workspaceId.empty())
        {
            return false;
        }

        (void)locked;
        return _persistManagerChange(WorkspaceManager::Load(), [&](auto& manager) {
            return SetWorkspaceLocked(manager, workspaceId, true);
        }).has_value();
    }

    bool SetWorkspaceNodeInputVisibility(WorkspaceManager& manager, const std::wstring_view workspaceId, const size_t nodeIndex, const bool showInputPanel)
    {
        if (const auto workspace = std::find_if(manager.Workspaces().begin(), manager.Workspaces().end(), [&](const auto& candidate) {
                return candidate.Id == workspaceId;
            });
            workspace != manager.Workspaces().end() && nodeIndex < workspace->Nodes.size())
        {
            workspace->Nodes.at(nodeIndex).ShowInputPanel = showInputPanel;
            return true;
        }

        return false;
    }

    bool RemoveWorkspaceDefinition(WorkspaceManager& manager, const std::wstring_view workspaceId, size_t* removedWorkspaceIndex)
    {
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return false;
        }

        if (removedWorkspaceIndex)
        {
            *removedWorkspaceIndex = static_cast<size_t>(std::distance(workspaces.begin(), workspaceIt));
        }
        workspaces.erase(workspaceIt);
        return true;
    }

    WorkspaceNodeMutationResult RemoveWorkspaceNode(WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view nodeId)
    {
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return {};
        }

        auto& nodes = workspaceIt->Nodes;
        const auto nodeIt = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
            return node.Id == nodeId;
        });
        if (nodeIt == nodes.end())
        {
            return {};
        }

        WorkspaceNodeMutationResult result;
        result.WorkspaceIndex = static_cast<size_t>(std::distance(workspaces.begin(), workspaceIt));
        result.NodeIndex = static_cast<size_t>(std::distance(nodes.begin(), nodeIt));
        if (nodes.size() == 1)
        {
            workspaces.erase(workspaceIt);
            result.Disposition = WorkspaceNodeMutationDisposition::RemovedWorkspace;
            return result;
        }

        nodes.erase(nodeIt);
        result.Disposition = WorkspaceNodeMutationDisposition::RemovedNode;
        return result;
    }

    void FinalizeWorkspaceManagerNames(WorkspaceManager& manager)
    {
        std::unordered_set<std::wstring> usedWorkspaceNames;
        for (auto& workspace : manager.Workspaces())
        {
            NormalizeWorkspacePersistableNames(workspace);
            workspace.Name = MakeUniquePersistedName(workspace.Name, usedWorkspaceNames);
            workspace.Id = workspace.Name;
        }
    }

    std::optional<std::wstring> RenameWorkspace(WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view newName)
    {
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return std::nullopt;
        }

        const auto workspaceIndex = static_cast<size_t>(std::distance(workspaces.begin(), workspaceIt));
        workspaceIt->Name = SanitizeWorkspaceDirectoryName(newName, L"workspace");
        FinalizeWorkspaceManagerNames(manager);
        if (workspaceIndex >= workspaces.size())
        {
            return std::nullopt;
        }

        return workspaces.at(workspaceIndex).Name;
    }

    std::optional<uint64_t> FindPersistedOpenWorkspaceWindowId(const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            return std::nullopt;
        }

        return WorkspaceStateManager::LoadRuntime().FindOpenWorkspaceWindowId(workspaceId);
    }

    std::optional<PersistedWorkspaceRename> PersistWorkspaceRename(const std::wstring_view workspaceId, const std::wstring_view newName)
    {
        if (workspaceId.empty() || newName.empty())
        {
            return std::nullopt;
        }

        auto manager = WorkspaceManager::Load();
        const auto resolvedWorkspaceName = RenameWorkspace(manager, workspaceId, newName);
        if (!resolvedWorkspaceName.has_value() || !manager.Save())
        {
            return std::nullopt;
        }

        return PersistedWorkspaceRename{
            .Manager = std::move(manager),
            .ResolvedWorkspaceName = std::move(*resolvedWorkspaceName),
        };
    }

    std::optional<PersistedWorkspaceEditorSave> PersistWorkspaceEditorState(const WorkspaceManager& editorManager,
                                                                            const std::wstring_view currentWorkspaceId,
                                                                            const std::wstring_view lastOpenedWorkspaceId,
                                                                            const size_t fallbackSelectedWorkspaceIndex)
    {
        const auto persistedManager = WorkspaceManager::Load();
        auto manager = editorManager;
        const auto savePlan = PrepareWorkspaceEditorForSave(manager,
                                                            persistedManager,
                                                            currentWorkspaceId,
                                                            lastOpenedWorkspaceId,
                                                            fallbackSelectedWorkspaceIndex);
        if (!manager.Save())
        {
            return std::nullopt;
        }

        return PersistedWorkspaceEditorSave{
            .Manager = std::move(manager),
            .SavePlan = savePlan,
        };
    }

    WorkspaceCurrentIdChangePlan PrepareWorkspaceCurrentIdChange(const std::wstring_view previousWorkspaceId,
                                                                 const std::wstring_view nextWorkspaceId,
                                                                 const std::wstring_view lastWorkspaceId,
                                                                 const std::wstring_view currentBaselineWorkspaceId)
    {
        WorkspaceCurrentIdChangePlan plan;
        plan.LastWorkspaceId = std::wstring{ lastWorkspaceId };
        if (!nextWorkspaceId.empty())
        {
            plan.LastWorkspaceId = std::wstring{ nextWorkspaceId };
        }
        else
        {
            plan.ResetSaveBaseline = true;
        }

        if (!nextWorkspaceId.empty() &&
            previousWorkspaceId != nextWorkspaceId &&
            (currentBaselineWorkspaceId.empty() || currentBaselineWorkspaceId != nextWorkspaceId))
        {
            plan.ResetSaveBaseline = true;
        }

        plan.StartHeartbeat = !nextWorkspaceId.empty();
        return plan;
    }

    WorkspaceWindowRefreshPlan PrepareWorkspaceWindowRefresh(const std::uint64_t windowId,
                                                             const std::wstring_view currentWorkspaceId)
    {
        WorkspaceWindowRefreshPlan plan;
        plan.SkipRefresh = windowId == 0;
        plan.WorkspaceId = std::wstring{ currentWorkspaceId };
        plan.ClearPendingWorkspaceLaunch = !plan.SkipRefresh && !currentWorkspaceId.empty();
        return plan;
    }

    WorkspaceWindowRefreshPlan RefreshWorkspaceWindowState(const std::uint64_t windowId,
                                                           const std::wstring_view currentWorkspaceId)
    {
        auto plan = PrepareWorkspaceWindowRefresh(windowId, currentWorkspaceId);
        if (plan.SkipRefresh)
        {
            return plan;
        }

        plan.ProcessId = GetCurrentProcessId();
        plan.Refreshed = WorkspaceStateManager::RefreshRuntimeWindowState(windowId,
                                                                          std::wstring_view{},
                                                                          plan.WorkspaceId);
        return plan;
    }

    std::wstring ResolveWorkspaceSaveTargetId(const std::wstring_view currentWorkspaceId, const std::wstring_view lastWorkspaceId, const WorkspaceManager& manager)
    {
        if (!currentWorkspaceId.empty())
        {
            return std::wstring{ currentWorkspaceId };
        }

        if (lastWorkspaceId.empty())
        {
            return {};
        }

        return manager.FindById(lastWorkspaceId) ? std::wstring{ lastWorkspaceId } : std::wstring{};
    }

    std::wstring ResolveWorkspaceSaveTargetName(const std::wstring_view currentWorkspaceId, const std::wstring_view lastWorkspaceId, const WorkspaceManager& manager)
    {
        if (const auto targetId = ResolveWorkspaceSaveTargetId(currentWorkspaceId, lastWorkspaceId, manager); !targetId.empty())
        {
            if (const auto workspace = manager.FindById(targetId))
            {
                return workspace->Name;
            }
        }

        return {};
    }

    std::wstring SuggestWorkspaceSaveName(const std::wstring_view resolvedTargetName,
                                          const std::wstring_view windowName,
                                          const std::wstring_view singleTabTitle,
                                          const size_t workspaceCount,
                                          const std::wstring_view generatedFallbackName)
    {
        std::wstring suggestedName;
        if (!resolvedTargetName.empty())
        {
            suggestedName = resolvedTargetName;
        }
        else if (!windowName.empty())
        {
            suggestedName = windowName;
        }
        else if (!singleTabTitle.empty())
        {
            suggestedName = singleTabTitle;
        }
        else if (!generatedFallbackName.empty())
        {
            suggestedName = generatedFallbackName;
        }
        else
        {
            suggestedName = L"Workspace " + std::to_wstring(workspaceCount + 1);
        }

        return SanitizeWorkspaceDirectoryName(suggestedName, L"Workspace");
    }

    std::optional<WorkspaceEditorDefinitionAddResult> AddWorkspaceDefinition(WorkspaceManager& manager,
                                                                             const std::wstring_view generatedName,
                                                                             const std::optional<size_t> templateIndex)
    {
        if (generatedName.empty())
        {
            return std::nullopt;
        }

        auto& workspaces = manager.Workspaces();
        Workspace workspace;
        if (templateIndex.has_value() && *templateIndex < workspaces.size())
        {
            workspace = workspaces.at(*templateIndex);
        }

        workspace.Name = std::wstring{ generatedName };
        workspace.Id = workspace.Name;
        if (workspace.BackgroundColor.empty())
        {
            workspace.BackgroundColor = PickUnusedWorkspaceColor(workspaces);
        }

        workspaces.emplace_back(std::move(workspace));
        return WorkspaceEditorDefinitionAddResult{
            .AddedWorkspaceIndex = workspaces.size() - 1,
        };
    }

    WorkspaceEditorNodeAddResult AddWorkspaceNode(WorkspaceManager& manager,
                                                  const size_t workspaceIndex,
                                                  const std::wstring_view generatedName,
                                                  const std::wstring_view defaultProfileGuid,
                                                  const std::wstring_view defaultProfileName)
    {
        auto& workspaces = manager.Workspaces();
        if (workspaceIndex >= workspaces.size() || generatedName.empty())
        {
            return {};
        }

        auto& workspace = workspaces.at(workspaceIndex);
        auto node = workspace.NewNodeDefaults;
        node.Name = std::wstring{ generatedName };
        node.Id = node.Name;
        if (node.ProfileGuid.empty())
        {
            node.ProfileGuid = std::wstring{ defaultProfileGuid };
            node.ProfileName = std::wstring{ defaultProfileName };
        }

        workspace.Nodes.emplace_back(std::move(node));
        return WorkspaceEditorNodeAddResult{
            .Added = true,
        };
    }

    std::optional<WorkspaceManager> PersistWorkspaceNodeInputVisibility(const WorkspaceManager& preferredManager,
                                                                        const std::wstring_view workspaceId,
                                                                        const size_t nodeIndex,
                                                                        const bool showInputPanel)
    {
        auto manager = preferredManager;
        if (!SetWorkspaceNodeInputVisibility(manager, workspaceId, nodeIndex, showInputPanel))
        {
            manager = WorkspaceManager::Load();
            if (!SetWorkspaceNodeInputVisibility(manager, workspaceId, nodeIndex, showInputPanel))
            {
                return std::nullopt;
            }
        }

        if (!manager.Save())
        {
            return std::nullopt;
        }

        return manager;
    }

    std::optional<WorkspaceManager> PersistWorkspaceNodeOrder(const std::wstring_view workspaceId,
                                                              const std::vector<std::wstring>& orderedNodeIds)
    {
        if (workspaceId.empty() || orderedNodeIds.empty())
        {
            return std::nullopt;
        }

        return _persistManagerChange(WorkspaceManager::Load(), [&](auto& manager) {
            return manager.ReorderWorkspaceNodes(workspaceId, orderedNodeIds);
        });
    }
