// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <filesystem>
#include <string_view>

namespace terminal::workspacechat
{
    std::filesystem::path ResolveWorkspaceArtifactDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey);
    std::filesystem::path ResolveWorkspaceLogsDirectory();
}
