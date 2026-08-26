        WT_WORKSPACE_EXT_API winrt::Windows::UI::Xaml::Controls::MenuFlyout _CreateWorkspaceFlyout();
        WT_WORKSPACE_EXT_API safe_void_coroutine _OpenWorkspace(const winrt::hstring& workspaceId, bool openInNewWindow);
        WT_WORKSPACE_EXT_API safe_void_coroutine _OpenWorkspaceManager();
        WT_WORKSPACE_EXT_API void _SaveCurrentWindowAsWorkspace(const winrt::hstring& workspaceName = {});
        WT_WORKSPACE_EXT_API std::wstring _SuggestedWorkspaceSaveName() const;
        WT_WORKSPACE_EXT_API void _NormalizeWorkspacePersistableNames(winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace) const;
        WT_WORKSPACE_EXT_API void _SetCurrentWorkspaceSaveBaseline(const winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace);
        WT_WORKSPACE_EXT_API void _RefreshCurrentWorkspaceSaveBaseline();
        WT_WORKSPACE_EXT_API winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceSaveTargetState _LoadWorkspaceSaveTargetStateSnapshot() const;
        WT_WORKSPACE_EXT_API std::wstring _ResolvedWorkspaceSaveTargetId() const;
        WT_WORKSPACE_EXT_API std::wstring _ResolvedWorkspaceSaveTargetName() const;
        WT_WORKSPACE_EXT_API bool _TryCaptureCurrentWorkspace(winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace) const;
        WT_WORKSPACE_EXT_API std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace> _SelectedCurrentWorkspaceForEditing() const;
        WT_WORKSPACE_EXT_API std::vector<winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceLiveTabSnapshot> _BuildWorkspaceLiveTabSnapshots(const winrt::com_ptr<Tab>& targetTab, size_t* targetTabIndex = nullptr) const;
        WT_WORKSPACE_EXT_API std::optional<size_t> _GetWorkspaceBackedTabNodeIndex(const winrt::com_ptr<Tab>& tab) const;
        WT_WORKSPACE_EXT_API winrt::com_ptr<Tab> _GetWorkspaceBackedTabByNodeIndex(size_t nodeIndex) const;
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceNodeInputVisibility(size_t nodeIndex, bool showInputPanel);
        WT_WORKSPACE_EXT_API void _PersistWorkspaceNodeInputVisibilityFromTab(const winrt::com_ptr<Tab>& tab, bool showInputPanel);
        WT_WORKSPACE_EXT_API bool _PersistCurrentWorkspaceTabOrder();
        WT_WORKSPACE_EXT_API bool _CurrentWorkspaceNeedsSave() const;
        WT_WORKSPACE_EXT_API bool _CurrentWorkspaceLocked() const;
        WT_WORKSPACE_EXT_API void _RemoveWorkspaceManagedTabsForLockedState();
        WT_WORKSPACE_EXT_API void _SetCurrentWorkspaceLocked(bool locked);
        WT_WORKSPACE_EXT_API void _LoadWorkspaceEditorState(bool preserveSelection = true);
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceEditorState(const winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager,
                                                             size_t selectedWorkspaceIndex,
                                                             std::optional<int32_t> navSelection = std::nullopt);
        WT_WORKSPACE_EXT_API winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager _LoadWorkspaceEditorMutationManager() const;
        WT_WORKSPACE_EXT_API bool _TryPersistWorkspaceEditorMutation(winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager);
        WT_WORKSPACE_EXT_API std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceEditorDefinitionRemovalPlan> _PrepareWorkspaceDefinitionRemovalPlan(winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager,
                                                                                                                                                                                    std::wstring_view workspaceId) const;
        WT_WORKSPACE_EXT_API winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceEditorNodeRemovalPlan _PrepareWorkspaceNodeRemovalPlan(winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& manager,
                                                                                                                                                           std::wstring_view workspaceId,
                                                                                                                                                           std::wstring_view nodeId) const;
        WT_WORKSPACE_EXT_API void _FinalizeWorkspaceEditorMutationUi(bool removedCurrentWorkspace);
        WT_WORKSPACE_EXT_API void _RebuildWorkspaceManagerTab();
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceManagerWorkspaceIconSelection(std::wstring_view iconValue);
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceManagerNodeIconSelection(size_t nodeIndex, std::wstring_view iconValue);
        WT_WORKSPACE_EXT_API void _AppendWorkspaceManagerWorkspaceEditorContent(const winrt::Windows::UI::Xaml::Controls::StackPanel& root,
                                                                                const winrt::Windows::UI::Xaml::ResourceDictionary& workspaceResources);
        WT_WORKSPACE_EXT_API void _AppendWorkspaceManagerNodeEditorContent(const winrt::Windows::UI::Xaml::Controls::StackPanel& root,
                                                                           size_t nodeIndex,
                                                                           const winrt::Windows::UI::Xaml::ResourceDictionary& workspaceResources);
        WT_WORKSPACE_EXT_API void _AddWorkspaceDefinition(std::optional<size_t> templateIndex = std::nullopt);
        WT_WORKSPACE_EXT_API void _DeleteSelectedWorkspaceDefinition();
        WT_WORKSPACE_EXT_API void _AddWorkspaceNode();
        WT_WORKSPACE_EXT_API void _DeleteWorkspaceNode(size_t nodeIndex);
        WT_WORKSPACE_EXT_API void _SetSelectedWorkspaceIndex(size_t index);
        WT_WORKSPACE_EXT_API bool _RemoveWorkspaceDefinitionById(std::wstring_view workspaceId);
        WT_WORKSPACE_EXT_API bool _SaveWorkspaceEditorState();
        WT_WORKSPACE_EXT_API winrt::Windows::UI::Xaml::UIElement _BuildWorkspaceManagerContent();
        WT_WORKSPACE_EXT_API winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace* _SelectedWorkspaceForEditing() noexcept;
        WT_WORKSPACE_EXT_API const winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace* _SelectedWorkspaceForEditing() const noexcept;
        WT_WORKSPACE_EXT_API winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace* _SelectedCurrentWorkspaceForEditingPtr() noexcept;
        WT_WORKSPACE_EXT_API const winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace* _SelectedCurrentWorkspaceForEditingPtr() const noexcept;
        WT_WORKSPACE_EXT_API std::wstring _SelectedWorkspaceId() const;
        WT_WORKSPACE_EXT_API winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceCurrentState _LoadCurrentWorkspaceStateSnapshot() const;
        WT_WORKSPACE_EXT_API std::wstring _CurrentWorkspaceDisplayName() const;
        WT_WORKSPACE_EXT_API std::wstring _CurrentWorkspaceTabRowName() const;
        WT_WORKSPACE_EXT_API std::optional<winrt::Windows::UI::Color> _CurrentWorkspaceColor() const;
        WT_WORKSPACE_EXT_API std::optional<uint64_t> _FindOpenWorkspaceWindowId(std::wstring_view workspaceId) const;
        WT_WORKSPACE_EXT_API void _CloseOpenWorkspaceWindow(std::wstring_view workspaceId);
        WT_WORKSPACE_EXT_API void _RefreshWorkspaceChrome();
        WT_WORKSPACE_EXT_API void _UpdateWorkspaceInteractionState();
        WT_WORKSPACE_EXT_API winrt::Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility _CurrentTabCloseButtonVisibility() const;
        WT_WORKSPACE_EXT_API bool _WorkspaceMiddleClickHookEnabled(winrt::Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility visibility) const;
        WT_WORKSPACE_EXT_API void _UpdateWorkspaceTabRow();
        WT_WORKSPACE_EXT_API bool _ShouldBlockSplitForWorkspaceManagedTab(const winrt::com_ptr<Tab>& tab) const noexcept;
        WT_WORKSPACE_EXT_API void _RefreshWorkspaceUiAfterSettingsReload();
        WT_WORKSPACE_EXT_API void _InitializeWorkspaceTabRowUi();
        enum class WorkspaceNodeRemoveResult
        {
            RemovedNode,
            RemovedWorkspace,
            NotFound,
            SaveFailed,
        };
        WT_WORKSPACE_EXT_API void _InitializeWorkspaceChatUi();
        WT_WORKSPACE_EXT_API void _UpdateTerminalContentHostClip();
        WT_WORKSPACE_EXT_API void _UpdateWorkspaceChatHeader();
        WT_WORKSPACE_EXT_API void _PreparePendingWorkspaceNodeInputVisibility(const winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace);
        WT_WORKSPACE_EXT_API void _PreparePendingWorkspaceNodeIds(const winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace& workspace);
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceNodeTitlePolicy(const winrt::com_ptr<Tab>& tab);
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceNodeTitlePolicy(size_t nodeIndex);
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceChatStateForFocusedTab();
        WT_WORKSPACE_EXT_API void _FocusActiveTabSurface();
        WT_WORKSPACE_EXT_API void _ReloadWorkspaceChatState();
        WT_WORKSPACE_EXT_API safe_void_coroutine _DispatchWorkspaceChatSubmitKey(winrt::Microsoft::Terminal::Control::TermControl control, bool restoreInputFocus);
        WT_WORKSPACE_EXT_API safe_void_coroutine _DispatchWorkspaceChatWindowKeyboardInput(winrt::Microsoft::Terminal::Control::TermControl control, std::wstring text, bool restoreInputFocus);
        WT_WORKSPACE_EXT_API void _PersistWorkspaceChatDraft();
        WT_WORKSPACE_EXT_API void _UpdateWorkspaceChatInputHeight();
        WT_WORKSPACE_EXT_API void _DispatchWorkspaceChatInput(const winrt::Microsoft::Terminal::Control::TermControl& control, std::wstring_view text);
        WT_WORKSPACE_EXT_API void _SendWorkspaceChatMessage();
        WT_WORKSPACE_EXT_API void _SetWorkspaceChatCollapsed(bool collapsed);
        WT_WORKSPACE_EXT_API void _ToggleWorkspaceChatCollapsed();
        void _RegisterTerminalEvents(Microsoft::Terminal::Control::TermControl term);
        void _RegisterTabEvents(Tab& hostingTab);
        WT_WORKSPACE_EXT_API void _updateAllTabCloseButtons();
        WT_WORKSPACE_EXT_API std::wstring _CurrentWorkspaceStorageKey() const;
        WT_WORKSPACE_EXT_API std::wstring _CurrentWorkspaceArtifactTabKey() const;
        WT_WORKSPACE_EXT_API std::wstring _ResolveWorkspaceArtifactTabKey(const winrt::com_ptr<Tab>& tab) const;
        WT_WORKSPACE_EXT_API std::wstring _TrimmedWorkspaceChatInput();
        WT_WORKSPACE_EXT_API std::optional<TerminalRoutingContext> _ResolveTerminalContext(const winrt::Microsoft::Terminal::Control::TermControl& control) const;
        WT_WORKSPACE_EXT_API std::wstring _WorkspaceChatStateKey(const TerminalRoutingContext& context) const;
        WT_WORKSPACE_EXT_API std::wstring _WorkspaceChatStateKey(const winrt::Microsoft::Terminal::Control::TermControl& control) const;
        WT_WORKSPACE_EXT_API void _OnTerminalKeySent(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Microsoft::Terminal::Control::KeySentEventArgs& args);
        WT_WORKSPACE_EXT_API void _OnTerminalCharSent(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Microsoft::Terminal::Control::CharSentEventArgs& args);
        WT_WORKSPACE_EXT_API void _OnTerminalStringSent(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Microsoft::Terminal::Control::StringSentEventArgs& args);
        WT_WORKSPACE_EXT_API void _FlushTerminalInputBuffer(const winrt::Microsoft::Terminal::Control::TermControl& control, std::wstring_view inputOverride = {});
        WT_WORKSPACE_EXT_API std::wstring _ResolveTrackedTerminalWorkingDirectory(const winrt::Microsoft::Terminal::Control::TermControl& control) const;
        WT_WORKSPACE_EXT_API bool _ShouldUseTwoPhaseWorkspaceChatSubmit(const winrt::Microsoft::Terminal::Control::TermControl& control);
        WT_WORKSPACE_EXT_API void _ScheduleTerminalOutputCapture(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                                                 const TerminalRoutingContext& context,
                                                                 std::wstring correlationId);
        WT_WORKSPACE_EXT_API void _ProcessPendingTerminalOutputCaptures();
        WT_WORKSPACE_EXT_API void _ShowWorkspaceNameMenu();
        WT_WORKSPACE_EXT_API void _BeginWorkspaceNameEdit();
        WT_WORKSPACE_EXT_API void _CommitWorkspaceNameEdit();
        WT_WORKSPACE_EXT_API void _CancelWorkspaceNameEdit();
