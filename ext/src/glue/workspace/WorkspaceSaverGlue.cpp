    void TerminalPage::_OpenWorkspaceSaver()
    {
        if (_CurrentWorkspaceLocked())
        {
            return;
        }

        if (WorkspaceSaver() == nullptr)
        {
            if (auto tip{ FindName(L"WorkspaceSaver").try_as<MUX::Controls::TeachingTip>() })
            {
                tip.Closed({ get_weak(), &TerminalPage::_FocusActiveControl });
            }
        }

        _UpdateTeachingTipTheme(WorkspaceSaver().try_as<winrt::Windows::UI::Xaml::FrameworkElement>());

        auto tip = WorkspaceSaver();
        tip.Title(RS_(L"WorkspaceSaver_Title"));
        tip.Subtitle(RS_(L"WorkspaceSaver_Subtitle"));
        tip.ActionButtonContent(box_value(RS_(L"WorkspaceSaver_ActionButtonContent")));
        tip.CloseButtonContent(box_value(RS_(L"WorkspaceSaver_CloseButtonContent")));

        WorkspaceSaverTextBox().Text(_SuggestedWorkspaceSaveName());

        _workspaceSaverLayoutUpdatedRevoker.revoke();
        _workspaceSaverLayoutUpdatedRevoker = WorkspaceSaverTextBox().LayoutUpdated(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                auto& count{ self->_workspaceExtension->WorkspaceSaverLayoutCount() };
                if (count < 2)
                {
                    count++;
                }

                if (count >= 2)
                {
                    self->_workspaceExtension->WorkspaceSaverLayoutUpdatedRevoker().revoke();
                    self->WorkspaceSaverTextBox().Focus(FocusState::Programmatic);
                    self->WorkspaceSaverTextBox().SelectAll();
                }
            }
        });

        _workspaceSaverPressedEnter = false;
        WorkspaceSaver().IsOpen(true);
    }

    std::wstring TerminalPage::_SuggestedWorkspaceSaveName() const
    {
        const auto manager = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        const auto singleTabTitle = NumberOfTabs() == 1 ? std::wstring{ _tabs.GetAt(0).Title().c_str() } : std::wstring{};
        return Microsoft::Terminal::Settings::Model::implementation::SuggestWorkspaceSaveName(_ResolvedWorkspaceSaveTargetName(),
                                                                                               _WindowProperties.WindowName().c_str(),
                                                                                               singleTabTitle,
                                                                                               manager.Workspaces().size(),
                                                                                               RS_fmt(L"WorkspaceGeneratedName", manager.Workspaces().size() + 1));
    }

    void TerminalPage::_NormalizeWorkspacePersistableNames(Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace) const
    {
        Microsoft::Terminal::Settings::Model::implementation::NormalizeWorkspacePersistableNames(workspace);
    }

    void TerminalPage::_SetCurrentWorkspaceSaveBaseline(const Workspace& workspace)
    {
        auto baseline = workspace;
        EnsureWorkspaceNodeTabColors(baseline, _settings);
        _currentWorkspaceSaveBaseline = std::move(baseline);
    }

    void TerminalPage::_RefreshCurrentWorkspaceSaveBaseline()
    {
        if (const auto workspace = ::terminal::workspace::LoadWorkspaceDefinition(_currentWorkspaceId.c_str()))
        {
            _SetCurrentWorkspaceSaveBaseline(*workspace);
        }
        else
        {
            _currentWorkspaceSaveBaseline.reset();
        }
    }

    void TerminalPage::_SaveCurrentWindowAsWorkspace(const winrt::hstring& workspaceName)
    {
        using Microsoft::Terminal::Settings::Model::implementation::Workspace;
        using Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager;
        using Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode;
        try
        {
            Workspace workspace;

            auto manager = WorkspaceManager::Load();
            auto workspaces = manager.Workspaces();
            const auto targetWorkspaceId = workspaceName.empty() ? _ResolvedWorkspaceSaveTargetId() : std::wstring{};

            if (!_TryCaptureCurrentWorkspace(workspace))
            {
                ActionSaveFailed(RS_(L"WorkspaceSaveFailedNoSavableTabs"));
                return;
            }

            EnsureWorkspaceNodeTabColors(workspace, _settings);

            const auto singleTabTitle = NumberOfTabs() == 1 ? std::wstring{ _tabs.GetAt(0).Title().c_str() } : std::wstring{};
            const auto savePlan = Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceForSave(workspace,
                                                                                                                 workspaces,
                                                                                                                 targetWorkspaceId,
                                                                                                                 workspaceName.c_str(),
                                                                                                                 _WindowProperties.WindowName().c_str(),
                                                                                                                 _ResolvedWorkspaceSaveTargetName(),
                                                                                                                 singleTabTitle,
                                                                                                                 RS_fmt(L"WorkspaceGeneratedName", workspaces.size() + 1));
            workspaces = savePlan.Workspaces;
            workspace = savePlan.SavedWorkspace;

            manager.SetWorkspaces(std::move(workspaces));
            if (!manager.Save())
            {
                ActionSaveFailed(RS_(L"WorkspaceSaveFailedWorkspacesFile"));
                return;
            }

            const auto bindWorkspaceNodeIdToTab = [&](const winrt::com_ptr<Tab>& tabImpl, const std::wstring& nodeId) {
                if (!tabImpl || nodeId.empty())
                {
                    return;
                }

                if (const auto rootPane = tabImpl->GetRootPane())
                {
                    rootPane->WalkTree([&](const auto& pane) {
                        if (const auto content = pane->GetContent().try_as<winrt::TerminalApp::TerminalPaneContent>())
                        {
                            if (const auto control = content.GetTermControl())
                            {
                                const auto contentId = control.ContentId();
                                if (contentId != 0)
                                {
                                    _workspaceExtension->SetWorkspaceNodeRuntimeNodeId(contentId, std::wstring{ nodeId });
                                }
                            }
                        }
                    });
                    return;
                }

                if (const auto control = tabImpl->GetActiveTerminalControl())
                {
                    const auto contentId = control.ContentId();
                    if (contentId != 0)
                    {
                        _workspaceExtension->SetWorkspaceNodeRuntimeNodeId(contentId, std::wstring{ nodeId });
                    }
                }
            };

            const auto visibleNodeIds = Microsoft::Terminal::Settings::Model::implementation::VisibleWorkspaceNodeIds(workspace);
            size_t visibleNodeIndex = 0;
            for (const auto& tab : _tabs)
            {
                const auto tabImpl = _GetTabImpl(tab);
                if (!tabImpl || !_BuildWorkspaceNodeArgs(tabImpl))
                {
                    continue;
                }

                if (visibleNodeIndex >= visibleNodeIds.size())
                {
                    break;
                }

                bindWorkspaceNodeIdToTab(tabImpl, visibleNodeIds.at(visibleNodeIndex));
                ++visibleNodeIndex;
            }

            const auto state = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
            state.LastOpenedWorkspaceId(workspace.Name);
            state.Flush();

            _SetCurrentWorkspaceSaveBaseline(workspace);
            CurrentWorkspaceId(winrt::hstring{ workspace.Name });
            if (workspace.Locked)
            {
                _RemoveWorkspaceManagedTabsForLockedState();
            }
            _UpdateWorkspaceTabRow();
            _UpdateWorkspaceInteractionState();
            _updateAllTabCloseButtons();

            // Rebuilding the attached flyout inline from the save click path can re-enter
            // the current menu/TeachingTip teardown. Defer it until the UI thread returns
            // to the dispatcher so the freshly saved workspace can still appear safely.
            Dispatcher().RunAsync(CoreDispatcherPriority::Low, [weak = get_weak()]() {
                if (auto self{ weak.get() })
                {
                    self->_CreateNewTabFlyout();
                }
            });
        }
        catch (const winrt::hresult_error& ex)
        {
            LOG_CAUGHT_EXCEPTION();
            Json::Value payload{ Json::objectValue };
            terminal::workspacechat::AppendExceptionDiagnostic(payload, ex);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "currentWorkspaceId", _currentWorkspaceId.c_str());
            _logWorkspaceChatDiagnostic(L"workspace_save_exception", payload);
            const auto message = ex.message();
            ActionSaveFailed(message.empty() ? RS_(L"WorkspaceSaveFailedGeneric") : message);
        }
        catch (const std::exception& ex)
        {
            LOG_CAUGHT_EXCEPTION();
            Json::Value payload{ Json::objectValue };
            terminal::workspacechat::AppendExceptionDiagnostic(payload, ex);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "currentWorkspaceId", _currentWorkspaceId.c_str());
            _logWorkspaceChatDiagnostic(L"workspace_save_exception", payload);
            const auto message = til::u8u16(std::string_view{ ex.what() });
            ActionSaveFailed(message.empty() ? RS_(L"WorkspaceSaveFailedGeneric") : winrt::hstring{ message });
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            Json::Value payload{ Json::objectValue };
            terminal::workspacechat::AppendUnknownExceptionDiagnostic(payload);
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "currentWorkspaceId", _currentWorkspaceId.c_str());
            _logWorkspaceChatDiagnostic(L"workspace_save_exception", payload);
            ActionSaveFailed(RS_(L"WorkspaceSaveFailedGeneric"));
        }
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> TerminalPage::_ConfirmSaveWorkspaceOnExit()
    {
        if (!_CurrentWorkspaceNeedsSave())
        {
            co_return true;
        }

        const auto dialog = FindName(L"UnsavedWorkspaceDialog").as<ContentDialog>();
        const auto message = FindName(L"UnsavedWorkspaceDialogMessage").as<TextBlock>();
        const auto textBox = FindName(L"UnsavedWorkspaceDialogTextBox").as<TextBox>();
        dialog.Title(box_value(RS_(L"WorkspaceUnsavedExitDialog_Title")));
        dialog.PrimaryButtonText(RS_(L"WorkspaceUnsavedExitDialog_Save"));
        dialog.SecondaryButtonText(RS_(L"WorkspaceUnsavedExitDialog_DontSave"));
        dialog.CloseButtonText(RS_(L"WorkspaceUnsavedExitDialog_Cancel"));

        const auto needsName = _ResolvedWorkspaceSaveTargetId().empty();
        message.Text(needsName ? RS_(L"WorkspaceUnsavedExitDialog_MessageNeedsName") :
                                 RS_(L"WorkspaceUnsavedExitDialog_Message"));
        textBox.Visibility(needsName ? WUX::Visibility::Visible : WUX::Visibility::Collapsed);
        textBox.Text(needsName ? _SuggestedWorkspaceSaveName() : L"");

        const auto result = co_await _ShowDialogHelper(L"UnsavedWorkspaceDialog");
        if (result == ContentDialogResult::Primary)
        {
            auto workspaceName = winrt::hstring{};
            if (needsName)
            {
                workspaceName = textBox.Text();
                if (workspaceName.empty())
                {
                    workspaceName = winrt::hstring{ _SuggestedWorkspaceSaveName() };
                }
            }

            _SaveCurrentWindowAsWorkspace(workspaceName);
            co_return !_CurrentWorkspaceNeedsSave();
        }

        co_return result == ContentDialogResult::Secondary;
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> TerminalPage::_ConfirmWorkspaceCloseWindowIfNeeded()
    {
        if (_CurrentWorkspaceNeedsSave() &&
            !_displayingCloseDialog)
        {
            _displayingCloseDialog = true;
            const auto weak = get_weak();
            const auto proceed = co_await _ConfirmSaveWorkspaceOnExit();
            auto strong = weak.get();
            if (!strong)
            {
                co_return false;
            }

            _displayingCloseDialog = false;
            if (!proceed)
            {
                co_return false;
            }
        }

        co_return true;
    }

    void TerminalPage::_WorkspaceSaverActionClick(const IInspectable& /*sender*/,
                                                  const IInspectable& /*eventArgs*/)
    {
        const auto workspaceName = WorkspaceSaverTextBox().Text();
        if (!workspaceName.empty())
        {
            WorkspaceSaver().IsOpen(false);
            _SaveCurrentWindowAsWorkspace(workspaceName);
        }
    }

    void TerminalPage::_WorkspaceSaverKeyDown(const IInspectable& /*sender*/,
                                              const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto key = e.OriginalKey();
        if (key == Windows::System::VirtualKey::Enter)
        {
            _workspaceSaverPressedEnter = true;
        }
    }

    void TerminalPage::_WorkspaceSaverKeyUp(const IInspectable& sender,
                                            const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto key = e.OriginalKey();
        if (key == Windows::System::VirtualKey::Enter && _workspaceSaverPressedEnter)
        {
            _WorkspaceSaverActionClick(sender, nullptr);
        }
        else if (key == Windows::System::VirtualKey::Escape)
        {
            WorkspaceSaver().IsOpen(false);
            _workspaceSaverPressedEnter = false;
        }
    }
