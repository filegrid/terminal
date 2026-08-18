// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceIconPickerDialog.h"
#include "WorkspaceIconPickerDialog.g.cpp"

using namespace winrt;

namespace winrt::TerminalApp::implementation
{
    WorkspaceIconPickerDialog::WorkspaceIconPickerDialog()
    {
        InitializeComponent();
    }

    hstring WorkspaceIconPickerDialog::DialogTitle()
    {
        return _dialogTitle;
    }

    void WorkspaceIconPickerDialog::DialogTitle(const hstring& value)
    {
        _dialogTitle = value;
        if (const auto titleBlock = FindName(L"DialogTitleBlock").try_as<Windows::UI::Xaml::Controls::TextBlock>())
        {
            titleBlock.Text(value);
        }
    }

    Windows::UI::Xaml::UIElement WorkspaceIconPickerDialog::DialogBody()
    {
        return _dialogBody;
    }

    void WorkspaceIconPickerDialog::DialogBody(const Windows::UI::Xaml::UIElement& value)
    {
        _dialogBody = value;
        if (const auto bodyHost = FindName(L"DialogBodyHost").try_as<Windows::UI::Xaml::Controls::ContentControl>())
        {
            bodyHost.Content(value);
        }
    }

    void WorkspaceIconPickerDialog::CloseButton_Click(const Windows::Foundation::IInspectable&,
                                                      const Windows::UI::Xaml::RoutedEventArgs&)
    {
        Hide();
    }
}
