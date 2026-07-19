    WUX::Controls::MenuFlyout TerminalPage::_CreateWorkspaceFlyout()
    {
        auto workspaceFlyout = WUX::Controls::MenuFlyout{};
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
        const auto appState = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();

        for (const auto& entry : flyoutState.Entries)
        {
            auto workspaceItem = WUX::Controls::ToggleMenuFlyoutItem{};
            const auto& workspace = entry.Definition;
            workspaceItem.Text(winrt::hstring{ _WorkspaceDisplayName(workspace) });
            if (const auto color = _parseWorkspaceColor(workspace.BackgroundColor))
            {
                workspaceItem.Foreground(SolidColorBrush{ *color });
            }
            workspaceItem.IsChecked(entry.IsOpen);
            workspaceItem.Click([workspaceId{ winrt::hstring{ workspace.Id } }, weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto page{ weakThis.get() })
                {
                    page->_OpenWorkspace(workspaceId, Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance().OpenInNewWindow());
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

        workspaceFlyout.Items().Append(WUX::Controls::MenuFlyoutSeparator{});

        auto newWorkspaceItem = WUX::Controls::MenuFlyoutItem{};
        newWorkspaceItem.Text(RS_(L"WorkspaceNewMenuItem"));
        {
            WUX::Controls::SymbolIcon icon{};
            icon.Symbol(WUX::Controls::Symbol::Add);
            newWorkspaceItem.Icon(icon);
        }
        newWorkspaceItem.Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto page{ weakThis.get() })
            {
                page->_OpenNewWindow(NewTerminalArgs{});
            }
        });
        workspaceFlyout.Items().Append(newWorkspaceItem);

        auto lockWorkspaceItem = WUX::Controls::ToggleMenuFlyoutItem{};
        lockWorkspaceItem.Text(_CurrentWorkspaceLocked() ? RS_(L"WorkspaceLockedStateLocked") : RS_(L"WorkspaceLockedStateUnlocked"));
        lockWorkspaceItem.IsChecked(_CurrentWorkspaceLocked());
        lockWorkspaceItem.IsEnabled(!_currentWorkspaceId.empty());
        {
            WUX::Controls::FontIcon icon{};
            icon.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            icon.Glyph(_CurrentWorkspaceLocked() ? L"\xE72E" : L"\xE785");
            lockWorkspaceItem.Icon(icon);
        }
        lockWorkspaceItem.Click([weakThis{ get_weak() }](auto&& sender, auto&&) {
            if (auto page{ weakThis.get() })
            {
                if (const auto toggle = sender.try_as<WUX::Controls::ToggleMenuFlyoutItem>())
                {
                    const auto locked = !page->_CurrentWorkspaceLocked();
                    toggle.IsChecked(locked);
                    toggle.Text(locked ? RS_(L"WorkspaceLockedStateLocked") : RS_(L"WorkspaceLockedStateUnlocked"));
                    if (const auto icon = toggle.Icon().try_as<WUX::Controls::FontIcon>())
                    {
                        icon.Glyph(locked ? L"\xE72E" : L"\xE785");
                    }
                    page->_SetCurrentWorkspaceLocked(locked);
                }
            }
        });
        workspaceFlyout.Items().Append(lockWorkspaceItem);

        auto openInNewWindowItem = WUX::Controls::ToggleMenuFlyoutItem{};
        openInNewWindowItem.Text(RS_(L"WorkspaceOpenInNewWindow"));
        openInNewWindowItem.IsChecked(appState.OpenInNewWindow());
        openInNewWindowItem.Click([](auto&& sender, auto&&) {
            if (const auto toggle = sender.try_as<WUX::Controls::ToggleMenuFlyoutItem>())
            {
                const auto current = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
                current.OpenInNewWindow(toggle.IsChecked());
                current.Flush();
            }
        });
        workspaceFlyout.Items().Append(openInNewWindowItem);

        auto manageWorkspacesItem = WUX::Controls::MenuFlyoutItem{};
        manageWorkspacesItem.Text(RS_(L"WorkspaceManageMenuItem"));
        manageWorkspacesItem.IsEnabled(!_CurrentWorkspaceLocked());

        WUX::Controls::SymbolIcon manageWorkspacesIcon{};
        manageWorkspacesIcon.Symbol(WUX::Controls::Symbol::Setting);
        manageWorkspacesItem.Icon(manageWorkspacesIcon);

        manageWorkspacesItem.Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto page{ weakThis.get() })
            {
                page->_OpenWorkspaceManager();
            }
        });
        workspaceFlyout.Items().Append(manageWorkspacesItem);

        if (!_CurrentWorkspaceLocked() && _CurrentWorkspaceNeedsSave())
        {
            auto saveWorkspaceItem = WUX::Controls::MenuFlyoutItem{};
            saveWorkspaceItem.Text(RS_(L"WorkspaceSaveMenuItem"));

            WUX::Controls::SymbolIcon workspaceSaveIcon{};
            workspaceSaveIcon.Symbol(WUX::Controls::Symbol::Save);
            saveWorkspaceItem.Icon(workspaceSaveIcon);

            saveWorkspaceItem.Click([weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto page{ weakThis.get() })
                {
                    if (page->_ResolvedWorkspaceSaveTargetId().empty())
                    {
                        page->_OpenWorkspaceSaver();
                    }
                    else
                    {
                        page->_SaveCurrentWindowAsWorkspace();
                    }
                }
            });
            workspaceFlyout.Items().Append(saveWorkspaceItem);
        }

        return workspaceFlyout;
    }

    safe_void_coroutine TerminalPage::_OpenWorkspace(const winrt::hstring& workspaceId, const bool openInNewWindow)
    {
        const auto strong = get_strong();
        const auto appState = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();

        const auto loadedOpenState = ::terminal::workspace::LoadWorkspaceOpenState(workspaceId.c_str(),
                                                                                   openInNewWindow,
                                                                                   _currentWorkspaceId.c_str(),
                                                                                   _CurrentWorkspaceNeedsSave());
        auto manager = loadedOpenState.Manager;
        const auto& openPlan = loadedOpenState.OpenPlan;
        auto startupActions = manager.BuildStartupActions(openPlan.TargetWorkspace, _settings);
        std::vector<winrt::TerminalApp::Tab> tabsToReplace;
        tabsToReplace.reserve(_tabs.Size());
        for (const auto& tab : _tabs)
        {
            tabsToReplace.emplace_back(tab);
        }

        const auto executionPlan = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceOpenExecutionPlan(openPlan,
                                                                                                                            !startupActions.empty(),
                                                                                                                            !tabsToReplace.empty());
        if (executionPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceOpenExecutionDisposition::SummonExistingWindow)
        {
            if (executionPlan.SetLastOpenedWorkspaceId)
            {
                appState.LastOpenedWorkspaceId(workspaceId);
            }
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

        if (executionPlan.SetLastOpenedWorkspaceId)
        {
            appState.LastOpenedWorkspaceId(workspaceId);
        }

        if (executionPlan.Disposition == Microsoft::Terminal::Settings::Model::implementation::WorkspaceOpenExecutionDisposition::OpenInNewWindow)
        {
            if (executionPlan.UpdatePendingWorkspaceLaunch)
            {
                appState.RemovePendingWorkspaceLaunch(workspaceId);
                appState.EnqueuePendingWorkspaceLaunch(workspaceId);
            }
            appState.Flush();
            _MoveContent(std::move(startupActions), L"new", 0);
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
            _workspaceExtension->ReplacePendingWorkspaceNodeInputVisibility(openPlan.PendingNodeInputVisibility);
            _workspaceExtension->ReplacePendingWorkspaceNodeIds(openPlan.PendingNodeIds);
            clearPendingWorkspaceNodeQueues = true;
        }

        auto suspend = !tabsToReplace.empty();
        for (size_t i = 0; i < startupActions.size(); ++i)
        {
            if (suspend)
            {
                co_await wil::resume_foreground(Dispatcher(), CoreDispatcherPriority::Low);
            }

            if (_ShouldSkipWorkspaceStartupAction(startupActions[i], startupActions, i))
            {
                suspend = true;
                continue;
            }
            _actionDispatch->DoAction(startupActions[i]);
            suspend = true;
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

        if (executionPlan.SetCurrentWorkspaceAfterActions)
        {
            CurrentWorkspaceId(workspaceId);
        }
    }

    void TerminalPage::WorkspaceDefinitionsChanged()
    {
        _CreateNewTabFlyout();
        _UpdateWorkspaceTabRow();

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
        if (_CurrentWorkspaceLocked())
        {
            co_return;
        }

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
            _workspaceManagerContent = winrt::make_self<WorkspaceManagerPaneContent>(_BuildWorkspaceManagerContent(), _settings);
            auto resultPane = std::make_shared<Pane>(*_workspaceManagerContent);
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
            return workspace->Name;
        }
        return {};
    }

    std::wstring TerminalPage::_WorkspaceDisplayName(const Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace) const
    {
        return workspace.Name;
    }

    std::wstring TerminalPage::_CurrentWorkspaceDisplayName() const
    {
        return ::terminal::workspace::LoadCurrentWorkspaceState(_currentWorkspaceId.c_str(),
                                                                RS_(L"WorkspaceDefaultUnsaved").c_str(),
                                                                RS_(L"WorkspaceUnsavedName").c_str()).DisplayName;
    }

    std::wstring TerminalPage::_CurrentWorkspaceTabRowName() const
    {
        return ::terminal::workspace::LoadCurrentWorkspaceState(_currentWorkspaceId.c_str(),
                                                                RS_(L"WorkspaceDefaultUnsaved").c_str(),
                                                                RS_(L"WorkspaceUnsavedName").c_str()).TabRowName;
    }

    std::wstring TerminalPage::_ResolvedWorkspaceSaveTargetId() const
    {
        const auto manager = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        return Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceSaveTargetId(_currentWorkspaceId.c_str(), _lastWorkspaceId, manager);
    }

    std::wstring TerminalPage::_ResolvedWorkspaceSaveTargetName() const
    {
        const auto manager = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        return Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceSaveTargetName(_currentWorkspaceId.c_str(), _lastWorkspaceId, manager);
    }

    std::optional<winrt::Windows::UI::Color> TerminalPage::_CurrentWorkspaceColor() const
    {
        const auto state = ::terminal::workspace::LoadCurrentWorkspaceState(_currentWorkspaceId.c_str(),
                                                                            RS_(L"WorkspaceDefaultUnsaved").c_str(),
                                                                            RS_(L"WorkspaceUnsavedName").c_str());
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
            _workspaceManagerContent->UpdateSettings(_settings);
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

        const auto name = _CurrentWorkspaceTabRowName();
        const auto dirty = !_CurrentWorkspaceLocked() && _CurrentWorkspaceNeedsSave();
        _tabRow.WorkspaceName(winrt::hstring{ name });
        _tabRow.WorkspaceNameVisibility(name.empty() ? WUX::Visibility::Collapsed : WUX::Visibility::Visible);
        _tabRow.WorkspaceDirtyVisibility(dirty ? WUX::Visibility::Visible : WUX::Visibility::Collapsed);
        _tabRow.WorkspaceSaveVisibility(dirty ? WUX::Visibility::Visible : WUX::Visibility::Collapsed);
        _tabRow.WorkspaceLockGlyph(_CurrentWorkspaceLocked() ? L"\xE72E" : L"\xE785");
        _tabRow.WorkspaceLockVisibility(_currentWorkspaceId.empty() ? WUX::Visibility::Collapsed : WUX::Visibility::Visible);

        if (const auto color = _CurrentWorkspaceColor())
        {
            _tabRow.WorkspaceBackgroundBrush(SolidColorBrush{ *color });
            _tabRow.WorkspaceForegroundBrush(SolidColorBrush{ _workspaceForegroundColor(*color) });
        }
        else if (const auto resources = Application::Current().Resources())
        {
            _tabRow.WorkspaceBackgroundBrush(resources.Lookup(winrt::box_value(L"TabViewButtonBackground")).try_as<Media::Brush>());
            _tabRow.WorkspaceForegroundBrush(resources.Lookup(winrt::box_value(L"TabViewButtonForeground")).try_as<Media::Brush>());
        }

        _UpdateWorkspaceChatHeader();
    }
