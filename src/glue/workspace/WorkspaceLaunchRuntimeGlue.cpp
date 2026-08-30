    void TerminalPage::CurrentWorkspaceId(const winrt::hstring& value)
    {
        _workspaceExtension->SetCurrentWorkspaceId(value);
    }

    winrt::hstring TerminalPage::CurrentWorkspaceId() const noexcept
    {
        return _currentWorkspaceId;
    }

    void TerminalPage::RefreshWorkspaceWindowState()
    {
        _workspaceExtension->RefreshWorkspaceWindowState();
    }

    void TerminalPage::SetStartupActions(std::vector<ActionAndArgs> actions, const winrt::hstring& workspaceId)
    {
        _startupActions = std::move(actions);
        _startupWorkspaceId = workspaceId;
    }

    void TerminalPage::_RegisterWorkspaceNodeRuntimeState(const TermControl& control, const NewTerminalArgs& newTerminalArgs)
    {
        const auto contentId = control.ContentId();
        if (contentId == 0)
        {
            _workspaceExtension->ClearPendingWorkspaceNodeStartupAction();
            return;
        }

        const auto profile = _settings.GetProfileForArgs(newTerminalArgs);
        const auto profileSource = profile ? std::wstring{ profile.Source().c_str() } : std::wstring{};
        const auto commandline = std::wstring{ newTerminalArgs.Commandline().c_str() };
        const auto profileCommandline = profile ? std::wstring{ profile.Commandline().c_str() } : std::wstring{};
        const auto workspaceNodeId = _workspaceExtension->ConsumePendingWorkspaceNodeId();
        const auto pendingStartupActionValue = _workspaceExtension->TakePendingWorkspaceNodeStartupAction();
        const auto selectedWorkspace = [&]() -> std::optional<Workspace> {
            if (const auto workspace = _SelectedWorkspaceForEditing();
                workspace && workspace->Id == _currentWorkspaceId.c_str())
            {
                return *workspace;
            }
            return std::nullopt;
        }();
        WorkspaceNodeRuntimeRegistrationInput registrationInput;
        registrationInput.WorkspaceNodeId = workspaceNodeId;
        registrationInput.PendingStartupAction = pendingStartupActionValue.value_or(std::wstring{});
        registrationInput.StartingDirectory = newTerminalArgs.StartingDirectory().c_str();
        registrationInput.ProfileSource = profileSource;
        registrationInput.ProfileCommandline = profileCommandline;
        registrationInput.TerminalCommandline = commandline;
        registrationInput.CurrentWorkspaceId = _currentWorkspaceId.c_str();
        registrationInput.SelectedWorkspace = selectedWorkspace;
        const auto runtimeStatePlan = Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceNodeRuntimeState(registrationInput);

        WorkspaceNodeRuntimeState state;
        state.WorkspaceNodeId = runtimeStatePlan.WorkspaceNodeId;
        state.StartupAction = runtimeStatePlan.StartupAction;
        state.ExplicitCommandline = runtimeStatePlan.ExplicitCommandline;
        state.StartingDirectory = runtimeStatePlan.StartingDirectory;
        state.OperatingSystem = runtimeStatePlan.OperatingSystem;
        state.ShellType = runtimeStatePlan.ShellType;
        state.IsSshTransport = runtimeStatePlan.IsSshTransport;
        state.HasSshTtyOption = runtimeStatePlan.HasSshTtyOption;
        state.DeferredStartupInputs = runtimeStatePlan.DeferredStartupInputs;
        state.StartupInputPending = runtimeStatePlan.StartupInputPending;
        state.StartupInputDispatched = runtimeStatePlan.StartupInputDispatched;
        {
            Json::Value payload{ Json::objectValue };
            payload["contentId"] = Json::UInt64{ contentId };
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", _currentWorkspaceId.c_str());
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceNodeId", state.WorkspaceNodeId);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "profileSource", profileSource);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "terminalStartingDirectory", newTerminalArgs.StartingDirectory().c_str());
            terminal::workspacechat::AddDiagnosticTextFields(payload, "terminalCommandline", commandline);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "profileCommandline", profileCommandline);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "resolvedStartingDirectory", state.StartingDirectory);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "resolvedCommandline", state.ExplicitCommandline);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "resolvedOperatingSystem", state.OperatingSystem);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "resolvedShellType", state.ShellType);
            payload["isSshTransport"] = state.IsSshTransport;
            payload["hasSshTtyOption"] = state.HasSshTtyOption;
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_node_runtime_state_registered", payload);
        }
        _workspaceExtension->SetPendingWorkspaceNodeStartupSendInputSkip(runtimeStatePlan.SkipPendingStartupSendInput);

        if (runtimeStatePlan.HasRuntimeState)
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
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionPlanInput input;
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
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionPlanInput input;
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
                input.TrackedWorkingDirectory = _ResolveTrackedTerminalWorkingDirectory(control);
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
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionPlanInput input;
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
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeLaunchResolutionPlanInput input;
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
