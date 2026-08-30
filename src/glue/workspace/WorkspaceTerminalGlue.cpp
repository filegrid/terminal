    WUX::Controls::MenuFlyout TerminalPage::_CreateWorkspaceFlyout()
    {
        auto workspaceFlyout = WUX::Controls::MenuFlyout{};
        // Keep the same visual rhythm and opening direction as the native
        // new-Tab menu at the right side of the tab strip.
        workspaceFlyout.Placement(WUX::Controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        workspaceFlyout.Opening([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto page{ weakThis.get() })
            {
                page->_FocusCurrentTab(true);
            }
        });
        workspaceFlyout.Closing([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto page{ weakThis.get() })
            {
                if (!page->_commandPaletteIs(Visibility::Visible))
                {
                    page->_FocusCurrentTab(true);
                }
            }
        });
        workspaceFlyout.Closed([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto page{ weakThis.get() })
            {
                page->_workspaceFlyout = nullptr;
            }
        });

        const auto loadedFlyoutState = ::terminal::workspace::LoadWorkspaceFlyoutState(_currentWorkspaceId.c_str());
        const auto& flyoutState = loadedFlyoutState.FlyoutState;
        for (const auto& entry : flyoutState.Entries)
        {
            auto workspaceItem = WUX::Controls::ToggleMenuFlyoutItem{};
            const auto& workspace = entry.Definition;
            workspaceItem.Text(winrt::hstring{ workspace.Name });
            if (!workspace.Icon.empty())
            {
                if (const auto icon = _CreateNewTabFlyoutIcon(winrt::hstring{ workspace.Icon }))
                {
                    if (const auto frameworkElement = icon.try_as<FrameworkElement>())
                    {
                        frameworkElement.Width(16);
                        frameworkElement.Height(16);
                        frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                        frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                    }
                    workspaceItem.Icon(icon);
                }
            }
            if (const auto color = _parseWorkspaceColor(workspace.BackgroundColor))
            {
                workspaceItem.Foreground(SolidColorBrush{ *color });
            }
            workspaceItem.IsChecked(entry.IsOpen);
            workspaceItem.Click([workspaceId{ winrt::hstring{ workspace.Id } }, weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto page{ weakThis.get() })
                {
                    page->_OpenWorkspace(workspaceId, true);
                }
            });
            workspaceFlyout.Items().Append(workspaceItem);
        }

        if (!_currentWorkspaceId.empty() && !flyoutState.CurrentWorkspaceExists)
        {
            auto currentWorkspaceItem = WUX::Controls::MenuFlyoutItem{};
            currentWorkspaceItem.Text(winrt::hstring{ _CurrentWorkspaceDisplayName() });
            currentWorkspaceItem.IsEnabled(false);
            workspaceFlyout.Items().Append(currentWorkspaceItem);
        }
        else if (flyoutState.Entries.empty() && !_currentWorkspaceId.empty())
        {
            auto placeholder = WUX::Controls::MenuFlyoutItem{};
            placeholder.Text(RS_(L"WorkspaceNoneSaved"));
            placeholder.IsEnabled(false);
            workspaceFlyout.Items().Append(placeholder);
        }

        return workspaceFlyout;
    }

    safe_void_coroutine TerminalPage::_OpenWorkspace(const winrt::hstring& workspaceId, const bool openInNewWindow)
    {
        const auto strong = get_strong();
        const auto appState = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();

        std::vector<winrt::TerminalApp::Tab> tabsToReplace;
        tabsToReplace.reserve(_tabs.Size());
        for (const auto& tab : _tabs)
        {
            tabsToReplace.emplace_back(tab);
        }

        const auto loadedOpenState = ::terminal::workspace::LoadWorkspaceOpenExecutionState(workspaceId.c_str(),
                                                                                            openInNewWindow,
                                                                                            _currentWorkspaceId.c_str(),
                                                                                            _CurrentWorkspaceNeedsSave(),
                                                                                            !tabsToReplace.empty(),
                                                                                            _settings);
        auto manager = loadedOpenState.Manager;
        const auto& openPlan = loadedOpenState.OpenPlan;
        const auto& startupState = loadedOpenState.StartupState;
        const auto& executionPlan = loadedOpenState.ExecutionPlan;
        _ConfigureTerminalContentWrapper(openPlan.TargetWorkspace);
        auto startupActions = manager.BuildStartupActions(openPlan.TargetWorkspace, _settings);
        {
            Json::Value payload{ Json::objectValue };
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "requestedWorkspaceId", workspaceId.c_str());
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "currentWorkspaceId", _currentWorkspaceId.c_str());
            payload["openInNewWindow"] = openInNewWindow;
            payload["startupActionCount"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(startupActions.size()) };
            payload["pendingNodeCount"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(startupState.PendingNodeIds.size()) };
            payload["tabCount"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(_tabs.Size()) };
            payload["disposition"] = Json::Int{ gsl::narrow_cast<int>(executionPlan.Disposition) };
            payload["setCurrentWorkspaceBeforeActions"] = executionPlan.SetCurrentWorkspaceBeforeActions;
            payload["setCurrentWorkspaceAfterActions"] = executionPlan.SetCurrentWorkspaceAfterActions;
            payload["replacePendingNodeQueues"] = executionPlan.ReplacePendingNodeQueues;
            payload["confirmSaveCurrentWorkspace"] = executionPlan.ConfirmSaveCurrentWorkspace;
            payload["removeCapturedTabsAfterActions"] = executionPlan.RemoveCapturedTabsAfterActions;
            if (executionPlan.ExistingWindowId.has_value())
            {
                payload["existingWindowId"] = Json::UInt64{ *executionPlan.ExistingWindowId };
            }
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_open_plan", payload);
        }
        if (executionPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceOpenExecutionDisposition::SummonExistingWindow)
        {
            appState.Flush();
            SummonWindowRequested.raise(*this, winrt::box_value(*executionPlan.ExistingWindowId));
            co_return;
        }

        if (executionPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceOpenExecutionDisposition::Missing ||
            executionPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceOpenExecutionDisposition::NoStartupActions)
        {
            appState.Flush();
            co_return;
        }

        if (executionPlan.ConfirmSaveCurrentWorkspace)
        {
            const auto weak = get_weak();
            const auto proceed = co_await _ConfirmSaveWorkspaceOnExit();
            if (!weak.get() || !proceed)
            {
                co_return;
            }
        }

        if (executionPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceOpenExecutionDisposition::OpenInNewWindow)
        {
            appState.Flush();
            // The workspace identity travels only with this one window request.
            // It must never be persisted in ApplicationState: a stale request
            // would otherwise turn a later normal launch into a workspace window.
            {
                Json::Value payload{ Json::objectValue };
                terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", workspaceId.c_str());
                payload["startupActionCount"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(startupActions.size()) };
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_open_move_content", payload);
            }
            const auto workspaceWindowName = winrt::hstring{ std::wstring{ L"workspace:" } + std::wstring{ workspaceId.c_str() } };
            _MoveContent(std::move(startupActions), workspaceWindowName, 0);
            co_return;
        }

        if (executionPlan.SetSaveBaseline)
        {
            _SetCurrentWorkspaceSaveBaseline(openPlan.TargetWorkspace);
        }
        if (executionPlan.SetCurrentWorkspaceBeforeActions)
        {
            CurrentWorkspaceId(workspaceId);
        }
        appState.Flush();
        bool clearPendingWorkspaceNodeQueues = false;
        auto clearPendingInputVisibility = wil::scope_exit([&]() noexcept {
            if (clearPendingWorkspaceNodeQueues)
            {
                _workspaceExtension->ClearPendingWorkspaceNodeQueues();
            }
        });
        if (executionPlan.ReplacePendingNodeQueues)
        {
            _workspaceExtension->ReplacePendingWorkspaceNodeInputVisibility(startupState.PendingNodeInputVisibility);
            _workspaceExtension->ReplacePendingWorkspaceNodeIds(startupState.PendingNodeIds);
            clearPendingWorkspaceNodeQueues = true;
        }

        auto suspend = !tabsToReplace.empty();
        for (size_t i = 0; i < startupActions.size(); ++i)
        {
            if (suspend)
            {
                co_await wil::resume_foreground(Dispatcher(), CoreDispatcherPriority::Low);
            }

            if (_workspaceExtension->ShouldSkipStartupAction(startupActions[i], startupActions, i))
            {
                Json::Value payload{ Json::objectValue };
                terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", workspaceId.c_str());
                payload["index"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(i) };
                payload["action"] = Json::Int{ gsl::narrow_cast<int>(startupActions[i].Action()) };
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_startup_action_skipped", payload);
                suspend = true;
                continue;
            }

            {
                Json::Value payload{ Json::objectValue };
                terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", workspaceId.c_str());
                payload["index"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(i) };
                payload["action"] = Json::Int{ gsl::narrow_cast<int>(startupActions[i].Action()) };
                if (const auto newTabArgs = startupActions[i].Args().try_as<Microsoft::Terminal::Settings::Model::NewTabArgs>())
                {
                    if (const auto terminalArgs = newTabArgs.ContentArgs().try_as<Microsoft::Terminal::Settings::Model::NewTerminalArgs>())
                    {
                        terminal::workspacechat::AddOptionalDiagnosticString(payload, "profile", terminalArgs.Profile().c_str());
                        terminal::workspacechat::AddOptionalDiagnosticString(payload, "tabTitle", terminalArgs.TabTitle().c_str());
                        terminal::workspacechat::AddDiagnosticTextFields(payload, "startingDirectory", terminalArgs.StartingDirectory().c_str());
                        terminal::workspacechat::AddDiagnosticTextFields(payload, "commandline", terminalArgs.Commandline().c_str());
                    }
                }
                else if (const auto sendInputArgs = startupActions[i].Args().try_as<Microsoft::Terminal::Settings::Model::SendInputArgs>())
                {
                    terminal::workspacechat::AddDiagnosticTextFields(payload, "input", sendInputArgs.Input().c_str());
                }
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_startup_action_dispatch", payload);
            }
            _actionDispatch->DoAction(startupActions[i]);
            suspend = true;
        }

        // Startup created one native Tab per workspace node. Each node Tab's
        // wrapper now creates its own command-level host from node.Commands.
        _ConfigureTerminalContentWrapper(openPlan.TargetWorkspace);

        // The regular TerminalPage startup path installs this host itself.
        // Opening a workspace in the current window has its own action loop,
        // so it must do the same after all logical command tabs exist.
        if (executionPlan.SetCurrentWorkspaceAfterActions)
        {
            CurrentWorkspaceId(workspaceId);
        }
        if (executionPlan.FocusActiveContentAfterActions)
        {
            if (const auto& tabImpl{ _GetFocusedTabImpl() })
            {
                if (const auto& content{ tabImpl->GetActiveContent() })
                {
                    content.Focus(FocusState::Programmatic);
                }
            }
        }

        if (executionPlan.RemoveCapturedTabsAfterActions && NumberOfTabs() > tabsToReplace.size())
        {
            for (const auto& tab : tabsToReplace)
            {
                _RemoveTab(tab);
            }
        }

    }

    void TerminalPage::WorkspaceDefinitionsChanged()
    {
        _RefreshWorkspaceChrome();

        if (_workspaceExtension->WorkspaceEditorEditMode() && _workspaceExtension->WorkspaceDefinitionsDirty())
        {
            ActionSaveFailed(RS_(L"WorkspaceEditor_ExternalChangeError"));
            return;
        }

        _LoadWorkspaceEditorState();
        if (_workspaceManagerContent)
        {
            _RebuildWorkspaceManagerTab();
        }
    }

    safe_void_coroutine TerminalPage::_OpenWorkspaceManager()
    {
        _workspaceExtension->WorkspaceEditorEditMode() = true;
        _workspaceExtension->WorkspaceDefinitionsDirty() = false;
        _LoadWorkspaceEditorState(false);
        if (_workspaceExtension->WorkspaceEditorManager().Workspaces().empty())
        {
            _workspaceExtension->WorkspaceManagerNavSelection() = 0;
        }
        else
        {
            _workspaceExtension->WorkspaceManagerNavSelection() = 1000 + gsl::narrow_cast<int32_t>(_workspaceExtension->WorkspaceEditorSelectedIndex() * 100);
        }
        if (!_workspaceManagerTab)
        {
            _workspaceManagerContent = _workspaceExtension->CreateWorkspaceManagerPaneContent(_BuildWorkspaceManagerContent(), _settings);
            auto resultPane = std::make_shared<Pane>(_workspaceManagerContent);
            _workspaceManagerTab = _CreateNewTabFromPane(resultPane);
            _tabView.SelectedItem(_workspaceManagerTab.TabViewItem());
        }
        else
        {
            _RebuildWorkspaceManagerTab();
            _tabView.SelectedItem(_workspaceManagerTab.TabViewItem());
        }
        co_return;
    }

    void TerminalPage::_LoadWorkspaceEditorState(const bool preserveSelection)
    {
        const auto selectedWorkspaceId = preserveSelection ? _SelectedWorkspaceId() : std::wstring{};
        const auto state = ::terminal::workspace::LoadWorkspaceEditorState(selectedWorkspaceId,
                                                                           _currentWorkspaceId.c_str(),
                                                                           _workspaceExtension->WorkspaceEditorSelectedIndex());
        _workspaceExtension->WorkspaceEditorManager() = state.Manager;
        _workspaceExtension->WorkspaceEditorSelectedIndex() = state.SelectedWorkspaceIndex;
    }

    void TerminalPage::_SetSelectedWorkspaceIndex(const size_t index)
    {
        const auto& workspaces = _workspaceExtension->WorkspaceEditorManager().Workspaces();
        if (workspaces.empty())
        {
            _workspaceExtension->WorkspaceEditorSelectedIndex() = 0;
            return;
        }

        _workspaceExtension->WorkspaceEditorSelectedIndex() = std::min(index, workspaces.size() - 1);
    }

    std::wstring TerminalPage::_SelectedWorkspaceId() const
    {
        if (const auto workspace = _SelectedWorkspaceForEditing())
        {
            return workspace->Id;
        }
        return {};
    }

    Microsoft::Terminal::Settings::Model::implementation::WorkspaceCurrentState TerminalPage::_LoadCurrentWorkspaceStateSnapshot() const
    {
        return ::terminal::workspace::LoadCurrentWorkspaceState(_currentWorkspaceId.c_str(),
                                                                RS_(L"WorkspaceDefaultUnsaved").c_str(),
                                                                RS_(L"WorkspaceUnsavedName").c_str());
    }

    std::wstring TerminalPage::_CurrentWorkspaceDisplayName() const
    {
        return _LoadCurrentWorkspaceStateSnapshot().DisplayName;
    }

    std::wstring TerminalPage::_CurrentWorkspaceTabRowName() const
    {
        return _LoadCurrentWorkspaceStateSnapshot().TabRowName;
    }

    Microsoft::Terminal::Settings::Model::implementation::WorkspaceSaveTargetState TerminalPage::_LoadWorkspaceSaveTargetStateSnapshot() const
    {
        return ::terminal::workspace::LoadWorkspaceSaveTargetState(_currentWorkspaceId.c_str(), _lastWorkspaceId);
    }

    std::wstring TerminalPage::_ResolvedWorkspaceSaveTargetId() const
    {
        return _LoadWorkspaceSaveTargetStateSnapshot().Id;
    }

    std::wstring TerminalPage::_ResolvedWorkspaceSaveTargetName() const
    {
        return _LoadWorkspaceSaveTargetStateSnapshot().Name;
    }

    std::optional<winrt::Windows::UI::Color> TerminalPage::_CurrentWorkspaceColor() const
    {
        const auto state = _LoadCurrentWorkspaceStateSnapshot();
        if (!state.BackgroundColor.empty())
        {
            return _parseWorkspaceColor(state.BackgroundColor);
        }

        return std::nullopt;
    }

    bool TerminalPage::_CurrentWorkspaceLocked() const
    {
        return _workspaceExtension && _workspaceExtension->IsCurrentWorkspaceLocked();
    }

    void TerminalPage::_RemoveWorkspaceManagedTabsForLockedState()
    {
        const auto settingsTab = _settingsTab;
        const auto workspaceManagerTab = _workspaceManagerTab;

        if (settingsTab)
        {
            _RemoveTab(settingsTab);
        }

        if (workspaceManagerTab)
        {
            _RemoveTab(workspaceManagerTab);
        }
    }

    void TerminalPage::_SetCurrentWorkspaceLocked(const bool locked)
    {
        if (_currentWorkspaceId.empty())
        {
            return;
        }

        if (!::terminal::workspace::PersistWorkspaceLockedState(_currentWorkspaceId.c_str(), locked))
        {
            ActionSaveFailed(RS_(L"WorkspaceSaveFailedWorkspacesFile"));
            return;
        }

        if (locked)
        {
            _RemoveWorkspaceManagedTabsForLockedState();
        }

        _LoadWorkspaceEditorState();
        _UpdateWorkspaceTabRow();
        _UpdateWorkspaceInteractionState();
        _updateAllTabCloseButtons();

        Json::Value payload{ Json::objectValue };
        payload["locked"] = locked;
        terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", _currentWorkspaceId.c_str());
        payload["hasNewTabButton"] = static_cast<bool>(_newTabButton);
        if (_newTabButton)
        {
            payload["newTabButtonVisibility"] = _visibilityName(_newTabButton.Visibility());
        }
        _logWorkspaceChatDiagnostic(L"workspace_lock_state_applied", payload);
    }

    std::optional<uint64_t> TerminalPage::_FindOpenWorkspaceWindowId(const std::wstring_view workspaceId) const
    {
        return ::terminal::workspace::FindOpenWorkspaceWindowId(workspaceId);
    }

    void TerminalPage::_CloseOpenWorkspaceWindow(const std::wstring_view workspaceId)
    {
        // The local deletion path is finalized below by raising
        // CloseWindowRequested directly. Only route a close action when the
        // workspace belongs to another open window.
        if (workspaceId.empty() || workspaceId == _currentWorkspaceId.c_str())
        {
            return;
        }

        const auto windowId = _FindOpenWorkspaceWindowId(workspaceId);
        if (!windowId.has_value() || *windowId == _WindowProperties.WindowId())
        {
            return;
        }

        std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> closeActions;
        closeActions.emplace_back(Microsoft::Terminal::Settings::Model::ShortcutAction::CloseWindow, nullptr);
        _MoveContent(std::move(closeActions), winrt::to_hstring(*windowId), std::numeric_limits<uint32_t>::max());

        Json::Value payload{ Json::objectValue };
        payload["windowId"] = Json::UInt64{ *windowId };
        terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", workspaceId);
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_deleted_window_close_requested", payload);
    }

    void TerminalPage::_RefreshWorkspaceChrome()
    {
        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();
    }

    Settings::Model::TabCloseButtonVisibility TerminalPage::_CurrentTabCloseButtonVisibility() const
    {
        if (_CurrentWorkspaceLocked())
        {
            return Settings::Model::TabCloseButtonVisibility::Never;
        }
        return Settings::Model::TabCloseButtonVisibility::Always;
    }

    bool TerminalPage::_WorkspaceMiddleClickHookEnabled(const Settings::Model::TabCloseButtonVisibility visibility) const
    {
        return visibility == Settings::Model::TabCloseButtonVisibility::Never && !_CurrentWorkspaceLocked();
    }

    bool TerminalPage::_ShouldBlockSplitForWorkspaceManagedTab(const winrt::com_ptr<Tab>& tab) const noexcept
    {
        return tab && (*tab == _settingsTab || *tab == _workspaceManagerTab);
    }

    void TerminalPage::_RefreshWorkspaceUiAfterSettingsReload()
    {
        _UpdateWorkspaceTabRow();
        if (_workspaceManagerContent)
        {
            _workspaceExtension->UpdateWorkspaceManagerPaneSettings(_workspaceManagerContent, _settings);
            _RebuildWorkspaceManagerTab();
        }
    }

    void TerminalPage::_updateAllTabCloseButtons()
    {
        if (!_tabView)
        {
            return;
        }

        const auto visibility = _CurrentTabCloseButtonVisibility();
        _tabItemMiddleClickHookEnabled = _WorkspaceMiddleClickHookEnabled(visibility);

        for (const auto& tab : _tabs)
        {
            tab.CloseButtonVisibility(visibility);
        }

        switch (visibility)
        {
        case Settings::Model::TabCloseButtonVisibility::Never:
            _tabView.CloseButtonOverlayMode(MUX::Controls::TabViewCloseButtonOverlayMode::Auto);
            break;
        case Settings::Model::TabCloseButtonVisibility::Hover:
            _tabView.CloseButtonOverlayMode(MUX::Controls::TabViewCloseButtonOverlayMode::OnPointerOver);
            break;
        case Settings::Model::TabCloseButtonVisibility::ActiveOnly:
        default:
            _tabView.CloseButtonOverlayMode(MUX::Controls::TabViewCloseButtonOverlayMode::Always);
            break;
        }
    }

    void TerminalPage::_UpdateWorkspaceInteractionState()
    {
        const auto locked = _CurrentWorkspaceLocked();
        const auto canDragDrop = CanDragDrop() && !locked;
        if (_tabView)
        {
            _tabView.CanReorderTabs(canDragDrop);
            _tabView.CanDragTabs(canDragDrop);
        }
        if (_newTabButton)
        {
            _newTabButton.Visibility(locked ? WUX::Visibility::Collapsed : WUX::Visibility::Visible);
        }
    }

    void TerminalPage::_UpdateWorkspaceTabRow()
    {
        if (!_tabRow)
        {
            return;
        }

        const auto isWorkspaceWindow = !_currentWorkspaceId.empty();
        const auto currentWorkspaceState = _LoadCurrentWorkspaceStateSnapshot();
        const auto name = isWorkspaceWindow ? currentWorkspaceState.TabRowName : std::wstring{};
        _tabRow.WorkspaceName(winrt::hstring{ name });
        _tabRow.WorkspaceNameVisibility(name.empty() ? WUX::Visibility::Collapsed : WUX::Visibility::Visible);
        _tabRow.WorkspaceMenuVisibility(isWorkspaceWindow ? WUX::Visibility::Collapsed : WUX::Visibility::Visible);
        WUX::Controls::IconElement workspaceIcon{ nullptr };
        if (isWorkspaceWindow && !currentWorkspaceState.Icon.empty())
        {
            workspaceIcon = _CreateNewTabFlyoutIcon(winrt::hstring{ currentWorkspaceState.Icon });
        }
        if (workspaceIcon)
        {
            if (const auto frameworkElement = workspaceIcon.try_as<FrameworkElement>())
            {
                frameworkElement.Width(16);
                frameworkElement.Height(16);
                frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                frameworkElement.VerticalAlignment(VerticalAlignment::Center);
            }
        }
        _tabRow.WorkspaceIconElement(workspaceIcon);
        // Saving a runtime window into a workspace is no longer supported.
        _tabRow.WorkspaceDirtyVisibility(WUX::Visibility::Collapsed);
        _tabRow.WorkspaceSaveVisibility(WUX::Visibility::Collapsed);
        _tabRow.WorkspaceLockGlyph(_CurrentWorkspaceLocked() ? L"\xE72E" : L"\xE785");
        _tabRow.WorkspaceLockVisibility(WUX::Visibility::Collapsed);

        if (!currentWorkspaceState.BackgroundColor.empty())
        {
            if (const auto color = _parseWorkspaceColor(currentWorkspaceState.BackgroundColor))
            {
                _tabRow.WorkspaceBackgroundBrush(SolidColorBrush{ *color });
                _tabRow.WorkspaceForegroundBrush(SolidColorBrush{ _workspaceForegroundColor(*color) });
            }
        }
        else if (const auto resources = Application::Current().Resources())
        {
            _tabRow.WorkspaceBackgroundBrush(resources.Lookup(winrt::box_value(L"TabViewButtonBackground")).try_as<Media::Brush>());
            _tabRow.WorkspaceForegroundBrush(resources.Lookup(winrt::box_value(L"TabViewButtonForeground")).try_as<Media::Brush>());
        }

        _UpdateWorkspaceChatHeader();
    }
