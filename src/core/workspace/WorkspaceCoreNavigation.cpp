    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes)
    {
        const auto visibleNodeCount = static_cast<size_t>(std::count_if(workspace.Nodes.begin(), workspace.Nodes.end(), [](const auto& node) {
            return node.ShowTab;
        }));
        if (orderedVisibleNodes.size() < visibleNodeCount)
        {
            return false;
        }

        std::vector<std::wstring_view> seenNodeIds;
        seenNodeIds.reserve(orderedVisibleNodes.size());
        for (const auto& node : orderedVisibleNodes)
        {
            if (node.Id.empty())
            {
                return false;
            }

            if (std::find(seenNodeIds.begin(), seenNodeIds.end(), std::wstring_view{ node.Id }) != seenNodeIds.end())
            {
                return false;
            }

            seenNodeIds.emplace_back(node.Id);
        }

        std::vector<WorkspaceNode> reorderedNodes;
        reorderedNodes.reserve(workspace.Nodes.size() + orderedVisibleNodes.size() - visibleNodeCount);

        size_t visibleNodeCursor = 0;
        for (const auto& existingNode : workspace.Nodes)
        {
            if (!existingNode.ShowTab)
            {
                reorderedNodes.emplace_back(existingNode);
                continue;
            }

            reorderedNodes.emplace_back(orderedVisibleNodes.at(visibleNodeCursor++));
        }

        while (visibleNodeCursor < orderedVisibleNodes.size())
        {
            reorderedNodes.emplace_back(orderedVisibleNodes.at(visibleNodeCursor++));
        }

        workspace.Nodes = std::move(reorderedNodes);
        // The visible node vector and the persisted tab order are one logical
        // ordering. Keep both in sync so every caller (including directory
        // loading and editor drag/drop) has an order that survives Save().
        workspace.TabOrder = _captureVisibleWorkspaceNodeOrder(workspace.Nodes);
        return true;
    }

    std::vector<std::wstring> VisibleWorkspaceNodeIds(const Workspace& workspace)
    {
        std::vector<std::wstring> values;
        for (const auto nodeIndex : _orderedVisibleWorkspaceNodeIndices(workspace))
        {
            values.emplace_back(workspace.Nodes.at(nodeIndex).Id);
        }
        return values;
    }

    std::vector<bool> VisibleWorkspaceNodeInputVisibility(const Workspace& workspace)
    {
        std::vector<bool> values;
        for (const auto nodeIndex : _orderedVisibleWorkspaceNodeIndices(workspace))
        {
            values.emplace_back(workspace.Nodes.at(nodeIndex).ShowInputPanel);
        }
        return values;
    }

    int32_t WorkspaceManagerNavSelectionForWorkspace(const size_t workspaceIndex) noexcept
    {
        return _workspaceManagerWorkspaceSelectionBase + gsl::narrow_cast<int32_t>(workspaceIndex * _workspaceManagerWorkspaceSelectionStride);
    }

    int32_t WorkspaceManagerNavSelectionForWorkspaceNode(const size_t workspaceIndex, const size_t nodeIndex) noexcept
    {
        return WorkspaceManagerNavSelectionForWorkspace(workspaceIndex) + _workspaceManagerNodeSelectionBase + gsl::narrow_cast<int32_t>(nodeIndex);
    }

    std::optional<size_t> ResolveWorkspaceIndexFromManagerNavSelection(const int32_t navSelection) noexcept
    {
        if (navSelection < _workspaceManagerWorkspaceSelectionBase)
        {
            return std::nullopt;
        }

        return gsl::narrow_cast<size_t>((navSelection - _workspaceManagerWorkspaceSelectionBase) / _workspaceManagerWorkspaceSelectionStride);
    }

    std::optional<size_t> ResolveWorkspaceNodeIndexFromManagerNavSelection(const int32_t navSelection) noexcept
    {
        if (navSelection < _workspaceManagerWorkspaceSelectionBase)
        {
            return std::nullopt;
        }

        const auto subSelection = (navSelection - _workspaceManagerWorkspaceSelectionBase) % _workspaceManagerWorkspaceSelectionStride;
        if (subSelection < _workspaceManagerNodeSelectionBase)
        {
            return std::nullopt;
        }

        return gsl::narrow_cast<size_t>(subSelection - _workspaceManagerNodeSelectionBase);
    }

    int32_t ResolveWorkspaceManagerNavSelectionForEditor(const size_t workspaceCount, const size_t selectedWorkspaceIndex) noexcept
    {
        if (workspaceCount == 0)
        {
            return 0;
        }

        return WorkspaceManagerNavSelectionForWorkspace(std::min(selectedWorkspaceIndex, workspaceCount - 1));
    }

    int32_t ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(const int32_t previousNavSelection,
                                                                     const std::wstring_view selectedWorkspaceId,
                                                                     const std::wstring_view removedWorkspaceId,
                                                                     const size_t removedWorkspaceIndex,
                                                                     const size_t remainingWorkspaceCount) noexcept
    {
        const auto previousWorkspaceIndex = ResolveWorkspaceIndexFromManagerNavSelection(previousNavSelection);
        if (!previousWorkspaceIndex.has_value())
        {
            return previousNavSelection;
        }

        if (remainingWorkspaceCount == 0)
        {
            return 0;
        }

        if (selectedWorkspaceId == removedWorkspaceId)
        {
            return WorkspaceManagerNavSelectionForWorkspace(std::min(removedWorkspaceIndex, remainingWorkspaceCount - 1));
        }

        if (*previousWorkspaceIndex > removedWorkspaceIndex)
        {
            return previousNavSelection - _workspaceManagerWorkspaceSelectionStride;
        }

        return previousNavSelection;
    }

    int32_t ResolveWorkspaceManagerNavSelectionAfterNodeRemoval(const int32_t previousNavSelection,
                                                                const std::wstring_view selectedWorkspaceId,
                                                                const std::wstring_view workspaceId,
                                                                const size_t selectedWorkspaceIndex,
                                                                const size_t removedNodeIndex) noexcept
    {
        if (selectedWorkspaceId != workspaceId)
        {
            return previousNavSelection;
        }

        const auto selectedWorkspace = ResolveWorkspaceIndexFromManagerNavSelection(previousNavSelection);
        const auto selectedNode = ResolveWorkspaceNodeIndexFromManagerNavSelection(previousNavSelection);
        if (!selectedWorkspace.has_value() || !selectedNode.has_value() || *selectedWorkspace != selectedWorkspaceIndex)
        {
            return previousNavSelection;
        }

        if (*selectedNode == removedNodeIndex)
        {
            if (removedNodeIndex > 0)
            {
                return WorkspaceManagerNavSelectionForWorkspaceNode(selectedWorkspaceIndex, removedNodeIndex - 1);
            }

            return WorkspaceManagerNavSelectionForWorkspace(selectedWorkspaceIndex);
        }

        if (*selectedNode > removedNodeIndex)
        {
            return previousNavSelection - 1;
        }

        return previousNavSelection;
    }

    size_t ResolveWorkspaceEditorSelectedIndex(const WorkspaceManager& manager,
                                               const std::wstring_view selectedWorkspaceId,
                                               const std::wstring_view currentWorkspaceId,
                                               const size_t fallbackIndex) noexcept
    {
        const auto& workspaces = manager.Workspaces();
        if (workspaces.empty())
        {
            return 0;
        }

        if (!selectedWorkspaceId.empty())
        {
            if (const auto it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
                    return workspace.Id == selectedWorkspaceId;
                });
                it != workspaces.end())
            {
                return static_cast<size_t>(std::distance(workspaces.begin(), it));
            }
        }

        if (!currentWorkspaceId.empty())
        {
            if (const auto it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
                    return workspace.Id == currentWorkspaceId;
                });
                it != workspaces.end())
            {
                return static_cast<size_t>(std::distance(workspaces.begin(), it));
            }
        }

        return std::min(fallbackIndex, workspaces.size() - 1);
    }
