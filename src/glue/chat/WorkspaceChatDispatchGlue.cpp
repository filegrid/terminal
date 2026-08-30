    void TerminalPage::_DispatchWorkspaceChatInput(const TermControl& control, const std::wstring_view text)
    {
        const auto bracketedPasteEnabled = control.BracketedPasteEnabled();
        const auto dispatchPlan = ::terminal::workspacechat::BuildWorkspaceChatDispatchPlan(text,
                                                                                            _workspaceExtension->WorkspaceChatSubmitTransport() == WorkspaceChatSubmitTransport::WindowKeyboardInput,
                                                                                            bracketedPasteEnabled,
                                                                                            _ShouldUseTwoPhaseWorkspaceChatSubmit(control));

        Json::Value logPayload{ Json::objectValue };
        logPayload["controlInstanceId"] = Json::UInt64{ control.ContentId() };
        logPayload["bracketedPasteEnabled"] = bracketedPasteEnabled;
        logPayload["useSpecialSubmitPath"] = dispatchPlan.TwoPhaseSend;
        terminal::workspacechat::AddOptionalDiagnosticString(logPayload, "workspaceKey", _CurrentWorkspaceStorageKey());
        if (const auto context = _ResolveTerminalContext(control))
        {
            terminal::workspacechat::AddOptionalDiagnosticString(logPayload, "terminalKey", context->RoutingKey);
            terminal::workspacechat::AddOptionalDiagnosticString(logPayload, "tabId", context->TabId);
            terminal::workspacechat::AddOptionalDiagnosticString(logPayload, "paneId", context->PaneId);
        }
        terminal::workspacechat::AddDiagnosticTextFields(logPayload, "dispatchText", text);
        terminal::workspacechat::AddDiagnosticTextFields(logPayload, "pastePayload", dispatchPlan.PastePayload);
        terminal::workspacechat::AddDiagnosticTextFields(logPayload, "submitPayload", dispatchPlan.SubmitPayload);
        terminal::workspacechat::AddDiagnosticTextFields(logPayload, "combinedPayload", dispatchPlan.CombinedPayload);
        logPayload["twoPhaseSend"] = dispatchPlan.TwoPhaseSend;
        switch (dispatchPlan.Mode)
        {
        case terminal::workspacechat::WorkspaceChatDispatchMode::WindowKeyboardInput:
            if (bracketedPasteEnabled || dispatchPlan.TextPayload != text)
            {
                logPayload["dispatchMode"] = "window-keyboard-br-normalized-enter";
                _logWorkspaceChatDiagnostic(L"chat_dispatch_send_input", logPayload);
                _DispatchWorkspaceChatWindowKeyboardInput(control, dispatchPlan.TextPayload, true);
            }
            else
            {
                logPayload["dispatchMode"] = "window-keyboard-enter";
                _logWorkspaceChatDiagnostic(L"chat_dispatch_send_input", logPayload);
                _DispatchWorkspaceChatWindowKeyboardInput(control, dispatchPlan.TextPayload, true);
            }
            break;
        case terminal::workspacechat::WorkspaceChatDispatchMode::PasteThenSendInput:
            logPayload["dispatchMode"] = bracketedPasteEnabled ? "paste-leading-lf-trailing-lf-sendinput-cr" : "paste-lf-sendinput-cr";
            _logWorkspaceChatDiagnostic(L"chat_dispatch_send_input", logPayload);
            control.PasteText(winrt::hstring{ dispatchPlan.PastePayload });
            control.SendInput(winrt::hstring{ dispatchPlan.SubmitPayload });
            break;
        case terminal::workspacechat::WorkspaceChatDispatchMode::InlineSendInput:
            logPayload["dispatchMode"] = "sendinput-inline-cr";
            _logWorkspaceChatDiagnostic(L"chat_dispatch_send_input", logPayload);
            control.SendInput(winrt::hstring{ dispatchPlan.SubmitPayload });
            break;
        }
    }

    safe_void_coroutine TerminalPage::_DispatchWorkspaceChatSubmitKey(TermControl control, const bool restoreInputFocus)
    {
        if (!control)
        {
            co_return;
        }

        _FocusCurrentTab(true);
        control.Focus(FocusState::Programmatic);

        co_await winrt::resume_after(WorkspaceChatSubmitKeyDelay);
        co_await wil::resume_foreground(Dispatcher());

        Json::Value submitPayloadLog{ Json::objectValue };
        terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "workspaceKey", _CurrentWorkspaceStorageKey());
        submitPayloadLog["controlInstanceId"] = Json::UInt64{ control.ContentId() };
        submitPayloadLog["repeatIndex"] = Json::UInt64{ 0 };
        if (const auto context = _ResolveTerminalContext(control))
        {
            terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "terminalKey", context->RoutingKey);
            terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "tabId", context->TabId);
            terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "paneId", context->PaneId);
        }
        if (_hostingHwnd)
        {
            submitPayloadLog["hostingHwndClass"] = terminal::workspacechat::DiagnosticUtf8(terminal::workspacechat::WindowClassName(*_hostingHwnd));
        }
        if (const auto foreground = GetForegroundWindow())
        {
            submitPayloadLog["foregroundHwndClassBeforeSend"] = terminal::workspacechat::DiagnosticUtf8(terminal::workspacechat::WindowClassName(foreground));
        }

        const auto sent = _sendKeyboardEnterToFocusedWindow();
        submitPayloadLog["submitPayload"] = "<window-keyboard-enter>";
        submitPayloadLog["submitPath"] = "window-keyboard-sendinput-enter";
        submitPayloadLog["submitSucceeded"] = sent;
        if (const auto foreground = GetForegroundWindow())
        {
            submitPayloadLog["foregroundHwndClassAfterSend"] = terminal::workspacechat::DiagnosticUtf8(terminal::workspacechat::WindowClassName(foreground));
        }
        _logWorkspaceChatDiagnostic(L"termcontrol_submit_char_result", submitPayloadLog);

        if (restoreInputFocus)
        {
            co_await winrt::resume_after(50ms);
            co_await wil::resume_foreground(Dispatcher());
            if (const auto input = WorkspaceChatInput())
            {
                input.Focus(FocusState::Programmatic);
            }
        }
    }

    safe_void_coroutine TerminalPage::_DispatchWorkspaceChatWindowKeyboardInput(TermControl control, std::wstring text, const bool restoreInputFocus)
    {
        if (!control)
        {
            co_return;
        }

        auto& chatUiState = _workspaceExtension->WorkspaceChatUiState();
        if (chatUiState.SubmitInProgress)
        {
            co_return;
        }
        chatUiState.SubmitInProgress = true;
        auto submitGuard = wil::scope_exit([&]() noexcept {
            chatUiState.SubmitInProgress = false;
        });

        _FocusCurrentTab(true);
        const auto controlOwningHwnd = reinterpret_cast<HWND>(control.OwningHwnd());
        auto activationHwnd = controlOwningHwnd;
        if (!activationHwnd && _hostingHwnd)
        {
            activationHwnd = *_hostingHwnd;
        }
        const auto activationAttempted = activationHwnd != nullptr;
        const auto activationSucceeded = terminal::workspacechat::ActivateWindowForKeyboardInput(activationHwnd);
        const auto controlFocusRequested = control.Focus(FocusState::Programmatic);
        bool tryFocusAsyncRequested = false;
        bool tryFocusAsyncSucceeded = false;
        if (const auto controlAsDependency = control.try_as<WUX::DependencyObject>())
        {
            tryFocusAsyncRequested = true;
            if (const auto focusResult = co_await WUX::Input::FocusManager::TryFocusAsync(controlAsDependency, FocusState::Programmatic))
            {
                tryFocusAsyncSucceeded = focusResult.Succeeded();
            }
        }

        co_await winrt::resume_after(WorkspaceChatSubmitKeyDelay);
        co_await wil::resume_foreground(Dispatcher());

        Json::Value submitPayloadLog{ Json::objectValue };
        terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "workspaceKey", _CurrentWorkspaceStorageKey());
        submitPayloadLog["controlInstanceId"] = Json::UInt64{ control.ContentId() };
        terminal::workspacechat::AddDiagnosticTextFields(submitPayloadLog, "submitPayload", text);
        if (const auto context = _ResolveTerminalContext(control))
        {
            terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "terminalKey", context->RoutingKey);
            terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "tabId", context->TabId);
            terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "paneId", context->PaneId);
        }
        if (_hostingHwnd)
        {
            submitPayloadLog["hostingHwndClass"] = terminal::workspacechat::DiagnosticUtf8(terminal::workspacechat::WindowClassName(*_hostingHwnd));
        }
        if (const auto foreground = GetForegroundWindow())
        {
            submitPayloadLog["foregroundHwndClassBeforeSend"] = terminal::workspacechat::DiagnosticUtf8(terminal::workspacechat::WindowClassName(foreground));
        }
        submitPayloadLog["windowActivationAttempted"] = activationAttempted;
        submitPayloadLog["windowActivationSucceeded"] = activationSucceeded;
        submitPayloadLog["controlFocusRequested"] = controlFocusRequested;
        submitPayloadLog["tryFocusAsyncRequested"] = tryFocusAsyncRequested;
        submitPayloadLog["tryFocusAsyncSucceeded"] = tryFocusAsyncSucceeded;
        terminal::workspacechat::AppendHwndDiagnostic(submitPayloadLog, "windowActivationTarget", activationHwnd);
        terminal::workspacechat::AppendHwndDiagnostic(submitPayloadLog, "controlOwning", controlOwningHwnd);
        terminal::workspacechat::AppendGuiThreadFocusDiagnostics(submitPayloadLog, "win32BeforeSend");
        if (const auto root = this->XamlRoot())
        {
            submitPayloadLog["focusedElementBeforeSend"] = terminal::workspacechat::DiagnosticUtf8(_focusedElementDescriptor(root));
            submitPayloadLog["focusedIsWorkspaceChatInputBeforeSend"] = _isFocusedElementWithin(root, WorkspaceChatInput());
            submitPayloadLog["focusedIsTermControlBeforeSend"] = _isFocusedElementWithin(root, control.try_as<WUX::DependencyObject>());
        }

        if (const auto root = this->XamlRoot())
        {
            const auto controlDependency = control.try_as<WUX::DependencyObject>();
            auto waited = 0ms;
            while (_isFocusedElementWithin(root, WorkspaceChatInput()) &&
                   waited < 300ms)
            {
                co_await winrt::resume_after(30ms);
                co_await wil::resume_foreground(Dispatcher());
                if (controlDependency)
                {
                    std::ignore = WUX::Input::FocusManager::TryFocusAsync(controlDependency, FocusState::Programmatic);
                }
                waited += 30ms;
            }

            submitPayloadLog["focusedElementReadyToSend"] = terminal::workspacechat::DiagnosticUtf8(_focusedElementDescriptor(root));
            submitPayloadLog["focusedIsWorkspaceChatInputReadyToSend"] = _isFocusedElementWithin(root, WorkspaceChatInput());
            submitPayloadLog["focusedIsTermControlReadyToSend"] = _isFocusedElementWithin(root, controlDependency);
            terminal::workspacechat::AppendGuiThreadFocusDiagnostics(submitPayloadLog, "win32ReadyToSend");

            if (_isFocusedElementWithin(root, WorkspaceChatInput()))
            {
                submitPayloadLog["submitPath"] = "window-keyboard-abort-chat-input-still-focused";
                submitPayloadLog["textSendSucceeded"] = false;
                submitPayloadLog["submitSucceeded"] = false;
                _logWorkspaceChatDiagnostic(L"termcontrol_submit_char_result", submitPayloadLog);
                co_return;
            }
        }

        terminal::workspacechat::AddOptionalDiagnosticString(submitPayloadLog, "releasedModifiersBeforeSend", _releasePressedKeyboardModifiers());
        wchar_t unmappedPhysicalCharacter{};
        const auto textSent = _sendKeyboardTextToFocusedWindow(text, unmappedPhysicalCharacter);
        submitPayloadLog["textSendSucceeded"] = textSent;
        submitPayloadLog["keyboardTextInjectionMode"] = "physical-layout-scancode";
        if (unmappedPhysicalCharacter != L'\0')
        {
            terminal::workspacechat::AddDiagnosticTextFields(submitPayloadLog,
                                                             "unmappedPhysicalKeyCharacter",
                                                             std::wstring_view{ &unmappedPhysicalCharacter, 1 });
        }
        submitPayloadLog["keyboardSubmitInterKeyDelayMs"] = Json::UInt64{ 80 };

        auto enterSent = false;
        if (textSent)
        {
            co_await winrt::resume_after(WorkspaceChatKeyboardSubmitInterKeyDelay);
            co_await wil::resume_foreground(Dispatcher());
            enterSent = _sendKeyboardEnterToFocusedWindow();
        }

        submitPayloadLog["submitPath"] = "window-keyboard-sendinput-text-enter";
        submitPayloadLog["submitSucceeded"] = enterSent;
        if (const auto foreground = GetForegroundWindow())
        {
            submitPayloadLog["foregroundHwndClassAfterSend"] = terminal::workspacechat::DiagnosticUtf8(terminal::workspacechat::WindowClassName(foreground));
        }
        terminal::workspacechat::AppendGuiThreadFocusDiagnostics(submitPayloadLog, "win32AfterSend");
        if (const auto root = this->XamlRoot())
        {
            submitPayloadLog["focusedElementAfterSend"] = terminal::workspacechat::DiagnosticUtf8(_focusedElementDescriptor(root));
            submitPayloadLog["focusedIsWorkspaceChatInputAfterSend"] = _isFocusedElementWithin(root, WorkspaceChatInput());
            submitPayloadLog["focusedIsTermControlAfterSend"] = _isFocusedElementWithin(root, control.try_as<WUX::DependencyObject>());
        }
        _logWorkspaceChatDiagnostic(L"termcontrol_submit_char_result", submitPayloadLog);

        if (restoreInputFocus)
        {
            co_await winrt::resume_after(250ms);
            co_await wil::resume_foreground(Dispatcher());
            if (const auto input = WorkspaceChatInput())
            {
                input.Focus(FocusState::Programmatic);
            }
        }
    }

    void TerminalPage::_PersistWorkspaceChatDraft()
    {
        _workspaceExtension->WorkspaceChatController().SaveDraft(_CurrentWorkspaceStorageKey(),
                                                                 _CurrentWorkspaceArtifactTabKey(),
                                                                 WorkspaceChatInput().Text().c_str());
    }

    void TerminalPage::_UpdateWorkspaceChatInputHeight()
    {
        auto& chatUiState = _workspaceExtension->WorkspaceChatUiState();
        if (!_tabContent || !chatUiState.EnabledForActiveTab)
        {
            return;
        }

        const auto input = WorkspaceChatInput();
        if (!input)
        {
            return;
        }

        const auto padding = input.Padding();
        const auto fallbackLineHeight = std::max(16.0, std::ceil(input.FontSize() * 1.4));
        auto lineHeight = fallbackLineHeight;
        auto contentHeight = fallbackLineHeight;

        const auto text = input.Text();
        if (input.ActualWidth() > 0.0 && !text.empty())
        {
            const auto trailingLineBreaks = terminal::workspacechat::CountTrailingLineBreaks(text);
            if (const auto lastVisibleCharacterIndex = _lastNonLineBreakCharacterIndex(text))
            {
                const auto lastCharacterRect = input.GetRectFromCharacterIndex(gsl::narrow_cast<int32_t>(*lastVisibleCharacterIndex), true);
                if (lastCharacterRect.Height > 0.0)
                {
                    lineHeight = lastCharacterRect.Height;
                }
                contentHeight = std::max(lineHeight, static_cast<double>(lastCharacterRect.Y + lastCharacterRect.Height + (trailingLineBreaks * lineHeight)));
            }
            else
            {
                contentHeight = std::max(lineHeight, static_cast<double>((trailingLineBreaks + 1) * lineHeight));
            }
        }

        const auto minHeight = lineHeight + padding.Top + padding.Bottom;
        const auto maxHeight = std::max(minHeight, _tabContent.ActualHeight() - 120.0);
        const auto desiredHeight = std::clamp(contentHeight + padding.Top + padding.Bottom, minHeight, maxHeight);

        if (std::abs(chatUiState.ExpandedHeight - desiredHeight) < 0.5)
        {
            return;
        }

        chatUiState.ExpandedHeight = desiredHeight;
        input.Height(desiredHeight);
        WorkspaceChatPanelRow().Height(GridLengthHelper::FromValueAndType(desiredHeight, GridUnitType::Pixel));
        _UpdateTerminalContentHostClip();
    }

    void TerminalPage::_SendWorkspaceChatMessage()
    {
        auto& chatUiState = _workspaceExtension->WorkspaceChatUiState();
        if (chatUiState.SubmitInProgress)
        {
            return;
        }

        const auto rawText = std::wstring{ WorkspaceChatInput().Text().c_str() };
        const auto trimmed = terminal::workspacechat::TrimWorkspaceChatText(rawText);
        if (trimmed.empty())
        {
            return;
        }

        const auto dispatchText = terminal::workspacechat::TrimTrailingLineBreaksOnly(rawText);

        const auto control = _GetActiveControl();
        if (!control)
        {
            Json::Value payload{ Json::objectValue };
            payload["windowId"] = Json::UInt64{ _WindowProperties.WindowId() };
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceKey", _CurrentWorkspaceStorageKey());
            terminal::workspacechat::AddDiagnosticTextFields(payload, "rawText", rawText);
            _logWorkspaceChatDiagnostic(L"chat_submit_no_active_control", payload);
            return;
        }

        std::wstring tabId{ L"tab-0" };
        std::wstring paneId{ L"pane-0" };
        if (const auto context = _ResolveTerminalContext(control))
        {
            tabId = context->TabId;
            paneId = context->PaneId;
        }

        Json::Value payload{ Json::objectValue };
        payload["windowId"] = Json::UInt64{ _WindowProperties.WindowId() };
        payload["controlInstanceId"] = Json::UInt64{ control.ContentId() };
        payload["bracketedPasteEnabled"] = control.BracketedPasteEnabled();
        terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceKey", _CurrentWorkspaceStorageKey());
        terminal::workspacechat::AddOptionalDiagnosticString(payload, "currentWorkspaceId", _currentWorkspaceId.c_str());
        if (const auto context = _ResolveTerminalContext(control))
        {
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "terminalKey", context->RoutingKey);
        }
        terminal::workspacechat::AddOptionalDiagnosticString(payload, "tabId", tabId);
        terminal::workspacechat::AddOptionalDiagnosticString(payload, "paneId", paneId);
        terminal::workspacechat::AddDiagnosticTextFields(payload, "rawText", rawText);
        terminal::workspacechat::AddDiagnosticTextFields(payload, "commandlineBeforeSend", control.CommandHistory().CurrentCommandline().c_str());
        _logWorkspaceChatDiagnostic(L"chat_submit_requested", payload);

        _workspaceExtension->WorkspaceChatController().SubmitUserMessage(_CurrentWorkspaceStorageKey(),
                                                                         _CurrentWorkspaceArtifactTabKey(),
                                                                         _WindowProperties.WindowId(),
                                                                         tabId,
                                                                         paneId,
                                                                         dispatchText);

        _DispatchWorkspaceChatInput(control, dispatchText);
        chatUiState.DraftUpdateInProgress = true;
        WorkspaceChatInput().Text(L"");
        chatUiState.DraftUpdateInProgress = false;
        _PersistWorkspaceChatDraft();
        _UpdateWorkspaceChatInputHeight();
        WorkspaceChatInput().Focus(FocusState::Programmatic);
    }

    void TerminalPage::_SetWorkspaceChatCollapsed(const bool /*collapsed*/)
    {
        if (!_tabContent)
        {
            return;
        }

        auto& chatUiState = _workspaceExtension->WorkspaceChatUiState();
        chatUiState.Collapsed = false;

        if (!chatUiState.EnabledForActiveTab)
        {
            WorkspaceChatPanel().Visibility(Visibility::Collapsed);
            WorkspaceChatHeader().Visibility(Visibility::Collapsed);
            WorkspaceChatBody().Visibility(Visibility::Collapsed);
            WorkspaceChatResizeHandle().Visibility(Visibility::Collapsed);
            WorkspaceChatPanelRow().Height(GridLengthHelper::FromValueAndType(0, GridUnitType::Pixel));
            _UpdateTerminalContentHostClip();
            _UpdateWorkspaceChatHeader();
            return;
        }

        WorkspaceChatPanel().Visibility(Visibility::Visible);
        WorkspaceChatHeader().Visibility(Visibility::Collapsed);
        WorkspaceChatBody().Visibility(Visibility::Visible);
        WorkspaceChatResizeHandle().Visibility(Visibility::Collapsed);
        WorkspaceChatPanelRow().Height(GridLengthHelper::FromValueAndType(chatUiState.ExpandedHeight, GridUnitType::Pixel));
        _UpdateWorkspaceChatInputHeight();
        _UpdateTerminalContentHostClip();
        _UpdateWorkspaceChatHeader();
    }

    void TerminalPage::_ToggleWorkspaceChatCollapsed()
    {
        _SetWorkspaceChatCollapsed(!_workspaceExtension->WorkspaceChatUiState().Collapsed);
    }
