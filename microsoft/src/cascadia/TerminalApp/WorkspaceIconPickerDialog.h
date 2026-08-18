// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "WorkspaceIconPickerDialog.g.h"

namespace winrt::TerminalApp::implementation
{
    struct WorkspaceIconPickerDialog : WorkspaceIconPickerDialogT<WorkspaceIconPickerDialog>
    {
    public:
        WorkspaceIconPickerDialog();

        hstring DialogTitle();
        void DialogTitle(const hstring& value);

        Windows::UI::Xaml::UIElement DialogBody();
        void DialogBody(const Windows::UI::Xaml::UIElement& value);

        void CloseButton_Click(const Windows::Foundation::IInspectable& sender,
                               const Windows::UI::Xaml::RoutedEventArgs& args);

    private:
        friend struct WorkspaceIconPickerDialogT<WorkspaceIconPickerDialog>;

        hstring _dialogTitle;
        Windows::UI::Xaml::UIElement _dialogBody{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(WorkspaceIconPickerDialog);
}
