    std::optional<Workspace> PrepareWorkspaceForCapture(const std::optional<Workspace>& currentWorkspaceDefinition,
                                                        std::vector<WorkspaceNode> capturedNodes)
    {
        const auto capturedTabOrder = _captureVisibleWorkspaceNodeOrder(capturedNodes);
        Workspace workspace;
        if (currentWorkspaceDefinition.has_value())
        {
            workspace = *currentWorkspaceDefinition;

            std::vector<WorkspaceNode> mergedNodes;
            mergedNodes.reserve(workspace.Nodes.size() + capturedNodes.size());
            std::vector<bool> consumedCapturedNodes(capturedNodes.size(), false);

            for (const auto& existingNode : workspace.Nodes)
            {
                if (!WorkspaceNodeLoadsTab(existingNode))
                {
                    mergedNodes.emplace_back(existingNode);
                    continue;
                }

                if (!existingNode.Id.empty())
                {
                    auto matched = false;
                    for (size_t capturedIndex = 0; capturedIndex < capturedNodes.size(); ++capturedIndex)
                    {
                        const auto& capturedNode = capturedNodes.at(capturedIndex);
                        if (!consumedCapturedNodes.at(capturedIndex) && capturedNode.Id == existingNode.Id)
                        {
                            mergedNodes.emplace_back(capturedNode);
                            consumedCapturedNodes.at(capturedIndex) = true;
                            matched = true;
                            break;
                        }
                    }

                    if (matched)
                    {
                        continue;
                    }
                }

                // Visible nodes missing from the live capture were removed from the workspace.
            }

            for (size_t capturedIndex = 0; capturedIndex < capturedNodes.size(); ++capturedIndex)
            {
                if (!consumedCapturedNodes.at(capturedIndex))
                {
                    mergedNodes.emplace_back(std::move(capturedNodes.at(capturedIndex)));
                }
            }

            workspace.Nodes = std::move(mergedNodes);
        }
        else
        {
            workspace.Nodes = std::move(capturedNodes);
        }

        workspace.TabOrder = capturedTabOrder;

        if (workspace.Nodes.empty())
        {
            return std::nullopt;
        }

        return workspace;
    }

    std::optional<Workspace> ResolveWorkspaceDefinition(const std::wstring_view currentWorkspaceId,
                                                        const std::optional<Workspace>& selectedWorkspace,
                                                        const WorkspaceManager& manager)
    {
        if (currentWorkspaceId.empty())
        {
            return std::nullopt;
        }

        if (selectedWorkspace && selectedWorkspace->Id == currentWorkspaceId)
        {
            return selectedWorkspace;
        }

        if (const auto workspace = manager.FindById(currentWorkspaceId))
        {
            return *workspace;
        }

        return std::nullopt;
    }

    bool WorkspaceNodeLoadsTab(const WorkspaceNode& node) noexcept
    {
        return node.ShowTab;
    }

    namespace
    {
        std::vector<size_t> _orderedVisibleWorkspaceNodeIndices(const Workspace& workspace)
        {
            std::vector<size_t> orderedIndices;
            orderedIndices.reserve(workspace.Nodes.size());

            std::vector<bool> consumed(workspace.Nodes.size(), false);
            for (const auto& nodeId : workspace.TabOrder)
            {
                if (nodeId.empty())
                {
                    continue;
                }

                for (size_t index = 0; index < workspace.Nodes.size(); ++index)
                {
                    const auto& node = workspace.Nodes.at(index);
                    if (consumed[index] || !WorkspaceNodeLoadsTab(node) || node.Id != nodeId)
                    {
                        continue;
                    }

                    orderedIndices.emplace_back(index);
                    consumed[index] = true;
                    break;
                }
            }

            for (size_t index = 0; index < workspace.Nodes.size(); ++index)
            {
                if (!consumed[index] && WorkspaceNodeLoadsTab(workspace.Nodes.at(index)))
                {
                    orderedIndices.emplace_back(index);
                }
            }

            return orderedIndices;
        }

        std::vector<std::wstring> _captureVisibleWorkspaceNodeOrder(const std::vector<WorkspaceNode>& nodes)
        {
            std::vector<std::wstring> orderedIds;
            orderedIds.reserve(nodes.size());
            for (const auto& node : nodes)
            {
                if (WorkspaceNodeLoadsTab(node) && !node.Id.empty())
                {
                    orderedIds.emplace_back(node.Id);
                }
            }
            return orderedIds;
        }
    }

    std::optional<WorkspaceNode> FindWorkspaceNodeById(const Workspace& workspace, const std::wstring_view nodeId)
    {
        if (const auto index = FindWorkspaceNodeIndexById(workspace, nodeId))
        {
            return workspace.Nodes.at(*index);
        }
        return std::nullopt;
    }

    std::optional<WorkspaceNode> FindWorkspaceNodeById(const WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view nodeId)
    {
        if (const auto workspace = manager.FindById(workspaceId))
        {
            return FindWorkspaceNodeById(*workspace, nodeId);
        }
        return std::nullopt;
    }

    std::optional<WorkspaceNode> ResolveCurrentWorkspaceNode(const std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace,
                                                             const WorkspaceManager& manager,
                                                             const std::wstring_view nodeId)
    {
        if (nodeId.empty() || currentWorkspaceId.empty())
        {
            return std::nullopt;
        }

        if (selectedWorkspace && selectedWorkspace->Id == currentWorkspaceId)
        {
            if (const auto node = FindWorkspaceNodeById(*selectedWorkspace, nodeId))
            {
                return node;
            }
        }

        return FindWorkspaceNodeById(manager, currentWorkspaceId, nodeId);
    }

    std::optional<size_t> FindWorkspaceNodeIndexById(const Workspace& workspace, const std::wstring_view nodeId)
    {
        if (nodeId.empty())
        {
            return std::nullopt;
        }

        for (size_t i = 0; i < workspace.Nodes.size(); ++i)
        {
            if (workspace.Nodes.at(i).Id == nodeId)
            {
                return i;
            }
        }

        return std::nullopt;
    }

    std::optional<size_t> FindWorkspaceVisibleNodeIndex(const Workspace& workspace, const size_t visibleOrdinal)
    {
        const auto orderedIndices = _orderedVisibleWorkspaceNodeIndices(workspace);
        return visibleOrdinal < orderedIndices.size() ? std::optional<size_t>{ orderedIndices.at(visibleOrdinal) } : std::nullopt;
    }

    std::optional<size_t> ResolveWorkspaceBackedNodeIndex(const std::optional<Workspace>& workspaceDefinition,
                                                          const std::wstring_view runtimeNodeId,
                                                          const std::optional<size_t> visibleOrdinal)
    {
        if (!workspaceDefinition.has_value())
        {
            return visibleOrdinal;
        }

        if (const auto nodeIndex = FindWorkspaceNodeIndexById(*workspaceDefinition, runtimeNodeId))
        {
            return nodeIndex;
        }

        if (visibleOrdinal.has_value())
        {
            return FindWorkspaceVisibleNodeIndex(*workspaceDefinition, *visibleOrdinal);
        }

        return std::nullopt;
    }

    std::optional<WorkspaceNode> ResolveWorkspaceBackedNode(const std::optional<Workspace>& workspaceDefinition,
                                                            const std::wstring_view runtimeNodeId,
                                                            const std::optional<size_t> visibleOrdinal)
    {
        if (!workspaceDefinition.has_value())
        {
            return std::nullopt;
        }

        if (const auto nodeIndex = ResolveWorkspaceBackedNodeIndex(workspaceDefinition, runtimeNodeId, visibleOrdinal);
            nodeIndex.has_value() && *nodeIndex < workspaceDefinition->Nodes.size())
        {
            return workspaceDefinition->Nodes.at(*nodeIndex);
        }

        return std::nullopt;
    }

    namespace
    {
        std::optional<size_t> _resolveWorkspaceLiveTabVisibleOrdinal(const std::vector<WorkspaceLiveTabSnapshot>& tabs, const size_t targetTabIndex)
        {
            if (targetTabIndex >= tabs.size() || !tabs.at(targetTabIndex).LoadsWorkspaceNode)
            {
                return std::nullopt;
            }

            size_t visibleOrdinal = 0;
            for (size_t index = 0; index < targetTabIndex; ++index)
            {
                if (tabs.at(index).LoadsWorkspaceNode)
                {
                    ++visibleOrdinal;
                }
            }
            return visibleOrdinal;
        }
    }

    std::optional<size_t> ResolveWorkspaceBackedTabIndex(const std::optional<Workspace>& workspaceDefinition,
                                                         const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                         const size_t targetTabIndex)
    {
        if (targetTabIndex >= tabs.size())
        {
            return std::nullopt;
        }

        return ResolveWorkspaceBackedNodeIndex(workspaceDefinition,
                                               tabs.at(targetTabIndex).RuntimeNodeId,
                                               _resolveWorkspaceLiveTabVisibleOrdinal(tabs, targetTabIndex));
    }

    std::optional<WorkspaceNode> ResolveWorkspaceBackedTabNode(const std::optional<Workspace>& workspaceDefinition,
                                                               const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                               const size_t targetTabIndex)
    {
        if (targetTabIndex >= tabs.size())
        {
            return std::nullopt;
        }

        return ResolveWorkspaceBackedNode(workspaceDefinition,
                                          tabs.at(targetTabIndex).RuntimeNodeId,
                                          _resolveWorkspaceLiveTabVisibleOrdinal(tabs, targetTabIndex));
    }

    std::optional<size_t> FindWorkspaceBackedTabSnapshotIndex(const std::optional<Workspace>& workspaceDefinition,
                                                              const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                              const size_t nodeIndex)
    {
        for (size_t tabIndex = 0; tabIndex < tabs.size(); ++tabIndex)
        {
            if (const auto currentNodeIndex = ResolveWorkspaceBackedTabIndex(workspaceDefinition, tabs, tabIndex);
                currentNodeIndex.has_value() && currentNodeIndex.value() == nodeIndex)
            {
                return tabIndex;
            }
        }

        return std::nullopt;
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceLiveTabCaptureState& state)
    {
        WorkspaceNode node = state.PersistedNode.value_or(WorkspaceNode{});
        if (!state.PersistedNode.has_value())
        {
            node.UseNodeNameAsTabTitle = false;
            node.Name = state.LiveTabTitle.empty() ? state.StartupTabTitle : state.LiveTabTitle;
            if (node.Name.empty())
            {
                node.Name = state.GeneratedNodeName;
            }
            node.Id = node.Name;
        }

        node.ProfileGuid = state.ProfileGuid;
        if (!state.ProfileName.empty() || !state.PersistedNode.has_value())
        {
            node.ProfileName = state.ProfileName;
        }
        node.StartupDirectory = state.LaunchResolution.StartingDirectory;
        node.StartupAction = state.LaunchResolution.StartupAction;
        node.OperatingSystem = state.LaunchResolution.OperatingSystem;
        node.ShellType = state.LaunchResolution.ShellType;
        node.ShowInputPanel = state.ShowInputPanel;
        node.TabColor = state.TabColor;
        return node;
    }

    WorkspaceCapturedNodeInput BuildWorkspaceCapturedNodeInput(const WorkspaceCapturedNodePlanInput& input)
    {
        WorkspaceCapturedNodeInput captureInput;
        captureInput.LaunchInput.PersistedNode = input.PersistedNode;
        captureInput.LaunchInput.ProfileSource = input.ProfileSource;
        captureInput.LaunchInput.ProfileCommandline = input.ProfileCommandline;
        captureInput.LaunchInput.TerminalCommandline = input.TerminalCommandline;
        captureInput.LaunchInput.TerminalStartingDirectory = input.TerminalStartingDirectory;
        captureInput.LaunchInput.ObservedStartupAction = input.ObservedStartupAction;
        captureInput.LaunchInput.ObservedWorkingDirectory = input.ObservedWorkingDirectory.empty() ? input.TrackedWorkingDirectory :
                                                                                                      input.ObservedWorkingDirectory;
        captureInput.LaunchInput.ObservedOperatingSystem = input.ObservedOperatingSystem;
        captureInput.LaunchInput.ObservedShellType = input.ObservedShellType;
        captureInput.LaunchInput.RuntimeStartupAction = input.RuntimeStartupAction;
        captureInput.LaunchInput.RuntimeExplicitCommandline = input.RuntimeExplicitCommandline;
        captureInput.LaunchInput.RuntimeStartingDirectory = input.RuntimeStartingDirectory;
        captureInput.LaunchInput.RuntimeOperatingSystem = input.RuntimeOperatingSystem;
        captureInput.LaunchInput.RuntimeShellType = input.RuntimeShellType;

        captureInput.CaptureState.PersistedNode = input.PersistedNode;
        captureInput.CaptureState.LiveTabTitle = input.LiveTabTitle;
        captureInput.CaptureState.StartupTabTitle = input.StartupTabTitle;
        captureInput.CaptureState.GeneratedNodeName = input.GeneratedNodeName;
        captureInput.CaptureState.ProfileGuid = input.ProfileGuid;
        captureInput.CaptureState.ProfileName = input.ProfileName;
        captureInput.CaptureState.ShowInputPanel = input.ShowInputPanel;
        captureInput.CaptureState.TabColor = input.TabColor;
        return captureInput;
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceCapturedNodeInput& input)
    {
        auto captureState = input.CaptureState;
        captureState.LaunchResolution = ResolveWorkspaceNodeLaunchResolution(input.LaunchInput);
        return BuildWorkspaceCapturedNode(captureState);
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceCapturedNodePlanInput& input)
    {
        return BuildWorkspaceCapturedNode(BuildWorkspaceCapturedNodeInput(input));
    }
