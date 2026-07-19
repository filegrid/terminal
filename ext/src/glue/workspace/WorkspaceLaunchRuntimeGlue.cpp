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

        const auto appState = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
        if (!_currentWorkspaceId.empty())
        {
            appState.RemovePendingWorkspaceLaunch(_currentWorkspaceId);
            appState.Flush();
        }
        ::terminal::workspace::RefreshPersistedWorkspaceWindowState(windowId,
                                                                    _WindowProperties.WindowName().c_str(),
                                                                    _currentWorkspaceId.c_str());
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

        const auto profile = _settings.GetProfileForArgs(newTerminalArgs);
        const auto profileSource = profile ? std::wstring{ profile.Source().c_str() } : std::wstring{};
        const auto commandline = std::wstring{ newTerminalArgs.Commandline().c_str() };
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        const auto runtimeLaunchState = Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceRuntimeLaunchState(newTerminalArgs.StartingDirectory().c_str(),
                                                                                                                                profileSource,
                                                                                                                                profileCommandline,
                                                                                                                                commandline);
        state.StartingDirectory = runtimeLaunchState.StartingDirectory;
        state.ExplicitCommandline = runtimeLaunchState.ExplicitCommandline;
        state.OperatingSystem = runtimeLaunchState.OperatingSystem;
        state.ShellType = runtimeLaunchState.ShellType;
        state.IsSshTransport = runtimeLaunchState.IsSshTransport;
        state.HasSshTtyOption = runtimeLaunchState.HasSshTtyOption;

        if (auto pendingStartupActionValue = _workspaceExtension->TakePendingWorkspaceNodeStartupAction())
        {
            auto pendingStartupAction = std::move(*pendingStartupActionValue);
            if (state.IsSshTransport)
            {
                const auto selectedWorkspace = [&]() -> std::optional<Workspace> {
                    if (const auto workspace = _SelectedWorkspaceForEditing();
                        workspace && workspace->Id == _currentWorkspaceId.c_str())
                    {
                        return *workspace;
                    }
                    return std::nullopt;
                }();
                const auto resolvedNode = ::terminal::workspace::LoadResolvedWorkspaceNode(_currentWorkspaceId.c_str(),
                                                                                           selectedWorkspace,
                                                                                           state.WorkspaceNodeId);
                const auto sshStartupPlan = Microsoft::Terminal::Settings::Model::implementation::PrepareSshStartupPlan(pendingStartupAction,
                                                                                                                          state.StartingDirectory,
                                                                                                                          state.OperatingSystem,
                                                                                                                          state.ShellType,
                                                                                                                          resolvedNode);
                state.StartupAction = sshStartupPlan.StartupAction;
                state.StartingDirectory = sshStartupPlan.StartingDirectory;
                state.OperatingSystem = sshStartupPlan.OperatingSystem;
                state.ShellType = sshStartupPlan.ShellType;
                state.DeferredStartupInputs = sshStartupPlan.DeferredStartupInputs;
                state.StartupInputPending = sshStartupPlan.StartupInputPending;
                state.StartupInputDispatched = sshStartupPlan.StartupInputDispatched;
                _workspaceExtension->SetPendingWorkspaceNodeStartupSendInputSkip(state.StartupInputPending);
            }
            else
            {
                state.StartupAction = std::move(pendingStartupAction);
                if (!state.StartupAction.empty())
                {
                    state.DeferredStartupInputs = { state.StartupAction };
                    state.StartupInputPending = true;
                    state.StartupInputDispatched = false;
                    _workspaceExtension->SetPendingWorkspaceNodeStartupSendInputSkip(true);
                }
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

            auto& captureState = _workspaceExtension->GetOrCreateWorkspaceChatTerminalState(_WorkspaceChatStateKey(control));
            terminal::workspacechat::SeedCapturedShellMetadata(captureState, operatingSystem, shellType);
        }
        else
        {
            _workspaceExtension->RemoveWorkspaceNodeRuntimeState(contentId);
        }
    }

    safe_void_coroutine TerminalPage::_ReplayPendingWorkspaceStartupInput(TermControl control, ICoreState coreState)
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
            if (_workspaceExtension->FindWorkspaceNodeRuntimeState(contentId) == nullptr ||
                coreState.ConnectionState() >= ConnectionState::Closed)
            {
                co_return;
            }

            for (size_t index = 0; index < inputs.size(); ++index)
            {
                if (index > 0)
                {
                    co_await winrt::resume_after(WorkspaceStartupCommandReplayDelay);
                    co_await wil::resume_foreground(Dispatcher());
                    if (_workspaceExtension->FindWorkspaceNodeRuntimeState(contentId) == nullptr ||
                        coreState.ConnectionState() >= ConnectionState::Closed)
                    {
                        co_return;
                    }
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
        const auto profile = _settings.GetProfileForArgs(terminalArgs);
        const auto commandline = std::wstring{ terminalArgs.Commandline().c_str() };
        const auto profileSource = profile ? std::wstring{ profile.Source().c_str() } : std::wstring{};
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionInput input;
        input.ProfileSource = profileSource;
        input.ProfileCommandline = profileCommandline;
        input.TerminalCommandline = commandline;
        input.TerminalStartingDirectory = terminalArgs.StartingDirectory().c_str();
        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                if (const auto captureState = _workspaceExtension->FindWorkspaceChatTerminalState(stateKey))
                {
                    input.ObservedStartupAction = terminal::workspacechat::ResolveCapturedStartupAction(*captureState);
                }
            }

            if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tab))
            {
                input.RuntimeStartupAction = runtimeState->StartupAction;
                input.RuntimeExplicitCommandline = runtimeState->ExplicitCommandline;
                input.RuntimeStartingDirectory = runtimeState->StartingDirectory;
                input.RuntimeOperatingSystem = runtimeState->OperatingSystem;
                input.RuntimeShellType = runtimeState->ShellType;
            }

            input.PersistedNode = _ResolveCurrentWorkspaceNode(tab);
        }

        return Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeLaunchResolution(input).StartupAction;
    }

    std::wstring TerminalPage::_ResolveWorkspaceNodeStartingDirectory(const winrt::com_ptr<Tab>& tab,
                                                                      const NewTerminalArgs& terminalArgs) const
    {
        const auto profile = _settings.GetProfileForArgs(terminalArgs);
        const auto commandline = std::wstring{ terminalArgs.Commandline().c_str() };
        const auto profileSource = profile ? std::wstring{ profile.Source().c_str() } : std::wstring{};
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionInput input;
        input.ProfileSource = profileSource;
        input.ProfileCommandline = profileCommandline;
        input.TerminalCommandline = commandline;
        input.TerminalStartingDirectory = terminalArgs.StartingDirectory().c_str();
        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                if (const auto captureState = _workspaceExtension->FindWorkspaceChatTerminalState(stateKey))
                {
                    input.ObservedWorkingDirectory = terminal::workspacechat::ResolveCapturedWorkingDirectory(*captureState);
                    input.ObservedOperatingSystem = terminal::workspacechat::ResolveCapturedOperatingSystem(*captureState);
                    input.ObservedShellType = terminal::workspacechat::ResolveCapturedShellType(*captureState);
                }

                if (input.ObservedWorkingDirectory.empty())
                {
                    input.ObservedWorkingDirectory = _ResolveTrackedTerminalWorkingDirectory(control);
                }
            }

            if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tab))
            {
                input.RuntimeStartupAction = runtimeState->StartupAction;
                input.RuntimeExplicitCommandline = runtimeState->ExplicitCommandline;
                input.RuntimeStartingDirectory = runtimeState->StartingDirectory;
                input.RuntimeOperatingSystem = runtimeState->OperatingSystem;
                input.RuntimeShellType = runtimeState->ShellType;
            }

            input.PersistedNode = _ResolveCurrentWorkspaceNode(tab);
        }

        return Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeLaunchResolution(input).StartingDirectory;
    }

    std::wstring TerminalPage::_ResolveWorkspaceNodeOperatingSystem(const winrt::com_ptr<Tab>& tab,
                                                                    const NewTerminalArgs& terminalArgs) const
    {
        const auto profile = _settings.GetProfileForArgs(terminalArgs);
        const auto commandline = std::wstring{ terminalArgs.Commandline().c_str() };
        const auto profileSource = profile ? std::wstring{ profile.Source().c_str() } : std::wstring{};
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionInput input;
        input.ProfileSource = profileSource;
        input.ProfileCommandline = profileCommandline;
        input.TerminalCommandline = commandline;
        input.TerminalStartingDirectory = terminalArgs.StartingDirectory().c_str();
        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                if (const auto captureState = _workspaceExtension->FindWorkspaceChatTerminalState(stateKey))
                {
                    input.ObservedOperatingSystem = terminal::workspacechat::ResolveCapturedOperatingSystem(*captureState);
                    input.ObservedShellType = terminal::workspacechat::ResolveCapturedShellType(*captureState);
                }
            }

            if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tab))
            {
                input.RuntimeStartupAction = runtimeState->StartupAction;
                input.RuntimeExplicitCommandline = runtimeState->ExplicitCommandline;
                input.RuntimeStartingDirectory = runtimeState->StartingDirectory;
                input.RuntimeOperatingSystem = runtimeState->OperatingSystem;
                input.RuntimeShellType = runtimeState->ShellType;
            }

            input.PersistedNode = _ResolveCurrentWorkspaceNode(tab);
        }

        return Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeLaunchResolution(input).OperatingSystem;
    }

    std::wstring TerminalPage::_ResolveWorkspaceNodeShellType(const winrt::com_ptr<Tab>& tab,
                                                              const NewTerminalArgs& terminalArgs) const
    {
        const auto profile = _settings.GetProfileForArgs(terminalArgs);
        const auto commandline = std::wstring{ terminalArgs.Commandline().c_str() };
        const auto profileSource = profile ? std::wstring{ profile.Source().c_str() } : std::wstring{};
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionInput input;
        input.ProfileSource = profileSource;
        input.ProfileCommandline = profileCommandline;
        input.TerminalCommandline = commandline;
        input.TerminalStartingDirectory = terminalArgs.StartingDirectory().c_str();
        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                const auto stateKey = _WorkspaceChatStateKey(control);
                if (const auto captureState = _workspaceExtension->FindWorkspaceChatTerminalState(stateKey))
                {
                    input.ObservedOperatingSystem = terminal::workspacechat::ResolveCapturedOperatingSystem(*captureState);
                    input.ObservedShellType = terminal::workspacechat::ResolveCapturedShellType(*captureState);
                }
            }

            if (const auto runtimeState = _FindWorkspaceNodeRuntimeState(tab))
            {
                input.RuntimeStartupAction = runtimeState->StartupAction;
                input.RuntimeExplicitCommandline = runtimeState->ExplicitCommandline;
                input.RuntimeStartingDirectory = runtimeState->StartingDirectory;
                input.RuntimeOperatingSystem = runtimeState->OperatingSystem;
                input.RuntimeShellType = runtimeState->ShellType;
            }

            input.PersistedNode = _ResolveCurrentWorkspaceNode(tab);
        }

        return Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeLaunchResolution(input).ShellType;
    }
