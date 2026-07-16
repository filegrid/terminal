    void TerminalPage::CurrentWorkspaceId(const winrt::hstring& value)
    {
        if (_currentWorkspaceId == value)
        {
            return;
        }

        if (!value.empty())
        {
            _lastWorkspaceId = value.c_str();
        }
        else
        {
            _currentWorkspaceSaveBaseline.reset();
        }

        if (!value.empty() &&
            _currentWorkspaceId != value &&
            (!_currentWorkspaceSaveBaseline.has_value() || _currentWorkspaceSaveBaseline->Id != value.c_str()))
        {
            _currentWorkspaceSaveBaseline.reset();
        }

        _currentWorkspaceId = value;
        _UpdateWorkspaceTabRow();
        _UpdateWorkspaceInteractionState();
        _updateAllTabCloseButtons();
        _ReloadWorkspaceChatState();
        RefreshWorkspaceWindowState();
    }

    winrt::hstring TerminalPage::CurrentWorkspaceId() const noexcept
    {
        return _currentWorkspaceId;
    }

    void TerminalPage::RefreshWorkspaceWindowState()
    {
        const auto windowId = _WindowProperties.WindowId();
        if (windowId == 0)
        {
            return;
        }

        auto state = Microsoft::Terminal::Settings::Model::implementation::WorkspaceStateManager::Load();
        const auto appState = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
        if (!_currentWorkspaceId.empty())
        {
            appState.RemovePendingWorkspaceLaunch(_currentWorkspaceId);
            appState.Flush();
        }
        else
        {
            state.RemoveWindow(windowId);
            state.Save();
            return;
        }

        Microsoft::Terminal::Settings::Model::implementation::WorkspaceStateWindow window;
        window.WindowId = windowId;
        window.WindowName = _WindowProperties.WindowName().c_str();
        window.WorkspaceId = _currentWorkspaceId.c_str();
        state.UpsertWindow(std::move(window));
        state.Save();
    }

    void TerminalPage::SetStartupActions(std::vector<ActionAndArgs> actions, const winrt::hstring& workspaceId)
    {
        _startupActions = std::move(actions);
        _startupWorkspaceId = workspaceId;
    }

    void TerminalPage::_PreparePendingWorkspaceNodeStartupAction(const ActionAndArgs& action,
                                                                 const std::vector<ActionAndArgs>& actions,
                                                                 const size_t index)
    {
        _workspaceExtension->TrackWorkspaceNodeStartupAction(action, actions, index);
    }

    void TerminalPage::_RegisterWorkspaceNodeRuntimeState(const TermControl& control, const NewTerminalArgs& newTerminalArgs)
    {
        const auto contentId = control.ContentId();
        if (contentId == 0)
        {
            _workspaceExtension->ClearPendingWorkspaceNodeStartupAction();
            return;
        }

        WorkspaceNodeRuntimeState state;
        state.WorkspaceNodeId = _ConsumePendingWorkspaceNodeId();
        state.StartingDirectory = newTerminalArgs.StartingDirectory().c_str();

        const auto profile = _settings.GetProfileForArgs(newTerminalArgs);
        const auto commandline = std::wstring{ newTerminalArgs.Commandline().c_str() };
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        state.IsSshTransport = _isSshTransport(profile, commandline.empty() ? profileCommandline : commandline);
        state.HasSshTtyOption = _hasSshTtyOption(commandline.empty() ? profileCommandline : commandline);
        if (!commandline.empty() && commandline != profileCommandline)
        {
            state.ExplicitCommandline = commandline;
        }

        auto metadata = _inferRuntimeMetadataFromCommandline(commandline.empty() ? profileCommandline : commandline);
        if (metadata.OperatingSystem.empty() || metadata.ShellType.empty())
        {
            const auto profileMetadata = _inferRuntimeMetadataFromProfile(profile);
            if (metadata.OperatingSystem.empty())
            {
                metadata.OperatingSystem = profileMetadata.OperatingSystem;
            }
            if (metadata.ShellType.empty())
            {
                metadata.ShellType = profileMetadata.ShellType;
            }
        }
        state.OperatingSystem = metadata.OperatingSystem.empty() ? _inferOperatingSystemFromPath(state.StartingDirectory) : metadata.OperatingSystem;
        state.ShellType = metadata.ShellType;

        if (auto pendingStartupActionValue = _workspaceExtension->TakePendingWorkspaceNodeStartupAction())
        {
            auto pendingStartupAction = std::move(*pendingStartupActionValue);
            if (state.IsSshTransport)
            {
                const auto resolveNodeById = [&](std::wstring_view nodeId) -> std::optional<WorkspaceNode> {
                    if (nodeId.empty())
                    {
                        return std::nullopt;
                    }

                    if (const auto workspace = _SelectedWorkspaceForEditing();
                        workspace && workspace->Id == _currentWorkspaceId.c_str())
                    {
                        if (const auto it = std::find_if(workspace->Nodes.begin(), workspace->Nodes.end(), [&](const auto& node) {
                                return node.Id == nodeId;
                            });
                            it != workspace->Nodes.end())
                        {
                            return *it;
                        }
                    }

                    auto manager = WorkspaceManager::Load();
                    if (const auto workspace = manager.FindById(_currentWorkspaceId.c_str()))
                    {
                        if (const auto it = std::find_if(workspace->Nodes.begin(), workspace->Nodes.end(), [&](const auto& node) {
                                return node.Id == nodeId;
                            });
                            it != workspace->Nodes.end())
                        {
                            return *it;
                        }
                    }

                    return std::nullopt;
                };

                if (const auto node = resolveNodeById(state.WorkspaceNodeId))
                {
                    if (state.StartingDirectory.empty())
                    {
                        state.StartingDirectory = node->StartupDirectory;
                    }
                    if (state.OperatingSystem.empty())
                    {
                        state.OperatingSystem = node->OperatingSystem;
                    }
                    if (state.ShellType.empty())
                    {
                        state.ShellType = node->ShellType;
                    }

                    state.StartupAction = node->StartupAction;
                    state.DeferredStartupInputs = _buildSshDeferredStartupInputs(*node);
                }
                else
                {
                    state.StartupAction = pendingStartupAction;
                }

                if (state.DeferredStartupInputs.empty() && !pendingStartupAction.empty())
                {
                    state.DeferredStartupInputs.emplace_back(std::move(pendingStartupAction));
                }

                state.StartupInputPending = !state.DeferredStartupInputs.empty();
                state.StartupInputDispatched = false;
                _workspaceExtension->SetPendingWorkspaceNodeStartupSendInputSkip(state.StartupInputPending);
            }
            else
            {
                state.StartupAction = std::move(pendingStartupAction);
            }
        }

        if (!state.WorkspaceNodeId.empty() ||
            !state.StartupAction.empty() ||
            !state.ExplicitCommandline.empty() ||
            !state.StartingDirectory.empty() ||
            !state.OperatingSystem.empty() ||
            !state.ShellType.empty())
        {
            const auto operatingSystem = state.OperatingSystem;
            const auto shellType = state.ShellType;
            _workspaceExtension->UpsertWorkspaceNodeRuntimeState(contentId, state);

            auto& inputState = _workspaceExtension->WorkspaceChatTerminalStates()[_WorkspaceChatStateKey(control)].InputState;
            if (inputState.OperatingSystem.empty())
            {
                inputState.OperatingSystem = operatingSystem;
            }
            if (inputState.ShellType.empty())
            {
                inputState.ShellType = shellType;
            }
        }
        else
        {
            _workspaceExtension->RemoveWorkspaceNodeRuntimeState(contentId);
        }
    }

    safe_void_coroutine TerminalPage::_ReplayPendingWorkspaceStartupInput(const TermControl& control, const ICoreState& coreState)
    {
        const auto contentId = control.ContentId();
        if (auto state = _workspaceExtension->FindWorkspaceNodeRuntimeState(contentId);
            state &&
            state->StartupInputPending &&
            !state->StartupInputDispatched &&
            (!state->DeferredStartupInputs.empty() || !state->DeferredStartupInput.empty()))
        {
            const auto inputs = !state->DeferredStartupInputs.empty() ? state->DeferredStartupInputs : _splitDeferredStartupInput(state->DeferredStartupInput);
            const auto operatingSystem = state->OperatingSystem;
            const auto shouldWaitForFinalShell = _shouldWaitForSshTtyStartupReplay(operatingSystem, state->HasSshTtyOption);
            state->StartupInputDispatched = true;
            state->StartupInputPending = false;

            if (shouldWaitForFinalShell)
            {
                auto waited = 0ms;
                while (!_isSshTtyStartupReplayReady(control, operatingSystem))
                {
                    if (_workspaceExtension->FindWorkspaceNodeRuntimeState(contentId) == nullptr)
                    {
                        co_return;
                    }

                    if (coreState.ConnectionState() >= ConnectionState::Closed)
                    {
                        co_return;
                    }

                    if (waited >= WorkspaceStartupSshTtyReadyTimeout)
                    {
                        break;
                    }

                    co_await winrt::resume_after(WorkspaceStartupSshTtyReadyPollDelay);
                    waited += WorkspaceStartupSshTtyReadyPollDelay;
                    co_await wil::resume_foreground(Dispatcher());
                }
            }

            co_await winrt::resume_after(WorkspaceStartupInitialReplayDelay);
            co_await wil::resume_foreground(Dispatcher());

            for (size_t index = 0; index < inputs.size(); ++index)
            {
                if (index > 0)
                {
                    co_await winrt::resume_after(WorkspaceStartupCommandReplayDelay);
                    co_await wil::resume_foreground(Dispatcher());
                }

                auto payload = _appendDeferredStartupSubmit(inputs[index]);
                control.SendInput(winrt::hstring{ payload });
            }
        }
    }

    void TerminalPage::_RegisterWorkspaceNodeRuntimeStateIfNeeded(const TermControl& control, const NewTerminalArgs& newTerminalArgs)
    {
        if (newTerminalArgs)
        {
            _RegisterWorkspaceNodeRuntimeState(control, newTerminalArgs);
        }
    }

    std::wstring TerminalPage::_ResolveWorkspaceNodeStartupAction(const winrt::com_ptr<Tab>& tab,
                                                                  const NewTerminalArgs& terminalArgs) const
    {
        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                const auto& terminalStates = _workspaceExtension->WorkspaceChatTerminalStates();
                if (const auto captureState = terminalStates.find(stateKey);
                    captureState != terminalStates.end())
                {
                    if (terminal::workspacechat::ContainsLineBreak(captureState->second.LastSubmittedInput))
                    {
                        return captureState->second.LastSubmittedInput;
                    }

                    if (!captureState->second.InputState.LastCommand.empty())
                    {
                        return captureState->second.InputState.LastCommand;
                    }

                    if (!captureState->second.LastSubmittedInput.empty())
                    {
                        return captureState->second.LastSubmittedInput;
                    }
                }
            }

            if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tab))
            {
                if (!runtimeState->StartupAction.empty())
                {
                    return runtimeState->StartupAction;
                }
                if (!runtimeState->ExplicitCommandline.empty())
                {
                    return runtimeState->ExplicitCommandline;
                }
            }
        }

        if (const auto node = _ResolveCurrentWorkspaceNode(tab))
        {
            return node->StartupAction;
        }

        const auto profile = _settings.GetProfileForArgs(terminalArgs);
        const auto commandline = std::wstring{ terminalArgs.Commandline().c_str() };
        if (!commandline.empty())
        {
            const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
            if (commandline != profileCommandline)
            {
                return commandline;
            }
        }

        return {};
    }

    std::wstring TerminalPage::_ResolveWorkspaceNodeStartingDirectory(const winrt::com_ptr<Tab>& tab,
                                                                      const NewTerminalArgs& terminalArgs) const
    {
        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                const auto& terminalStates = _workspaceExtension->WorkspaceChatTerminalStates();
                if (const auto captureState = terminalStates.find(stateKey);
                    captureState != terminalStates.end() &&
                    !captureState->second.InputState.LastWorkingDirectory.empty())
                {
                    return captureState->second.InputState.LastWorkingDirectory;
                }

                if (const auto trackedWorkingDirectory = _ResolveTrackedTerminalWorkingDirectory(control);
                    !trackedWorkingDirectory.empty())
                {
                    return trackedWorkingDirectory;
                }
            }
        }

        if (const auto node = _ResolveCurrentWorkspaceNode(tab))
        {
            return node->StartupDirectory;
        }

        return std::wstring{ terminalArgs.StartingDirectory().c_str() };
    }

    std::wstring TerminalPage::_ResolveWorkspaceNodeOperatingSystem(const winrt::com_ptr<Tab>& tab,
                                                                    const NewTerminalArgs& terminalArgs) const
    {
        const auto profile = _settings.GetProfileForArgs(terminalArgs);
        const auto profileMetadata = _inferRuntimeMetadataFromProfile(profile);
        if (profileMetadata.ShellType == L"wsl")
        {
            return L"linux";
        }

        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                const auto& terminalStates = _workspaceExtension->WorkspaceChatTerminalStates();
                if (const auto captureState = terminalStates.find(stateKey);
                    captureState != terminalStates.end() &&
                    !captureState->second.InputState.OperatingSystem.empty())
                {
                    return captureState->second.InputState.OperatingSystem;
                }
            }

            if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tab);
                runtimeState && !runtimeState->OperatingSystem.empty())
            {
                return runtimeState->OperatingSystem;
            }
        }

        const auto commandline = std::wstring{ terminalArgs.Commandline().c_str() };
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        auto metadata = _inferRuntimeMetadataFromCommandline(commandline.empty() ? profileCommandline : commandline);
        if (metadata.OperatingSystem.empty())
        {
            metadata = _inferRuntimeMetadataFromProfile(profile);
        }
        if (!metadata.OperatingSystem.empty())
        {
            return metadata.OperatingSystem;
        }

        if (const auto node = _ResolveCurrentWorkspaceNode(tab))
        {
            return node->OperatingSystem;
        }

        const auto startingDirectory = std::wstring{ terminalArgs.StartingDirectory().c_str() };
        return _inferOperatingSystemFromPath(startingDirectory);
    }

    std::wstring TerminalPage::_ResolveWorkspaceNodeShellType(const winrt::com_ptr<Tab>& tab,
                                                              const NewTerminalArgs& terminalArgs) const
    {
        const auto profile = _settings.GetProfileForArgs(terminalArgs);
        const auto profileMetadata = _inferRuntimeMetadataFromProfile(profile);
        if (profileMetadata.ShellType == L"wsl")
        {
            return profileMetadata.ShellType;
        }

        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                const auto& terminalStates = _workspaceExtension->WorkspaceChatTerminalStates();
                if (const auto captureState = terminalStates.find(stateKey);
                    captureState != terminalStates.end() &&
                    !captureState->second.InputState.ShellType.empty())
                {
                    return captureState->second.InputState.ShellType;
                }
            }

            if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tab);
                runtimeState && !runtimeState->ShellType.empty())
            {
                return runtimeState->ShellType;
            }
        }

        const auto commandline = std::wstring{ terminalArgs.Commandline().c_str() };
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        auto metadata = _inferRuntimeMetadataFromCommandline(commandline.empty() ? profileCommandline : commandline);
        if (metadata.ShellType.empty())
        {
            metadata = _inferRuntimeMetadataFromProfile(profile);
        }
        if (!metadata.ShellType.empty())
        {
            return metadata.ShellType;
        }

        if (const auto node = _ResolveCurrentWorkspaceNode(tab))
        {
            return node->ShellType;
        }

        return {};
    }
