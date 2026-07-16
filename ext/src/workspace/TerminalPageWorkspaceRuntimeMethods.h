        WT_WORKSPACE_EXT_API std::optional<Microsoft::Terminal::Settings::Model::NewTerminalArgs> _BuildWorkspaceNodeArgs(const winrt::com_ptr<Tab>& tab) const;
        WT_WORKSPACE_EXT_API const WorkspaceNodeRuntimeState* _FindWorkspaceNodeRuntimeState(const winrt::Microsoft::Terminal::Control::TermControl& control) const;
        WT_WORKSPACE_EXT_API const WorkspaceNodeRuntimeState* _FindWorkspaceNodeRuntimeState(const winrt::com_ptr<Tab>& tab) const;
        WT_WORKSPACE_EXT_API std::optional<Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode> _ResolveCurrentWorkspaceNode(const winrt::com_ptr<Tab>& tab) const;
        WT_WORKSPACE_EXT_API std::wstring _ResolveLiveCurrentWorkspaceNodeId(const winrt::com_ptr<Tab>& tab) const;
        WT_WORKSPACE_EXT_API winrt::com_ptr<Tab> _GetCurrentWorkspaceTabByNodeId(std::wstring_view nodeId) const;
        WT_WORKSPACE_EXT_API WorkspaceNodeRemoveResult _RemoveWorkspaceNodeById(std::wstring_view workspaceId, std::wstring_view nodeId);
        WT_WORKSPACE_EXT_API WorkspaceNodeRemoveResult _RemoveWorkspaceNodeTab(const winrt::TerminalApp::Tab& tab, std::wstring_view workspaceId, std::wstring_view nodeId);
        WT_WORKSPACE_EXT_API void _PreparePendingWorkspaceNodeStartupAction(const Microsoft::Terminal::Settings::Model::ActionAndArgs& action,
                                                                            const std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions,
                                                                            size_t index);
        WT_WORKSPACE_EXT_API safe_void_coroutine _ReplayPendingWorkspaceStartupInput(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                                                                     const winrt::Microsoft::Terminal::Control::ICoreState& coreState);
        WT_WORKSPACE_EXT_API void _RegisterWorkspaceNodeRuntimeStateIfNeeded(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                                                             const Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs);
        WT_WORKSPACE_EXT_API void _RegisterWorkspaceNodeRuntimeState(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                                                     const Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs);
        WT_WORKSPACE_EXT_API std::wstring _ResolveWorkspaceNodeStartupAction(const winrt::com_ptr<Tab>& tab,
                                                                             const Microsoft::Terminal::Settings::Model::NewTerminalArgs& terminalArgs) const;
        WT_WORKSPACE_EXT_API std::wstring _ResolveWorkspaceNodeStartingDirectory(const winrt::com_ptr<Tab>& tab,
                                                                                 const Microsoft::Terminal::Settings::Model::NewTerminalArgs& terminalArgs) const;
        WT_WORKSPACE_EXT_API std::wstring _ResolveWorkspaceNodeOperatingSystem(const winrt::com_ptr<Tab>& tab,
                                                                               const Microsoft::Terminal::Settings::Model::NewTerminalArgs& terminalArgs) const;
        WT_WORKSPACE_EXT_API std::wstring _ResolveWorkspaceNodeShellType(const winrt::com_ptr<Tab>& tab,
                                                                         const Microsoft::Terminal::Settings::Model::NewTerminalArgs& terminalArgs) const;
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceNodeLoadState(size_t nodeIndex);
        WT_WORKSPACE_EXT_API void _ApplyWorkspaceNodeTabColor(size_t nodeIndex);
