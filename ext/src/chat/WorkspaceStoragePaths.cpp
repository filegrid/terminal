#include "pch.h"
#include "WorkspaceStoragePaths.h"

#include <shlobj.h>
#include <til/unicode.h>
#include <wil/resource.h>

namespace terminal::workspacechat
{
    namespace
    {
        constexpr std::wstring_view _workspacesDirectoryName{ L"workspaces" };
        constexpr std::wstring_view _cacheDirectoryName{ L"cache" };
        constexpr std::wstring_view _logsDirectoryName{ L"logs" };

        std::wstring _sanitizePathComponent(std::wstring_view value)
        {
            std::wstring sanitized;
            sanitized.reserve(value.size());
            for (const auto ch : value)
            {
                switch (ch)
                {
                case L'<':
                case L'>':
                case L':':
                case L'"':
                case L'/':
                case L'\\':
                case L'|':
                case L'?':
                case L'*':
                    sanitized.push_back(L'_');
                    break;
                default:
                    sanitized.push_back(ch);
                    break;
                }
            }

            if (sanitized.empty())
            {
                sanitized = L"_";
            }
            return sanitized;
        }

        bool _isUnsavedWorkspaceKey(const std::wstring_view value) noexcept
        {
            return value.starts_with(L"__unsaved-window");
        }

        std::filesystem::path _workspaceRoot()
        {
            if (const auto userProfile = wil::TryGetEnvironmentVariableW<std::wstring>(L"USERPROFILE"); !userProfile.empty())
            {
                return std::filesystem::path{ userProfile } / L".wt";
            }

            wil::unique_cotaskmem_string profileFolder;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &profileFolder)) && profileFolder)
            {
                return std::filesystem::path{ profileFolder.get() } / L".wt";
            }

            return {};
        }
    }

    std::filesystem::path ResolveWorkspaceArtifactDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey)
    {
        const auto root = _workspaceRoot();
        if (root.empty())
        {
            return {};
        }

        const auto sanitizedWorkspaceKey = _sanitizePathComponent(workspaceKey);
        const auto sanitizedTabKey = _sanitizePathComponent(tabKey);
        const auto workspaceDirectory = _isUnsavedWorkspaceKey(workspaceKey) ?
                                            root / std::filesystem::path{ _cacheDirectoryName } / sanitizedWorkspaceKey :
                                            root / std::filesystem::path{ _workspacesDirectoryName } / sanitizedWorkspaceKey;
        return workspaceDirectory / std::filesystem::path{ sanitizedTabKey };
    }

    std::filesystem::path ResolveWorkspaceLogsDirectory()
    {
        const auto root = _workspaceRoot();
        return root.empty() ? std::filesystem::path{} : root / std::filesystem::path{ _logsDirectoryName };
    }
}
