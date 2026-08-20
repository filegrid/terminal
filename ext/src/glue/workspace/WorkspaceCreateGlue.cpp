    void TerminalPage::_InitializeWorkspaceTabRowUi()
    {
        _UpdateWorkspaceTabRow();
        _rearranging = false;
        _InitializeWorkspaceChatUi();

        _UpdateWorkspaceInteractionState();
        _UpdateTerminalContentHostClip();
        _tabView.TabDragStarting({ get_weak(), &TerminalPage::_TabDragStarted });
        _tabView.TabDragCompleted({ get_weak(), &TerminalPage::_TabDragCompleted });

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        _newTabButton = tabRowImpl->NewTabButton();
        // Workspace definitions are created and edited exclusively in the manager.
        // Do not wire the legacy title-bar save affordance.
        tabRowImpl->WorkspaceSaveButton().Visibility(WUX::Visibility::Collapsed);
        const auto showWorkspaceMenu = [weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_ShowWorkspaceNameMenu();
            }
        };
        // The default window uses the icon-only switcher. Workspace windows
        // retain their named button; both open the same constrained flyout.
        tabRowImpl->WorkspaceMenuButton().Click(showWorkspaceMenu);
        tabRowImpl->WorkspaceNameButton().Click(showWorkspaceMenu);
        tabRowImpl->WorkspaceNameButton().DoubleTapped([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                auto& workspaceNameTapTimer = self->_workspaceExtension->WorkspaceNameTapTimer();
                if (workspaceNameTapTimer)
                {
                    workspaceNameTapTimer.Stop();
                }
                self->_BeginWorkspaceNameEdit();
            }
        });
        tabRowImpl->WorkspaceNameEditor().LostFocus([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_CommitWorkspaceNameEdit();
            }
        });
        tabRowImpl->WorkspaceNameEditor().KeyDown([weakThis{ get_weak() }](auto&&, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e) {
            if (auto self{ weakThis.get() })
            {
                if (e.OriginalKey() == Windows::System::VirtualKey::Enter)
                {
                    self->_workspaceExtension->SetWorkspaceNamePressedEnter(true);
                }
            }
        });
        tabRowImpl->WorkspaceNameEditor().KeyUp([weakThis{ get_weak() }](auto&&, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e) {
            if (auto self{ weakThis.get() })
            {
                const auto key = e.OriginalKey();
                if (key == Windows::System::VirtualKey::Enter && self->_workspaceExtension->WorkspaceNamePressedEnter())
                {
                    self->_CommitWorkspaceNameEdit();
                }
                else if (key == Windows::System::VirtualKey::Escape)
                {
                    self->_CancelWorkspaceNameEdit();
                }
                self->_workspaceExtension->SetWorkspaceNamePressedEnter(false);
            }
        });
    }

    void TerminalPage::_PrepareStartupWorkspaceState()
    {
        if (_startupWorkspaceId.empty())
        {
            return;
        }

        CurrentWorkspaceId(_startupWorkspaceId);
        const auto startupState = ::terminal::workspace::LoadWorkspaceStartupState(_startupWorkspaceId.c_str());
        _workspaceExtension->ReplacePendingWorkspaceNodeInputVisibility(std::move(startupState.PendingNodeInputVisibility));
        _workspaceExtension->ReplacePendingWorkspaceNodeIds(std::move(startupState.PendingNodeIds));
        _startupWorkspaceId.clear();
    }

    void TerminalPage::_ClearPendingWorkspaceStartupState() noexcept
    {
        _workspaceExtension->ClearPendingWorkspaceNodeQueues();
    }

    bool TerminalPage::_ShouldSkipWorkspaceStartupAction(const ActionAndArgs& action,
                                                         const std::vector<ActionAndArgs>& actions,
                                                         const size_t index)
    {
        return _workspaceExtension->ShouldSkipStartupAction(action, actions, index);
    }
