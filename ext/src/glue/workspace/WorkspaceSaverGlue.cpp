    void TerminalPage::_OpenWorkspaceSaver()
    {
        // Definitions are created only through the workspace manager.
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
        (void)workspaceName;
        // Runtime state must never be inferred back into a definition.
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
        // Runtime windows never write workspace definitions. Closing them is
        // therefore the same as closing an ordinary Terminal window.
        co_return true;
    }

    void TerminalPage::_WorkspaceSaverActionClick(const IInspectable& /*sender*/,
                                                  const IInspectable& /*eventArgs*/)
    {
        // The obsolete title-bar saver intentionally has no action.
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
