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

        auto& state = _workspaceExtension->WorkspaceChatTerminalStates()[_WorkspaceChatStateKey(control)];
        switch (args.VKey())
        {
        case VK_BACK:
            if (!state.PendingInput.empty())
            {
                state.PendingInput.pop_back();
            }
            break;
        case VK_RETURN:
            _FlushTerminalInputBuffer(control);
            break;
        default:
            break;
        }
    }

    void TerminalPage::_OnTerminalCharSent(const IInspectable& sender, const CharSentEventArgs& args)
    {
        const auto control = sender.try_as<TermControl>();
        if (!control)
        {
            return;
        }

        auto& state = _workspaceExtension->WorkspaceChatTerminalStates()[_WorkspaceChatStateKey(control)];
        const auto character = args.Character();
        if (character == L'\r' || character == L'\n')
        {
            _FlushTerminalInputBuffer(control);
        }
        else if (character >= L' ' || character == L'\t')
        {
            state.PendingInput.push_back(character);
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
            auto& state = _workspaceExtension->WorkspaceChatTerminalStates()[_WorkspaceChatStateKey(*context)];
            const auto pendingInputLength = state.PendingInput.size();
            const auto inputText = inputOverride.empty() ? state.PendingInput : std::wstring{ inputOverride };
            state.PendingInput.clear();

            auto normalizedInput = terminal::workspacechat::NormalizeTerminalInput(inputText);
            const auto currentCommandline = terminal::workspacechat::NormalizeTerminalInput(control.CommandHistory().CurrentCommandline().c_str());
            if (normalizedInput.empty())
            {
                if (!currentCommandline.empty())
                {
                    normalizedInput = currentCommandline;
                }
            }
            else if (!currentCommandline.empty() &&
                     terminal::workspacechat::ContainsLineBreak(currentCommandline) &&
                     currentCommandline.size() > normalizedInput.size() &&
                     currentCommandline.ends_with(normalizedInput))
            {
                normalizedInput = currentCommandline;
            }

            const auto lines = terminal::workspacechat::SplitTerminalInputLines(normalizedInput);
            Json::Value payload{ Json::objectValue };
            payload["controlInstanceId"] = Json::UInt64{ context->ContentId };
            payload["pendingInputLengthBeforeClear"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(pendingInputLength) };
            payload["usedInputOverride"] = !inputOverride.empty();
            payload["detectedLineCount"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(lines.size()) };
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceKey", _CurrentWorkspaceStorageKey());
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "terminalKey", context->RoutingKey);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "tabId", context->TabId);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "paneId", context->PaneId);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "inputText", inputText);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "normalizedInput", normalizedInput);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "currentCommandline", currentCommandline);
            _logWorkspaceChatDiagnostic(L"terminal_input_flush", payload);

            if (lines.empty())
            {
                return;
            }

            state.LastSubmittedInput = normalizedInput;

            if (!state.HasBufferSnapshot)
            {
                state.LastBufferSnapshot = control.ReadEntireBuffer().c_str();
                state.HasBufferSnapshot = true;
            }

            auto correlationId = _workspaceExtension->WorkspaceChatController().ConsumePendingCorrelationId();
            auto workspaceStateChanged = _SyncTerminalCapturedWorkingDirectory(state, _ResolveTrackedTerminalWorkingDirectory(control));
            for (const auto& line : lines)
            {
                const auto snapshot = terminal::workspacechat::TrackTerminalInput(state.InputState, line);
                workspaceStateChanged = true;
                _workspaceExtension->WorkspaceChatController().LogTerminalInput(_CurrentWorkspaceStorageKey(),
                                                                                context->TabKey,
                                                                                context->TabId,
                                                                                context->PaneId,
                                                                                line,
                                                                                snapshot.WorkingDirectory,
                                                                                snapshot.Command,
                                                                                correlationId);
            }
            if (workspaceStateChanged)
            {
                _UpdateWorkspaceTabRow();
            }
            _ScheduleTerminalOutputCapture(control, *context, std::move(correlationId));
        }
    }

    bool TerminalPage::_SyncTerminalCapturedWorkingDirectory(TerminalCaptureState& state, std::wstring_view workingDirectory) const
    {
        return terminal::workspacechat::SyncCapturedWorkingDirectory(state.InputState,
                                                                     state.LastReportedWorkingDirectory,
                                                                     workingDirectory);
    }

    std::wstring TerminalPage::_ResolveTrackedTerminalWorkingDirectory(const TermControl& control) const
    {
        const auto reportedWorkingDirectory = _trimWorkspaceNodeValue(control.WorkingDirectory().c_str());
        if (const auto state = _workspaceExtension->FindWorkspaceNodeRuntimeState(control.ContentId()))
        {
            const auto shellType = _toLower(state->ShellType);
            const auto operatingSystem = _toLower(state->OperatingSystem);
            const auto preferTerminalReportedDirectory = state->IsSshTransport || shellType == L"ssh" || operatingSystem == L"linux";
            if (preferTerminalReportedDirectory)
            {
                return reportedWorkingDirectory;
            }
        }

        if (const auto conn = control.Connection())
        {
            if (const auto pty = conn.try_as<ConptyConnection>())
            {
                if (const auto processWorkingDirectory = _currentDirectoryFromProcessHandle(reinterpret_cast<HANDLE>(pty.RootProcessHandle()));
                    !processWorkingDirectory.empty())
                {
                    return processWorkingDirectory;
                }
            }
        }

        if (!reportedWorkingDirectory.empty())
        {
            return reportedWorkingDirectory;
        }

        if (const auto state = _workspaceExtension->FindWorkspaceNodeRuntimeState(control.ContentId());
            state &&
            !state->StartingDirectory.empty())
        {
            return _trimWorkspaceNodeValue(state->StartingDirectory);
        }

        return {};
    }

    bool TerminalPage::_ShouldUseTwoPhaseWorkspaceChatSubmit(const TermControl& control)
    {
        const auto stateKey = _WorkspaceChatStateKey(control);
        auto& terminalStates = _workspaceExtension->WorkspaceChatTerminalStates();
        const auto stateIt = terminalStates.find(stateKey);
        const auto runtimeState = _FindWorkspaceNodeRuntimeState(control);
        const auto shouldPrefer = terminal::workspacechat::ShouldPreferTwoPhaseSubmit(
            control.CommandHistory().CurrentCommandline().c_str(),
            stateIt != terminalStates.end() && stateIt->second.PreferTwoPhaseSubmit,
            stateIt != terminalStates.end() ? stateIt->second.InputState.LastCommand : std::wstring_view{},
            stateIt != terminalStates.end() ? stateIt->second.LastSubmittedInput : std::wstring_view{},
            runtimeState ? std::wstring_view{ runtimeState->StartupAction } : std::wstring_view{},
            runtimeState ? std::wstring_view{ runtimeState->ExplicitCommandline } : std::wstring_view{});
        if (shouldPrefer)
        {
            terminalStates[stateKey].PreferTwoPhaseSubmit = true;
        }
        return shouldPrefer;
    }

    void TerminalPage::_ScheduleTerminalOutputCapture(const TermControl& control,
                                                      const TerminalRoutingContext& context,
                                                      std::wstring correlationId)
    {
        auto& pendingCaptures = _workspaceExtension->WorkspaceChatPendingOutputCaptures();
        pendingCaptures.erase(std::remove_if(pendingCaptures.begin(),
                                             pendingCaptures.end(),
                                                                 [&](const auto& pending) {
                                                                     const auto existingControl = pending.Control.get();
                                                                     return existingControl && existingControl.ContentId() == context.ContentId;
                                                                 }),
                              pendingCaptures.end());

        PendingTerminalOutputCapture pending;
        pending.Control = control;
        pending.StateKey = _WorkspaceChatStateKey(context);
        pending.WorkspaceKey = _CurrentWorkspaceStorageKey();
        pending.TabKey = context.TabKey;
        pending.TabId = context.TabId;
        pending.PaneId = context.PaneId;
        pending.CorrelationId = std::move(correlationId);
        pending.DueTick = GetTickCount64() + 350;
        pendingCaptures.emplace_back(std::move(pending));

        if (_workspaceChatOutputCaptureTimer && !_workspaceChatOutputCaptureTimer.IsEnabled())
        {
            _workspaceChatOutputCaptureTimer.Start();
        }
    }

    void TerminalPage::_ProcessPendingTerminalOutputCaptures()
    {
        const auto now = GetTickCount64();
        auto& pendingCaptures = _workspaceExtension->WorkspaceChatPendingOutputCaptures();
        auto& terminalStates = _workspaceExtension->WorkspaceChatTerminalStates();
        auto next = pendingCaptures.begin();
        for (auto it = pendingCaptures.begin(); it != pendingCaptures.end(); ++it)
        {
            if (it->DueTick > now)
            {
                *next++ = std::move(*it);
                continue;
            }

            const auto control = it->Control.get();
            if (!control)
            {
                continue;
            }

            auto& state = terminalStates[it->StateKey];
            if (_SyncTerminalCapturedWorkingDirectory(state, _ResolveTrackedTerminalWorkingDirectory(control)))
            {
                _UpdateWorkspaceTabRow();
            }
            const auto currentBuffer = std::wstring{ control.ReadEntireBuffer().c_str() };
            const auto outputSummary = terminal::workspacechat::SummarizeTerminalOutput(currentBuffer, state.LastBufferSnapshot);
            state.LastBufferSnapshot = currentBuffer;
            state.HasBufferSnapshot = true;

            if (!outputSummary.empty())
            {
                _workspaceExtension->WorkspaceChatController().LogTerminalOutput(it->WorkspaceKey,
                                                                                 it->TabKey,
                                                                                 it->TabId,
                                                                                 it->PaneId,
                                                                                 outputSummary,
                                                                                 state.InputState.LastWorkingDirectory,
                                                                                 state.InputState.LastCommand,
                                                                                 it->CorrelationId);
            }
        }

        pendingCaptures.erase(next, pendingCaptures.end());
        if (pendingCaptures.empty() && _workspaceChatOutputCaptureTimer)
        {
            _workspaceChatOutputCaptureTimer.Stop();
        }
    }
