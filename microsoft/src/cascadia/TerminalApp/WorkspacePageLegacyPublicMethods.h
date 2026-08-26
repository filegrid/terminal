        WT_WORKSPACE_EXT_API void SetStartupActions(std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> actions, const winrt::hstring& workspaceId = {});
        WT_WORKSPACE_EXT_API void CurrentWorkspaceId(const winrt::hstring& value);
        WT_WORKSPACE_EXT_API winrt::hstring CurrentWorkspaceId() const noexcept;
        WT_WORKSPACE_EXT_API void RefreshWorkspaceWindowState();
        WT_WORKSPACE_EXT_API void WorkspaceDefinitionsChanged();
