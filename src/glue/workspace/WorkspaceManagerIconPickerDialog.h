// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "../contracts/GluePageHostContract.h"

namespace terminal::workspace
{
    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerIcon(
        TerminalPageBase& host,
        std::wstring initialIcon,
        std::optional<size_t> nodeIndex);
}
