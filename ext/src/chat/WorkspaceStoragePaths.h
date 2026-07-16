#pragma once

#include <filesystem>
#include <string_view>

namespace terminal::workspacechat
{
    std::filesystem::path ResolveWorkspaceArtifactDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey);
    std::filesystem::path ResolveWorkspaceLogsDirectory();
}
