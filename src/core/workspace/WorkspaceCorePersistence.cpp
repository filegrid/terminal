    bool IsWorkspaceDirty(const Workspace& capturedWorkspace,
                          const std::wstring_view currentWorkspaceId,
                          const std::optional<Workspace>& baselineWorkspace,
                          const std::optional<Workspace>& persistedWorkspace)
    {
        if (currentWorkspaceId.empty())
        {
            return true;
        }

        if (baselineWorkspace.has_value())
        {
            return !WorkspaceLayoutEquivalent(*baselineWorkspace, capturedWorkspace);
        }

        if (persistedWorkspace.has_value())
        {
            return !WorkspaceLayoutEquivalent(*persistedWorkspace, capturedWorkspace);
        }

        return true;
    }

    bool WorkspaceManager::ReorderWorkspaceNodes(std::wstring_view workspaceId, const std::vector<std::wstring>& orderedNodeIds)
    {
        const auto workspaceIt = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == _workspaces.end())
        {
            return false;
        }

        auto& workspace = *workspaceIt;
        const auto visibleNodeCount = static_cast<size_t>(std::count_if(workspace.Nodes.begin(), workspace.Nodes.end(), [](const auto& node) {
            return node.ShowTab;
        }));
        if (orderedNodeIds.size() != visibleNodeCount)
        {
            return false;
        }

        std::vector<WorkspaceNode> orderedVisibleNodes;
        orderedVisibleNodes.reserve(orderedNodeIds.size());
        std::vector<std::wstring_view> consumedNodeIds;
        consumedNodeIds.reserve(orderedNodeIds.size());
        for (const auto& nodeId : orderedNodeIds)
        {
            if (std::find(consumedNodeIds.begin(), consumedNodeIds.end(), std::wstring_view{ nodeId }) != consumedNodeIds.end())
            {
                return false;
            }

            const auto nodeIt = std::find_if(workspace.Nodes.begin(), workspace.Nodes.end(), [&](const auto& node) {
                return node.Id == nodeId && node.ShowTab;
            });
            if (nodeIt == workspace.Nodes.end())
            {
                return false;
            }

            orderedVisibleNodes.emplace_back(*nodeIt);
            consumedNodeIds.emplace_back(orderedVisibleNodes.back().Id);
        }

        workspace.TabOrder.assign(orderedNodeIds.begin(), orderedNodeIds.end());
        // Keep the persisted node list aligned with the tab order as well.
        // Hidden nodes retain their relative positions; only visible nodes move.
        size_t visibleCursor = 0;
        for (auto& node : workspace.Nodes)
        {
            if (node.ShowTab)
            {
                node = orderedVisibleNodes.at(visibleCursor++);
            }
        }
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

    bool WorkspaceManager::DemoEnabled() const noexcept
    {
        return _demoEnabled;
    }

    void WorkspaceManager::SetDemoEnabled(const bool enabled) noexcept
    {
        _demoEnabled = enabled;
    }

    std::wstring SanitizeWorkspaceDirectoryName(std::wstring_view value, std::wstring_view fallback) noexcept
    {
        return terminal::workspacepaths::SanitizeWorkspaceDirectoryName(value, fallback);
    }

    std::wstring NormalizeWorkspaceColor(const std::wstring_view color) noexcept
    {
        if (color.size() != 7 || color[0] != L'#')
        {
            return {};
        }

        std::wstring normalized;
        normalized.reserve(color.size());
        normalized.push_back(L'#');

        for (size_t i = 1; i < color.size(); ++i)
        {
            if (!_isHexDigit(color[i]))
            {
                return {};
            }
            normalized.push_back(static_cast<wchar_t>(std::towupper(color[i])));
        }

        return normalized;
    }

    std::wstring PickUnusedWorkspaceColor(const std::vector<Workspace>& workspaces)
    {
        std::unordered_set<std::wstring> usedColors;
        usedColors.reserve(workspaces.size());
        for (const auto& workspace : workspaces)
        {
            if (const auto normalized = NormalizeWorkspaceColor(workspace.BackgroundColor); !normalized.empty())
            {
                usedColors.emplace(std::move(normalized));
            }
        }

        for (const auto color : _workspaceColorPalette)
        {
            if (!usedColors.contains(std::wstring{ color }))
            {
                return std::wstring{ color };
            }
        }

        return std::wstring{ _workspaceColorPalette[workspaces.size() % _workspaceColorPalette.size()] };
    }

    std::wstring MakeUniquePersistedName(const std::wstring_view baseName, std::unordered_set<std::wstring>& usedNames)
    {
        std::wstring candidate{ baseName };
        auto lowered = _toLower(candidate);
        for (size_t index = 2; !usedNames.emplace(lowered).second; ++index)
        {
            candidate = baseName;
            candidate += L" (";
            candidate += std::to_wstring(index);
            candidate += L")";
            lowered = _toLower(candidate);
        }
        return candidate;
    }

    void NormalizeWorkspacePersistableNames(Workspace& workspace)
    {
        workspace.Name = SanitizeWorkspaceDirectoryName(workspace.Name, L"workspace");
        workspace.Id = workspace.Name;

        const auto previousTabOrder = workspace.TabOrder;
        std::vector<std::wstring> originalNodeIds;
        originalNodeIds.reserve(workspace.Nodes.size());
        for (const auto& node : workspace.Nodes)
        {
            originalNodeIds.emplace_back(node.Id);
        }

        std::unordered_set<std::wstring> usedNodeNames;
        for (auto& node : workspace.Nodes)
        {
            node.Name = SanitizeWorkspaceDirectoryName(node.Name, L"tab");
            node.Name = MakeUniquePersistedName(node.Name, usedNodeNames);
            node.Id = node.Name;
        }

        std::unordered_map<std::wstring, std::wstring> normalizedNodeIds;
        for (size_t nodeIndex = 0; nodeIndex < workspace.Nodes.size() && nodeIndex < originalNodeIds.size(); ++nodeIndex)
        {
            normalizedNodeIds.emplace(originalNodeIds.at(nodeIndex), workspace.Nodes.at(nodeIndex).Id);
        }

        std::vector<std::wstring> normalizedTabOrder;
        normalizedTabOrder.reserve(previousTabOrder.size());
        for (const auto& nodeId : previousTabOrder)
        {
            if (const auto it = normalizedNodeIds.find(nodeId); it != normalizedNodeIds.end() && !it->second.empty())
            {
                normalizedTabOrder.emplace_back(it->second);
            }
        }
        workspace.TabOrder = std::move(normalizedTabOrder);
    }

    bool WorkspaceNodeEquivalent(const WorkspaceNode& lhs, const WorkspaceNode& rhs)
    {
        const auto lhsTabColor = NormalizeWorkspaceColor(lhs.TabColor).empty() ? lhs.TabColor : NormalizeWorkspaceColor(lhs.TabColor);
        const auto rhsTabColor = NormalizeWorkspaceColor(rhs.TabColor).empty() ? rhs.TabColor : NormalizeWorkspaceColor(rhs.TabColor);
        return lhs.Name == rhs.Name &&
               lhs.ProfileGuid == rhs.ProfileGuid &&
               lhs.ProfileName == rhs.ProfileName &&
               lhs.Icon == rhs.Icon &&
               lhsTabColor == rhsTabColor &&
               lhs.ShowTab == rhs.ShowTab &&
               lhs.StartupDirectory == rhs.StartupDirectory &&
               lhs.StartupAction == rhs.StartupAction &&
               lhs.OperatingSystem == rhs.OperatingSystem &&
               lhs.ShellType == rhs.ShellType &&
               lhs.ShowInputPanel == rhs.ShowInputPanel &&
               lhs.UseNodeNameAsTabTitle == rhs.UseNodeNameAsTabTitle &&
               lhs.Mirror.Mode == rhs.Mirror.Mode &&
               lhs.Mirror.MaximumEvents == rhs.Mirror.MaximumEvents &&
               lhs.Mirror.MaximumCheckpoints == rhs.Mirror.MaximumCheckpoints &&
               (lhs.ConnectionRef.empty() || rhs.ConnectionRef.empty() || lhs.ConnectionRef == rhs.ConnectionRef);
    }

    bool WorkspaceLayoutEquivalent(const Workspace& lhs, const Workspace& rhs)
    {
        if (lhs.TabOrder != rhs.TabOrder)
        {
            return false;
        }

        if (lhs.Nodes.size() != rhs.Nodes.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs.Nodes.size(); ++i)
        {
            if (!WorkspaceNodeEquivalent(lhs.Nodes[i], rhs.Nodes[i]))
            {
                return false;
            }
        }

        return true;
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
        WorkspaceSavePlan plan;
        plan.Workspaces = existingWorkspaces;

        auto workspace = capturedWorkspace;
        const auto existingWorkspaceIt = !targetWorkspaceId.empty() ?
                                             std::find_if(plan.Workspaces.begin(), plan.Workspaces.end(), [&](const auto& existingWorkspace) {
                                                 return existingWorkspace.Id == targetWorkspaceId;
                                             }) :
                                             plan.Workspaces.end();

        if (existingWorkspaceIt != plan.Workspaces.end())
        {
            workspace.Name = existingWorkspaceIt->Name;
            workspace.Description = existingWorkspaceIt->Description;
            workspace.BackgroundColor = existingWorkspaceIt->BackgroundColor;
            workspace.Locked = true;

            for (auto& node : workspace.Nodes)
            {
                if (node.ConnectionRef.empty() || node.Id.empty())
                {
                    const auto existingNodeIt = std::find_if(existingWorkspaceIt->Nodes.begin(), existingWorkspaceIt->Nodes.end(), [&](const auto& existingNode) {
                        return existingNode.Id == node.Id && !existingNode.ConnectionRef.empty();
                    });
                    if (existingNodeIt != existingWorkspaceIt->Nodes.end())
                    {
                        node.ConnectionRef = existingNodeIt->ConnectionRef;
                    }
                }
            }

            *existingWorkspaceIt = workspace;
            plan.SavedWorkspaceIndex = static_cast<size_t>(std::distance(plan.Workspaces.begin(), existingWorkspaceIt));
        }
        else
        {
            if (!explicitWorkspaceName.empty())
            {
                workspace.Name = explicitWorkspaceName;
            }
            else if (!fallbackWindowName.empty())
            {
                workspace.Name = fallbackWindowName;
            }
            else if (!fallbackTargetName.empty())
            {
                workspace.Name = fallbackTargetName;
            }
            else if (!fallbackSingleTabTitle.empty())
            {
                workspace.Name = fallbackSingleTabTitle;
            }

            if (workspace.Name.empty())
            {
                workspace.Name = generatedFallbackName;
            }

            workspace.BackgroundColor = PickUnusedWorkspaceColor(plan.Workspaces);
            workspace.Locked = true;
            plan.Workspaces.emplace_back(workspace);
            plan.SavedWorkspaceIndex = plan.Workspaces.size() - 1;
        }

        std::unordered_set<std::wstring> usedWorkspaceNames;
        for (auto& candidate : plan.Workspaces)
        {
            const auto previousTabOrder = candidate.TabOrder;
            std::vector<std::wstring> originalNodeIds;
            originalNodeIds.reserve(candidate.Nodes.size());
            for (const auto& node : candidate.Nodes)
            {
                originalNodeIds.emplace_back(node.Id);
            }

            NormalizeWorkspacePersistableNames(candidate);

            std::unordered_map<std::wstring, std::wstring> normalizedNodeIds;
            for (size_t nodeIndex = 0; nodeIndex < candidate.Nodes.size() && nodeIndex < originalNodeIds.size(); ++nodeIndex)
            {
                normalizedNodeIds.emplace(originalNodeIds.at(nodeIndex), candidate.Nodes.at(nodeIndex).Id);
            }

            std::vector<std::wstring> normalizedTabOrder;
            normalizedTabOrder.reserve(previousTabOrder.size());
            for (const auto& nodeId : previousTabOrder)
            {
                if (const auto it = normalizedNodeIds.find(nodeId); it != normalizedNodeIds.end() && !it->second.empty())
                {
                    normalizedTabOrder.emplace_back(it->second);
                }
            }
            candidate.TabOrder = std::move(normalizedTabOrder);

            candidate.Name = MakeUniquePersistedName(candidate.Name, usedWorkspaceNames);
            candidate.Id = candidate.Name;
        }

        plan.SavedWorkspace = plan.Workspaces.at(plan.SavedWorkspaceIndex);
        return plan;
    }
