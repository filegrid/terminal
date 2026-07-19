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
        tabRowImpl->WorkspaceSaveButton().Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                if (self->_CurrentWorkspaceLocked())
                {
                    return;
                }

                if (self->_ResolvedWorkspaceSaveTargetId().empty())
                {
                    self->_OpenWorkspaceSaver();
                }
                else
                {
                    self->_SaveCurrentWindowAsWorkspace();
                }
            }
        });
        tabRowImpl->WorkspaceNameButton().Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                auto& workspaceNameTapTimer = self->_workspaceExtension->WorkspaceNameTapTimer();
                if (!workspaceNameTapTimer)
                {
                    workspaceNameTapTimer = WUX::DispatcherTimer{};
                    workspaceNameTapTimer.Interval(std::chrono::milliseconds(220));
                    workspaceNameTapTimer.Tick([weakThis](auto&& sender, auto&&) {
                        if (const auto timer = sender.try_as<WUX::DispatcherTimer>())
                        {
                            timer.Stop();
                        }
                        if (auto self{ weakThis.get() })
                        {
                            self->_ShowWorkspaceNameMenu();
                        }
                    });
                }
                workspaceNameTapTimer.Stop();
                workspaceNameTapTimer.Start();
            }
        });
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

        auto startupState = winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceStartupState{};
        if (const auto startupWorkspace = ::terminal::workspace::LoadResolvedWorkspaceDefinition(_startupWorkspaceId.c_str(), std::nullopt))
        {
            startupState = winrt::Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceStartupState(*startupWorkspace, _settings);
        }
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
