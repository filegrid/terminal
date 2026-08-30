// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "WorkspaceStoragePaths.h"

#include "../workspace/WorkspacePersistencePaths.h"

namespace terminal::workspacechat
{
    namespace
    {
        constexpr std::wstring_view _cacheDirectoryName{ L"cache" };
        constexpr std::wstring_view _logsDirectoryName{ L"logs" };

        bool _isUnsavedWorkspaceKey(const std::wstring_view value) noexcept
        {
            return value.starts_with(L"__unsaved-window");
        }
    }

    std::filesystem::path ResolveWorkspaceArtifactDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey)
    {
        const auto root = terminal::workspacepaths::ResolveWorkspaceRootPath();
        if (root.empty())
        {
            return {};
        }

        const auto sanitizedWorkspaceKey = terminal::workspacepaths::SanitizeWorkspaceDirectoryName(workspaceKey, L"_");
        const auto sanitizedTabKey = terminal::workspacepaths::SanitizeWorkspaceDirectoryName(tabKey, L"_");
        const auto workspaceDirectory = _isUnsavedWorkspaceKey(workspaceKey) ?
                                            root / std::filesystem::path{ _cacheDirectoryName } / sanitizedWorkspaceKey :
                                            root / std::filesystem::path{ terminal::workspacepaths::WorkspaceDefinitionsDirectoryName } / sanitizedWorkspaceKey;
        return workspaceDirectory / std::filesystem::path{ sanitizedTabKey };
    }

    std::filesystem::path ResolveWorkspaceLogsDirectory()
    {
        const auto root = terminal::workspacepaths::ResolveWorkspaceRootPath();
        return root.empty() ? std::filesystem::path{} : root / std::filesystem::path{ _logsDirectoryName };
    }
}
