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

    WorkspaceNodeLaunchResolution ResolveWorkspaceNodeLaunchResolution(const WorkspaceNodeLaunchResolutionPlanInput& input)
    {
        return _fromCoreNodeLaunchResolution(terminal::workspace::ResolveWorkspaceNodeLaunchResolution(_toCoreNodeLaunchResolutionPlanInput(input)));
    }

    std::wstring ResolveTrackedWorkspaceDirectory(const WorkspaceTrackedDirectoryInput& input)
    {
        return terminal::workspace::ResolveTrackedWorkspaceDirectory(_toCoreTrackedDirectoryInput(input));
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
            // Exactly one first-level Tab is created for each node. Command
            // windows are children of that Tab's TerminalContentWrapper and
            // must never consume this native-tab registration queue.
            state.PendingNodeInputVisibility.emplace_back(node.ShowInputPanel);
            state.PendingNodeIds.emplace_back(node.Id);
        }
        return state;
    }

    std::vector<WorkspaceNodeCommandLaunch> WorkspaceManager::BuildNodeCommandLaunches(const Workspace& workspace,
                                                                                         const size_t nodeIndex,
                                                                                         const Model::CascadiaSettings& settings) const
    {
        std::vector<WorkspaceNodeCommandLaunch> launches;
        if (nodeIndex >= workspace.Nodes.size())
        {
            return launches;
        }

        const auto& node = workspace.Nodes.at(nodeIndex);
        const auto profile = _resolveNodeProfile(node, settings);
        if (!profile)
        {
            return launches;
        }

        const auto isSshTransport = _isSshTransportNode(node, profile);
        const auto isWslTransport = _isWslProfileSource(profile.Source().c_str()) ||
                                    _isWslCommandline(profile.Commandline().c_str());
        const auto commands = node.Commands.empty() ?
                                  std::vector<WorkspaceNodeCommand>{ WorkspaceNodeCommand{ node.Id + L":legacy-command", node.Icon, node.Name, node.StartupAction } } :
                                  node.Commands;
        launches.reserve(commands.size());
        for (const auto& command : commands)
        {
            Model::NewTerminalArgs terminalArgs;
            terminalArgs.TabTitle(!command.Name.empty() ? command.Name : node.Name);
            terminalArgs.SuppressApplicationTitle(node.UseNodeNameAsTabTitle);
            terminalArgs.Profile(Utils::GuidToString(profile.Guid()));
            if (isSshTransport)
            {
                terminalArgs.Commandline(profile.Commandline());
                terminalArgs.StartingDirectory(profile.EvaluatedStartingDirectory());
            }
            else if (!isWslTransport && !node.StartupDirectory.empty())
            {
                terminalArgs.StartingDirectory(node.StartupDirectory);
            }
            if (const auto tabColor = ResolveWorkspaceNodeTabColor(workspace, nodeIndex, settings))
            {
                terminalArgs.TabColor(*tabColor);
            }

            auto startupInput = isSshTransport ? _buildSshStartupInput(node) :
                                isWslTransport ? _buildWslStartupInput(node) : std::wstring{};
            launches.emplace_back(WorkspaceNodeCommandLaunch{
                .TerminalArgs = terminalArgs,
                .StartupInput = _appendStartupCommand(std::move(startupInput), command.Command),
            });
        }
        return launches;
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

    WorkspaceNodeRuntimeStatePlan PrepareWorkspaceNodeRuntimeState(const WorkspaceNodeRuntimeRegistrationInput& input)
    {
        return _fromCoreRuntimeStatePlan(terminal::workspace::PrepareWorkspaceNodeRuntimeState(_toCoreRuntimeRegistrationInput(input)));
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

    WorkspaceSshStartupPlan LoadWorkspaceSshStartupPlan(const std::wstring_view currentWorkspaceId,
                                                        const std::optional<Workspace>& selectedWorkspace,
                                                        const std::wstring_view workspaceNodeId,
                                                        const std::wstring_view pendingStartupAction,
                                                        const std::wstring_view startingDirectory,
                                                        const std::wstring_view operatingSystem,
                                                        const std::wstring_view shellType)
    {
        return PrepareSshStartupPlan(pendingStartupAction,
                                     startingDirectory,
                                     operatingSystem,
                                     shellType,
                                     LoadResolvedWorkspaceNode(currentWorkspaceId, selectedWorkspace, workspaceNodeId));
    }

    void EnsureWorkspaceNodeTabColors(Workspace& workspace, const Model::CascadiaSettings& settings)
    {
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

            const auto replacement = PickWorkspacePaletteColor(usedColors, i);
            if (!replacement.empty())
            {
                workspace.Nodes.at(i).TabColor = replacement;
                usedColors.emplace(workspace.Nodes.at(i).TabColor);
            }
            else if (const auto fallback = ResolveWorkspaceNodeTabColor(workspace, i, settings))
            {
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
            const auto profile = resolvedNode.Profile;

            const auto commands = node.Commands.empty() ? std::vector<WorkspaceNodeCommand>{ WorkspaceNodeCommand{ node.Id + L":legacy-command", node.Icon, node.Name, node.StartupAction } } : node.Commands;
            const auto launches = BuildNodeCommandLaunches(workspace, nodeIndex, settings);
            // A multi-command node is one logical workspace node. In split mode
            // its commands must therefore be created in the same tab, not as a
            // series of unrelated tabs. Build a right-growing binary split: the
            // first split reserves the aggregate size for all remaining panes;
            // every following split divides that remainder. This preserves the
            // configured absolute weights for two and three command windows.
            auto weights = node.MultiWindowPreference.SplitWeights;
            if (weights.size() != commands.size())
            {
                weights.assign(commands.size(), 1.0 / static_cast<double>(commands.size()));
            }
            for (size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
            {
                const auto displayMode = node.MultiWindowPreference.DisplayMode;
                if (commandIndex >= launches.size())
                {
                    continue;
                }
                const auto& launch = launches[commandIndex];
                const auto& terminalArgs = launch.TerminalArgs;
                // INVARIANT: first-level native tabs are created from workspace
                // nodes only. Commands never create native tabs, regardless of
                // display mode or command count. TabHost owns command-level UI.
                if (commandIndex == 0)
                {
                    Model::ActionAndArgs newTabAction;
                    newTabAction.Action(ShortcutAction::NewTab);
                    newTabAction.Args(Model::NewTabArgs{ terminalArgs });
                    actions.emplace_back(std::move(newTabAction));
                }
                else if (displayMode == WorkspaceWindowDisplayMode::Split)
                {
                    // The active pane is the previous command's remaining
                    // subtree. For three windows, the first split must give
                    // the right subtree (windows 2 + 3) its combined share,
                    // then the next split divides that subtree. Dividing each
                    // command by a running global remainder produced the
                    // wrong 3-pane proportions.
                    const auto activeSubtreeWeight = std::accumulate(weights.begin() + commandIndex - 1, weights.end(), 0.0);
                    const auto newSubtreeWeight = std::accumulate(weights.begin() + commandIndex, weights.end(), 0.0);
                    const auto newPaneSize = static_cast<float>(std::clamp(newSubtreeWeight / activeSubtreeWeight, 0.05, 0.95));
                    Model::ActionAndArgs splitAction;
                    splitAction.Action(ShortcutAction::SplitPane);
                    splitAction.Args(Model::SplitPaneArgs{ Model::SplitType::Manual, Model::SplitDirection::Right, newPaneSize, terminalArgs });
                    actions.emplace_back(std::move(splitAction));
                }

                // Tab-mode command hosts are deliberately not represented by
                // native startup actions. TerminalContentWrapper consumes the
                // node command list and owns their creation in TabHost.
                if (displayMode == WorkspaceWindowDisplayMode::Tab && commandIndex > 0)
                {
                    continue;
                }

                if (!launch.StartupInput.empty())
                {
                    Model::ActionAndArgs startupAction;
                    startupAction.Action(ShortcutAction::SendInput);
                    startupAction.Args(Model::SendInputArgs{ winrt::hstring{ launch.StartupInput } });
                    actions.emplace_back(std::move(startupAction));
                }
            }
        }
        return actions;
    }
