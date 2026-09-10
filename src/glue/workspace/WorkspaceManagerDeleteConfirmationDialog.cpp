// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "WorkspaceManagerDeleteConfirmationDialog.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml::Controls;

namespace terminal::workspace
{
    IAsyncOperation<bool> ConfirmWorkspaceManagerDeletion(TerminalPageBase& host, const bool deletingNode)
    {
        auto dialog = ContentDialog{};
        dialog.Title(box_value(deletingNode ? RS_(L"WorkspaceUi_DeleteNode") : RS_(L"WorkspaceUi_DeleteWorkspace")));
        dialog.Content(box_value(deletingNode ? RS_(L"WorkspaceUi_DeleteNodeConfirmation") : RS_(L"WorkspaceUi_DeleteWorkspaceConfirmation")));
        dialog.PrimaryButtonText(RS_(L"WorkspaceUi_Delete"));
        dialog.CloseButtonText(RS_(L"WorkspaceUi_Cancel"));
        co_return co_await host.ShowWorkspaceDialog(dialog) == ContentDialogResult::Primary;
    }
}
