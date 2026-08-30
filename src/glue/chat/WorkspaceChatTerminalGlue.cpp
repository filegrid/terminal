    std::wstring TerminalPage::_CurrentWorkspaceStorageKey() const
    {
        return terminal::workspacechat::WorkspaceStorageKey(_currentWorkspaceId.c_str(), _WindowProperties.WindowId());
    }

    std::wstring TerminalPage::_ResolveWorkspaceArtifactTabKey(const winrt::com_ptr<Tab>& tab) const
    {
        if (!tab)
        {
            return L"tab";
        }

        if (const auto nodeId = _ResolveLiveCurrentWorkspaceNodeId(tab); !nodeId.empty())
        {
            return nodeId;
        }

        const auto title = _trimWorkspaceNodeValue(tab->Title().c_str());
        if (!title.empty())
        {
            return title;
        }

        if (const auto index = _GetTabIndex(*tab))
        {
            return fmt::format(L"tab-{}", *index + 1);
        }

        return L"tab";
    }

    std::wstring TerminalPage::_CurrentWorkspaceArtifactTabKey() const
    {
        return _ResolveWorkspaceArtifactTabKey(_GetFocusedTabImpl());
    }

    std::wstring TerminalPage::_TrimmedWorkspaceChatInput()
    {
        return terminal::workspacechat::TrimWorkspaceChatText(WorkspaceChatInput().Text().c_str());
    }

    std::optional<TerminalPage::TerminalRoutingContext> TerminalPage::_ResolveTerminalContext(const TermControl& control) const
    {
        const auto contentId = control.ContentId();
        if (contentId == 0)
        {
            return std::nullopt;
        }

        for (uint32_t i = 0; i < _tabs.Size(); ++i)
        {
            const auto tabImpl = _GetTabImpl(_tabs.GetAt(i));
            if (!tabImpl)
            {
                continue;
            }

            if (const auto rootPane = tabImpl->GetRootPane())
            {
                const auto found = rootPane->WalkTree([&](const auto& pane) -> std::optional<TerminalRoutingContext> {
                    if (const auto paneControl = pane->GetTerminalControl())
                    {
                        if (paneControl.ContentId() == contentId)
                        {
                            TerminalRoutingContext context;
                            context.ContentId = contentId;
                            context.RoutingKey = fmt::format(L"tabptr-{}::paneptr-{}",
                                                             gsl::narrow_cast<uint64_t>(reinterpret_cast<uintptr_t>(tabImpl.get())),
                                                             gsl::narrow_cast<uint64_t>(reinterpret_cast<uintptr_t>(pane.get())));
                            context.TabKey = _ResolveWorkspaceArtifactTabKey(tabImpl);
                            context.TabId = fmt::format(L"tab-{}", i + 1);
                            context.PaneId = fmt::format(L"pane-{}", pane->Id().value_or(0));
                            return context;
                        }
                    }
                    return std::nullopt;
                });

                if (found)
                {
                    return found;
                }
            }
        }

        return std::nullopt;
    }

    std::wstring TerminalPage::_WorkspaceChatStateKey(const TerminalRoutingContext& context) const
    {
        return terminal::workspacechat::WorkspaceStateKey(context.RoutingKey, context.ContentId);
    }

    std::wstring TerminalPage::_WorkspaceChatStateKey(const TermControl& control) const
    {
        if (const auto context = _ResolveTerminalContext(control))
        {
            return _WorkspaceChatStateKey(*context);
        }

        return terminal::workspacechat::WorkspaceStateKey({}, control ? control.ContentId() : 0);
    }

    void TerminalPage::_OnTerminalKeySent(const IInspectable& sender, const KeySentEventArgs& args)
    {
        const auto control = sender.try_as<TermControl>();
        if (!control || !args.KeyDown())
        {
            return;
        }

        auto& state = _workspaceExtension->GetOrCreateWorkspaceChatTerminalState(_WorkspaceChatStateKey(control));
        if (terminal::workspacechat::HandleTerminalKeyDown(state.PendingInput, args.VKey()))
        {
            _FlushTerminalInputBuffer(control);
        }
    }

    void TerminalPage::_OnTerminalCharSent(const IInspectable& sender, const CharSentEventArgs& args)
    {
        const auto control = sender.try_as<TermControl>();
        if (!control)
        {
            return;
        }

        auto& state = _workspaceExtension->GetOrCreateWorkspaceChatTerminalState(_WorkspaceChatStateKey(control));
        if (terminal::workspacechat::HandleTerminalCharInput(state.PendingInput, args.Character()))
        {
            _FlushTerminalInputBuffer(control);
        }
    }

    void TerminalPage::_OnTerminalStringSent(const IInspectable& sender, const StringSentEventArgs& args)
    {
        const auto control = sender.try_as<TermControl>();
        if (!control)
        {
            return;
        }

        const auto text = std::wstring{ args.Text().c_str() };
        if (!text.empty())
        {
            if (text.size() > 1 || terminal::workspacechat::ContainsLineBreak(text))
            {
                Json::Value payload{ Json::objectValue };
                payload["controlInstanceId"] = Json::UInt64{ control.ContentId() };
                payload["bracketedPasteEnabled"] = control.BracketedPasteEnabled();
                terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceKey", _CurrentWorkspaceStorageKey());
                if (const auto context = _ResolveTerminalContext(control))
                {
                    terminal::workspacechat::AddOptionalDiagnosticString(payload, "terminalKey", context->RoutingKey);
                    terminal::workspacechat::AddOptionalDiagnosticString(payload, "tabId", context->TabId);
                    terminal::workspacechat::AddOptionalDiagnosticString(payload, "paneId", context->PaneId);
                }
                terminal::workspacechat::AddDiagnosticTextFields(payload, "stringSentText", text);
                _logWorkspaceChatDiagnostic(L"terminal_string_sent", payload);
            }
            _FlushTerminalInputBuffer(control, text);
        }
    }

    void TerminalPage::_FlushTerminalInputBuffer(const TermControl& control, std::wstring_view inputOverride)
    {
        if (const auto context = _ResolveTerminalContext(control))
        {
            auto& state = _workspaceExtension->GetOrCreateWorkspaceChatTerminalState(_WorkspaceChatStateKey(*context));
            const auto trackedWorkingDirectory = _ResolveTrackedTerminalWorkingDirectory(control);
            const auto captureResult = terminal::workspacechat::BuildTerminalInputCaptureResult(state,
                                                                                                inputOverride,
                                                                                                control.CommandHistory().CurrentCommandline().c_str(),
                                                                                                trackedWorkingDirectory,
                                                                                                control.ReadEntireBuffer().c_str());
            const auto currentCommandline = terminal::workspacechat::NormalizeTerminalInput(control.CommandHistory().CurrentCommandline().c_str());
            Json::Value payload{ Json::objectValue };
            payload["controlInstanceId"] = Json::UInt64{ context->ContentId };
            payload["pendingInputLengthBeforeClear"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(captureResult.PendingInput.PendingInputLength) };
            payload["usedInputOverride"] = !inputOverride.empty();
            payload["detectedLineCount"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(captureResult.FlushPlan.Lines.size()) };
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceKey", _CurrentWorkspaceStorageKey());
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "terminalKey", context->RoutingKey);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "tabId", context->TabId);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "paneId", context->PaneId);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "inputText", captureResult.PendingInput.InputText);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "normalizedInput", captureResult.FlushPlan.NormalizedInput);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "currentCommandline", currentCommandline);
            _logWorkspaceChatDiagnostic(L"terminal_input_flush", payload);

            if (captureResult.FlushPlan.Lines.empty())
            {
                return;
            }

            auto correlationId = _workspaceExtension->WorkspaceChatController().ConsumePendingCorrelationId();
            for (const auto& capturedEntry : captureResult.Entries)
            {
                _workspaceExtension->WorkspaceChatController().LogTerminalInput(_CurrentWorkspaceStorageKey(),
                                                                                context->TabKey,
                                                                                context->TabId,
                                                                                context->PaneId,
                                                                                capturedEntry.Text,
                                                                                capturedEntry.Snapshot.WorkingDirectory,
                                                                                capturedEntry.Snapshot.Command,
                                                                                correlationId);
            }
            if (captureResult.StateChanged)
            {
                _UpdateWorkspaceTabRow();
            }
            _ScheduleTerminalOutputCapture(control, *context, std::move(correlationId));
        }
    }

    std::wstring TerminalPage::_ResolveTrackedTerminalWorkingDirectory(const TermControl& control) const
    {
        Microsoft::Terminal::Settings::Model::implementation::WorkspaceTrackedDirectoryInput input;
        input.ReportedWorkingDirectory = _trimWorkspaceNodeValue(control.WorkingDirectory().c_str());
        if (const auto conn = control.Connection())
        {
            if (const auto pty = conn.try_as<ConptyConnection>())
            {
                if (const auto processWorkingDirectory = _currentDirectoryFromProcessHandle(reinterpret_cast<HANDLE>(pty.RootProcessHandle()));
                    !processWorkingDirectory.empty())
                {
                    input.ProcessWorkingDirectory = processWorkingDirectory;
                }
            }
        }

        if (const auto state = _workspaceExtension->FindWorkspaceNodeRuntimeState(control.ContentId()))
        {
            input.RuntimeStartingDirectory = _trimWorkspaceNodeValue(state->StartingDirectory);
            input.RuntimeOperatingSystem = state->OperatingSystem;
            input.RuntimeShellType = state->ShellType;
            input.IsSshTransport = state->IsSshTransport;
        }

        return Microsoft::Terminal::Settings::Model::implementation::ResolveTrackedWorkspaceDirectory(input);
    }

    bool TerminalPage::_ShouldUseTwoPhaseWorkspaceChatSubmit(const TermControl& control)
    {
        const auto stateKey = _WorkspaceChatStateKey(control);
        auto& state = _workspaceExtension->GetOrCreateWorkspaceChatTerminalState(stateKey);
        const auto runtimeState = _FindWorkspaceNodeRuntimeState(control);
        return terminal::workspacechat::UpdateTwoPhaseSubmitPreference(
            state.PreferTwoPhaseSubmit,
            control.CommandHistory().CurrentCommandline().c_str(),
            terminal::workspacechat::ResolveCapturedCommand(state),
            state.LastSubmittedInput,
            runtimeState ? std::wstring_view{ runtimeState->StartupAction } : std::wstring_view{},
            runtimeState ? std::wstring_view{ runtimeState->ExplicitCommandline } : std::wstring_view{});
    }

    void TerminalPage::_ScheduleTerminalOutputCapture(const TermControl& control,
                                                      const TerminalRoutingContext& context,
                                                      std::wstring correlationId)
    {
        PendingTerminalOutputCapture pending;
        pending.Control = control;
        pending.ContentId = context.ContentId;
        pending.StateKey = _WorkspaceChatStateKey(context);
        pending.WorkspaceKey = _CurrentWorkspaceStorageKey();
        pending.TabKey = context.TabKey;
        pending.TabId = context.TabId;
        pending.PaneId = context.PaneId;
        pending.CorrelationId = std::move(correlationId);
        pending.DueTick = GetTickCount64() + 350;
        _workspaceExtension->UpsertWorkspaceChatPendingOutputCapture(std::move(pending));

        if (_workspaceChatOutputCaptureTimer && !_workspaceChatOutputCaptureTimer.IsEnabled())
        {
            _workspaceChatOutputCaptureTimer.Start();
        }
    }

    void TerminalPage::_ProcessPendingTerminalOutputCaptures()
    {
        const auto now = GetTickCount64();
        auto pendingCaptures = _workspaceExtension->TakeReadyWorkspaceChatPendingOutputCaptures(now);
        for (auto& pendingCapture : pendingCaptures)
        {
            const auto control = pendingCapture.Control.get();
            if (!control)
            {
                continue;
            }

            auto& state = _workspaceExtension->GetOrCreateWorkspaceChatTerminalState(pendingCapture.StateKey);
            const auto captureResult = terminal::workspacechat::BuildTerminalOutputCaptureResult(state.InputState,
                                                                                                 state.LastReportedWorkingDirectory,
                                                                                                 state.LastBufferSnapshot,
                                                                                                 state.HasBufferSnapshot,
                                                                                                 _ResolveTrackedTerminalWorkingDirectory(control),
                                                                                                 control.ReadEntireBuffer().c_str());
            if (captureResult.WorkingDirectoryChanged)
            {
                _UpdateWorkspaceTabRow();
            }

            if (!captureResult.OutputSummary.empty())
            {
                _workspaceExtension->WorkspaceChatController().LogTerminalOutput(pendingCapture.WorkspaceKey,
                                                                                 pendingCapture.TabKey,
                                                                                 pendingCapture.TabId,
                                                                                 pendingCapture.PaneId,
                                                                                 captureResult.OutputSummary,
                                                                                 terminal::workspacechat::ResolveCapturedWorkingDirectory(state),
                                                                                 terminal::workspacechat::ResolveCapturedCommand(state),
                                                                                 pendingCapture.CorrelationId);
            }
        }

        if (!_workspaceExtension->HasWorkspaceChatPendingOutputCaptures() && _workspaceChatOutputCaptureTimer)
        {
            _workspaceChatOutputCaptureTimer.Stop();
        }
    }
