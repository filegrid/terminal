        WT_WORKSPACE_EXT_API winrt::Windows::Foundation::IAsyncOperation<bool> _ConfirmSaveWorkspaceOnExit();
        WT_WORKSPACE_EXT_API winrt::Windows::Foundation::IAsyncOperation<bool> _ConfirmWorkspaceCloseWindowIfNeeded();
        WT_WORKSPACE_EXT_API void _WorkspaceManagerPrimaryButtonClick(const IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& eventArgs);
        WT_WORKSPACE_EXT_API void _WorkspaceSaverActionClick(const IInspectable& sender, const IInspectable& eventArgs);
        WT_WORKSPACE_EXT_API void _WorkspaceSaverKeyDown(const IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        WT_WORKSPACE_EXT_API void _WorkspaceSaverKeyUp(const IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
