    const Workspace* WorkspaceManager::FindById(const std::wstring_view id) const noexcept
    {
        const auto it = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == id;
        });
        return it == _workspaces.end() ? nullptr : &*it;
    }

    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes)
    {
        const auto visibleNodeCount = std::count_if(workspace.Nodes.begin(), workspace.Nodes.end(), [](const auto& node) {
            return node.ShowTab;
        });
        if (orderedVisibleNodes.size() < gsl::narrow_cast<size_t>(visibleNodeCount))
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
        return true;
    }

    bool WorkspaceManager::ReorderWorkspaceNodes(const std::wstring_view workspaceId, const std::vector<std::wstring>& orderedNodeIds)
    {
        const auto workspaceIt = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == _workspaces.end())
        {
            return false;
        }

        auto& workspace = *workspaceIt;
        const auto visibleNodeCount = std::count_if(workspace.Nodes.begin(), workspace.Nodes.end(), [](const auto& node) {
            return node.ShowTab;
        });
        if (orderedNodeIds.size() != gsl::narrow_cast<size_t>(visibleNodeCount))
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

        return ApplyVisibleWorkspaceNodeOrder(workspace, orderedVisibleNodes);
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

    void EnsureWorkspaceNodeTabColors(Workspace& workspace, const Model::CascadiaSettings& settings)
    {
        for (size_t i = 0; i < workspace.Nodes.size(); ++i)
        {
            if (const auto color = ResolveWorkspaceNodeTabColor(workspace, i, settings))
            {
                workspace.Nodes.at(i).TabColor = _colorToString(*color);
            }
        }
    }

    std::vector<Model::ActionAndArgs> WorkspaceManager::BuildStartupActions(const Workspace& workspace, const Model::CascadiaSettings& settings) const
    {
        std::vector<Model::ActionAndArgs> actions;

        for (size_t nodeIndex = 0; nodeIndex < workspace.Nodes.size(); ++nodeIndex)
        {
            const auto& node = workspace.Nodes.at(nodeIndex);
            if (!node.ShowTab)
            {
                continue;
            }

            Model::NewTerminalArgs terminalArgs;

            if (!node.Name.empty())
            {
                terminalArgs.TabTitle(node.Name);
            }
            terminalArgs.SuppressApplicationTitle(node.UseNodeNameAsTabTitle);
            const auto profile = _resolveNodeProfile(node, settings);

            if (!profile)
            {
                continue;
            }

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

    const std::vector<WorkspaceStateWindow>& WorkspaceStateManager::Windows() const noexcept
    {
        return _windows;
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
