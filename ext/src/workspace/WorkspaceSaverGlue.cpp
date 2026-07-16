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
        auto suggestedName = _ResolvedWorkspaceSaveTargetName();
        if (suggestedName.empty())
        {
            suggestedName = _WindowProperties.WindowName();
        }
        if (suggestedName.empty() && NumberOfTabs() == 1)
        {
            suggestedName = _tabs.GetAt(0).Title();
        }
        if (suggestedName.empty())
        {
            suggestedName = RS_fmt(L"WorkspaceGeneratedName",
                                   Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load().Workspaces().size() + 1);
        }
        return Microsoft::Terminal::Settings::Model::implementation::SanitizeWorkspaceDirectoryName(suggestedName, L"Workspace");
    }

    void TerminalPage::_NormalizeWorkspacePersistableNames(Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace) const
    {
        workspace.Name = Microsoft::Terminal::Settings::Model::implementation::SanitizeWorkspaceDirectoryName(workspace.Name, L"workspace");
        workspace.Id = workspace.Name;

        std::unordered_set<std::wstring> usedNodeNames;

        for (auto& node : workspace.Nodes)
        {
            node.Name = Microsoft::Terminal::Settings::Model::implementation::SanitizeWorkspaceDirectoryName(node.Name, L"tab");
            node.Name = _makeUniquePersistedName(node.Name, usedNodeNames);
            node.Id = node.Name;
        }
    }

    void TerminalPage::_SetCurrentWorkspaceSaveBaseline(const Workspace& workspace)
    {
        auto baseline = workspace;
        EnsureWorkspaceNodeTabColors(baseline, _settings);
        _currentWorkspaceSaveBaseline = std::move(baseline);
    }

    void TerminalPage::_RefreshCurrentWorkspaceSaveBaseline()
    {
        if (_currentWorkspaceId.empty())
        {
            _currentWorkspaceSaveBaseline.reset();
            return;
        }

        const auto manager = WorkspaceManager::Load();
        if (const auto workspace = manager.FindById(_currentWorkspaceId.c_str()))
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
            const auto existingWorkspaceIt = !targetWorkspaceId.empty() ?
                                                 std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& existingWorkspace) {
                                                     return existingWorkspace.Id == targetWorkspaceId;
                                                 }) :
                                                 workspaces.end();
            const auto targetWorkspaceIndex = existingWorkspaceIt != workspaces.end() ?
                                                  std::optional<size_t>{ gsl::narrow_cast<size_t>(std::distance(workspaces.begin(), existingWorkspaceIt)) } :
                                                  std::nullopt;

            if (!_TryCaptureCurrentWorkspace(workspace))
            {
                ActionSaveFailed(RS_(L"WorkspaceSaveFailedNoSavableTabs"));
                return;
            }

            EnsureWorkspaceNodeTabColors(workspace, _settings);

            if (existingWorkspaceIt != workspaces.end())
            {
                workspace.Name = existingWorkspaceIt->Name;
                workspace.Description = existingWorkspaceIt->Description;
                workspace.BackgroundColor = existingWorkspaceIt->BackgroundColor;
                workspace.Locked = true;

                for (auto& node : workspace.Nodes)
                {
                    if (node.ConnectionRef.empty() || node.Id.empty())
                    {
                        const auto existingNodeIt = std::find_if(existingWorkspaceIt->Nodes.begin(), existingWorkspaceIt->Nodes.end(), [&](const auto& existingNode) {
                            return existingNode.Id == node.Id && !existingNode.ConnectionRef.empty();
                        });
                        if (existingNodeIt != existingWorkspaceIt->Nodes.end())
                        {
                            node.ConnectionRef = existingNodeIt->ConnectionRef;
                        }
                    }
                }

                *existingWorkspaceIt = workspace;
            }
            else
            {
                if (!workspaceName.empty())
                {
                    workspace.Name = workspaceName.c_str();
                }
                else if (const auto windowName = _WindowProperties.WindowName(); !windowName.empty())
                {
                    workspace.Name = windowName.c_str();
                }
                else if (const auto previousWorkspaceName = _ResolvedWorkspaceSaveTargetName(); !previousWorkspaceName.empty())
                {
                    workspace.Name = previousWorkspaceName;
                }
                else if (NumberOfTabs() == 1)
                {
                    workspace.Name = _tabs.GetAt(0).Title().c_str();
                }

                if (workspace.Name.empty())
                {
                    workspace.Name = RS_fmt(L"WorkspaceGeneratedName", workspaces.size() + 1).c_str();
                }

                workspace.BackgroundColor = _pickUnusedWorkspaceColor(workspaces);
                workspace.Locked = true;
                workspaces.emplace_back(workspace);
            }

            std::unordered_set<std::wstring> usedWorkspaceNames;
            for (auto& candidate : workspaces)
            {
                _NormalizeWorkspacePersistableNames(candidate);
                candidate.Name = _makeUniquePersistedName(candidate.Name, usedWorkspaceNames);
                candidate.Id = candidate.Name;
            }

            const auto resolvedWorkspaceIndex = targetWorkspaceIndex.value_or(workspaces.size() - 1);
            workspace = workspaces.at(resolvedWorkspaceIndex);

            manager.SetWorkspaces(std::move(workspaces));
            if (!manager.Save())
            {
                ActionSaveFailed(RS_(L"WorkspaceSaveFailedWorkspacesFile"));
                return;
            }

            size_t visibleNodeIndex = 0;
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

            for (const auto& tab : _tabs)
            {
                const auto tabImpl = _GetTabImpl(tab);
                if (!tabImpl || !_BuildWorkspaceNodeArgs(tabImpl))
                {
                    continue;
                }

                while (visibleNodeIndex < workspace.Nodes.size() && !_workspaceNodeLoadsTab(workspace.Nodes.at(visibleNodeIndex)))
                {
                    ++visibleNodeIndex;
                }

                if (visibleNodeIndex >= workspace.Nodes.size())
                {
                    break;
                }

                bindWorkspaceNodeIdToTab(tabImpl, workspace.Nodes.at(visibleNodeIndex).Id);
                ++visibleNodeIndex;
            }

            const auto state = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
            state.LastOpenedWorkspaceId(workspace.Name);
            state.Flush();

            _SetCurrentWorkspaceSaveBaseline(workspace);
            CurrentWorkspaceId(winrt::hstring{ workspace.Name });
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
            const auto message = ex.message();
            ActionSaveFailed(message.empty() ? RS_(L"WorkspaceSaveFailedGeneric") : message);
        }
        catch (const std::exception& ex)
        {
            LOG_CAUGHT_EXCEPTION();
            const auto message = til::u8u16(std::string_view{ ex.what() });
            ActionSaveFailed(message.empty() ? RS_(L"WorkspaceSaveFailedGeneric") : winrt::hstring{ message });
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
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
