        Windows::UI::Xaml::Controls::MenuFlyout _workspaceFlyout{ nullptr };
        TerminalApp::Tab _workspaceManagerTab{ nullptr };
        TerminalApp::IPaneContent _workspaceManagerContent{ nullptr };
        Windows::UI::Xaml::DispatcherTimer _workspaceStateHeartbeatTimer{ nullptr };
        struct WorkspaceCommandTabGroup
        {
            // Only NodeTab is shown in the first-level tab row. CommandTabs
            // are real terminal tabs kept behind it and selected by the
            // in-content second-level command tab strip.
            TerminalApp::Tab NodeTab{ nullptr };
            std::vector<TerminalApp::Tab> CommandTabs;
            std::vector<winrt::Windows::UI::Xaml::Controls::Grid> CommandContentHosts;
            std::vector<winrt::Microsoft::UI::Xaml::Controls::TabViewItem> CommandTabItems;
            std::vector<winrt::hstring> Names;
            std::vector<winrt::hstring> Icons;
            int32_t Placement{};
            size_t ActiveIndex{};
            std::optional<size_t> RenderedActiveIndex;
        };
        // A tab-mode workspace node presents several real terminal pages as
        // one logical node tab with an in-content command switcher.
        std::vector<WorkspaceCommandTabGroup> _workspaceCommandTabGroups;
        std::optional<Microsoft::Terminal::Settings::Model::implementation::Workspace> _terminalContentWorkspace;
