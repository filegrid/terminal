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
        dialog.Title(box_value(deletingNode ? L"删除节点" : L"删除工作区"));
        dialog.Content(box_value(deletingNode ? L"确定要删除这个节点吗？" : L"确定要删除这个工作区吗？"));
        dialog.PrimaryButtonText(L"删除");
        dialog.CloseButtonText(L"取消");
        co_return co_await host.ShowWorkspaceDialog(dialog) == ContentDialogResult::Primary;
    }
}
