// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "WorkspaceManagerPathPicker.h"

namespace terminal::workspace
{
    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerPath(
        TerminalPageBase& host,
        const bool pickFolder)
    {
        co_return co_await host.PickWorkspacePath(pickFolder);
    }
}
