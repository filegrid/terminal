    void TerminalPage::_InitializeWorkspaceChatUi()
    {
        WorkspaceChatTitle().Text(RS_(L"WorkspaceChat_Header"));
        WorkspaceChatInput().PlaceholderText(RS_(L"WorkspaceChat_InputPlaceholder"));
        WorkspaceChatClearDraftButton().Content(winrt::box_value(RS_(L"WorkspaceChat_ClearDraft")));
        WorkspaceChatClearDraftButton().Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                auto& chatUiState = self->_workspaceExtension->WorkspaceChatUiState();
                chatUiState.DraftUpdateInProgress = true;
                self->WorkspaceChatInput().Text(L"");
                chatUiState.DraftUpdateInProgress = false;
                self->_PersistWorkspaceChatDraft();
            }
        });
        WorkspaceChatToggleButton().Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_ToggleWorkspaceChatCollapsed();
            }
        });
        WorkspaceChatInput().TextChanged([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_UpdateWorkspaceChatInputHeight();
                if (!self->_workspaceExtension->WorkspaceChatUiState().DraftUpdateInProgress)
                {
                    self->_PersistWorkspaceChatDraft();
                }
            }
        });
        WorkspaceChatInput().SizeChanged([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_UpdateWorkspaceChatInputHeight();
            }
        });
        auto sendAccelerator = WUX::Input::KeyboardAccelerator{};
        sendAccelerator.Key(Windows::System::VirtualKey::Enter);
        sendAccelerator.Modifiers(Windows::System::VirtualKeyModifiers::Control);
        sendAccelerator.Invoked([weakThis{ get_weak() }](auto&&, const WUX::Input::KeyboardAcceleratorInvokedEventArgs& e) {
            if (auto self{ weakThis.get() })
            {
                e.Handled(true);
                self->_SendWorkspaceChatMessage();
            }
        });
        WorkspaceChatInput().KeyboardAccelerators().Append(sendAccelerator);

        WorkspaceChatResizeHandle().PointerPressed([weakThis{ get_weak() }](const auto& sender, const WUX::Input::PointerRoutedEventArgs& e) {
            if (auto self{ weakThis.get() })
            {
                if (self->_workspaceExtension->WorkspaceChatUiState().Collapsed)
                {
                    return;
                }

                if (const auto handle = sender.try_as<UIElement>())
                {
                    handle.CapturePointer(e.Pointer());
                }

                auto& chatUiState = self->_workspaceExtension->WorkspaceChatUiState();
                chatUiState.ResizeActive = true;
                chatUiState.ResizeStartHeight = self->WorkspaceChatPanelRow().Height().Value;
                chatUiState.ResizeStartPointerY = e.GetCurrentPoint(self->TabContent()).Position().Y;
                e.Handled(true);
            }
        });
        WorkspaceChatResizeHandle().PointerEntered([](auto&&, auto&&) {
            Window::Current().CoreWindow().PointerCursor(CoreCursor{ CoreCursorType::SizeNorthSouth, 0 });
        });
        WorkspaceChatResizeHandle().PointerMoved([weakThis{ get_weak() }](auto&&, const WUX::Input::PointerRoutedEventArgs& e) {
            if (auto self{ weakThis.get() })
            {
                auto& chatUiState = self->_workspaceExtension->WorkspaceChatUiState();
                if (!chatUiState.ResizeActive)
                {
                    return;
                }

                const auto currentY = e.GetCurrentPoint(self->TabContent()).Position().Y;
                const auto delta = chatUiState.ResizeStartPointerY - currentY;
                const auto maxHeight = std::max(WorkspaceChatDefaultExpandedHeight, self->TabContent().ActualHeight() - 120.0);
                chatUiState.ExpandedHeight = std::clamp(chatUiState.ResizeStartHeight + delta, WorkspaceChatMinimumExpandedHeight, maxHeight);
                self->WorkspaceChatPanelRow().Height(GridLengthHelper::FromValueAndType(chatUiState.ExpandedHeight, GridUnitType::Pixel));
                self->_UpdateTerminalContentHostClip();
                e.Handled(true);
            }
        });
        WorkspaceChatResizeHandle().PointerExited([](auto&&, auto&&) {
            Window::Current().CoreWindow().PointerCursor(CoreCursor{ CoreCursorType::Arrow, 0 });
        });
        WorkspaceChatResizeHandle().PointerReleased([weakThis{ get_weak() }](const auto& sender, const WUX::Input::PointerRoutedEventArgs& e) {
            if (auto self{ weakThis.get() })
            {
                if (const auto handle = sender.try_as<UIElement>())
                {
                    handle.ReleasePointerCapture(e.Pointer());
                }

                self->_workspaceExtension->WorkspaceChatUiState().ResizeActive = false;
                Window::Current().CoreWindow().PointerCursor(CoreCursor{ CoreCursorType::Arrow, 0 });
                e.Handled(true);
            }
        });
        WorkspaceChatResizeHandle().PointerCaptureLost([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_workspaceExtension->WorkspaceChatUiState().ResizeActive = false;
                Window::Current().CoreWindow().PointerCursor(CoreCursor{ CoreCursorType::Arrow, 0 });
            }
        });

        if (!_workspaceChatOutputCaptureTimer)
        {
            _workspaceChatOutputCaptureTimer = WUX::DispatcherTimer{};
            _workspaceChatOutputCaptureTimer.Interval(std::chrono::milliseconds(250));
            _workspaceChatOutputCaptureTimer.Tick([weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    self->_ProcessPendingTerminalOutputCaptures();
                }
            });
        }

        _UpdateWorkspaceChatHeader();
        _ApplyWorkspaceChatStateForFocusedTab();
    }

    void TerminalPage::_UpdateTerminalContentHostClip()
    {
        if (!_terminalContentWrapper)
        {
            return;
        }

        const auto width = static_cast<float>(_terminalContentWrapper.ActualWidth());
        const auto height = static_cast<float>(_terminalContentWrapper.ActualHeight());
        if (width <= 0.0f || height <= 0.0f)
        {
            _terminalContentWrapper.Clip(nullptr);
            return;
        }

        Media::RectangleGeometry clip{};
        clip.Rect(Windows::Foundation::Rect{ 0.0f, 0.0f, width, height });
        _terminalContentWrapper.Clip(clip);
    }

    void TerminalPage::_UpdateWorkspaceChatHeader()
    {
        if (!_tabContent)
        {
            return;
        }

        WorkspaceChatWorkspaceName().Text(winrt::hstring{ _CurrentWorkspaceDisplayName() });
        const auto collapsed = _workspaceExtension->WorkspaceChatUiState().Collapsed;
        const auto tooltipText = collapsed ? RS_(L"WorkspaceChat_Expand") : RS_(L"WorkspaceChat_Collapse");
        WUX::Controls::ToolTipService::SetToolTip(WorkspaceChatToggleButton(), winrt::box_value(tooltipText));
        WorkspaceChatToggleGlyph().Text(collapsed ? L"\xE70E" : L"\xE70D");
    }

    void TerminalPage::_PreparePendingWorkspaceNodeInputVisibility(const Workspace& workspace)
    {
        _workspaceExtension->ReplacePendingWorkspaceNodeInputVisibility(Microsoft::Terminal::Settings::Model::implementation::VisibleWorkspaceNodeInputVisibility(workspace));
    }

    void TerminalPage::_PreparePendingWorkspaceNodeIds(const Workspace& workspace)
    {
        const auto startupState = ::terminal::workspace::ResolveWorkspaceStartupState(workspace, _settings);
        _workspaceExtension->ReplacePendingWorkspaceNodeIds(std::move(startupState.PendingNodeIds));
    }

    void TerminalPage::_ApplyWorkspaceChatStateForFocusedTab()
    {
        const auto tab = _GetFocusedTabImpl();
        auto& chatUiState = _workspaceExtension->WorkspaceChatUiState();
        chatUiState.EnabledForActiveTab = tab && tab->ShowWorkspaceInputPanel();
        _SetWorkspaceChatCollapsed(chatUiState.Collapsed);
        _ReloadWorkspaceChatState();
    }

    void TerminalPage::_FocusActiveTabSurface()
    {
        const auto tab = _GetFocusedTab();
        if (!tab)
        {
            return;
        }

        const auto tabImpl = _GetTabImpl(tab);
        if (tabImpl && tabImpl->ShowWorkspaceInputPanel())
        {
            Dispatcher().RunAsync(CoreDispatcherPriority::Low, [weakThis{ get_weak() }, weakTab{ winrt::make_weak(tab) }]() {
                if (auto self{ weakThis.get() })
                {
                    if (const auto focusedTab{ weakTab.get() })
                    {
                        if (focusedTab == self->_GetFocusedTab())
                        {
                            if (const auto focusedTabImpl = self->_GetTabImpl(focusedTab);
                                focusedTabImpl && focusedTabImpl->ShowWorkspaceInputPanel() && self->_workspaceExtension->WorkspaceChatUiState().EnabledForActiveTab)
                            {
                                self->WorkspaceChatInput().Focus(FocusState::Programmatic);
                            }
                        }
                    }
                }
            });
            return;
        }

        tab.Focus(FocusState::Programmatic);
    }

    void TerminalPage::_ApplyWorkspaceNodeTitlePolicy(const winrt::com_ptr<Tab>& tab)
    {
        if (!tab)
        {
            return;
        }

        if (const auto node = _ResolveCurrentWorkspaceNode(tab))
        {
            const auto lockTitle = node->UseNodeNameAsTabTitle && !node->Name.empty();
            tab->SetTitleLock(lockTitle, winrt::hstring{ node->Name });
        }
        else
        {
            tab->SetTitleLock(false);
        }
    }

    void TerminalPage::_ApplyWorkspaceNodeTitlePolicy(const size_t nodeIndex)
    {
        const auto* workspace = _SelectedCurrentWorkspaceForEditingPtr();
        if (!workspace || nodeIndex >= workspace->Nodes.size())
        {
            return;
        }

        if (const auto tab = _GetWorkspaceBackedTabByNodeIndex(nodeIndex))
        {
            const auto& node = workspace->Nodes.at(nodeIndex);
            const auto lockTitle = node.UseNodeNameAsTabTitle && !node.Name.empty();
            tab->SetTitleLock(lockTitle, winrt::hstring{ node.Name });
        }
    }

    void TerminalPage::_ReloadWorkspaceChatState()
    {
        if (!_tabContent)
        {
            return;
        }

        const auto draft = _workspaceExtension->WorkspaceChatController().LoadDraft(_CurrentWorkspaceStorageKey(),
                                                                                    _CurrentWorkspaceArtifactTabKey());
        auto& chatUiState = _workspaceExtension->WorkspaceChatUiState();
        chatUiState.DraftUpdateInProgress = true;
        WorkspaceChatInput().Text(winrt::hstring{ draft });
        chatUiState.DraftUpdateInProgress = false;
        _UpdateWorkspaceChatInputHeight();
        _UpdateWorkspaceChatHeader();
    }
