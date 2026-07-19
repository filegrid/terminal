#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>

namespace terminal::workspacepaths
{
    inline constexpr std::wstring_view LegacyWorkspaceFileName{ L"workspaces.yaml" };
    inline constexpr std::wstring_view WorkspaceMetadataFileName{ L"workspace.yaml" };
    inline constexpr std::wstring_view WorkspaceNodeMetadataFileName{ L"tab.yaml" };
    inline constexpr std::wstring_view WorkspaceStateFileName{ L"state.yaml" };
    inline constexpr std::wstring_view LegacyWorkspaceStateFilePrefix{ L"workspace-window-state" };
    inline constexpr std::wstring_view WorkspaceDefinitionsDirectoryName{ L"workspaces" };

    inline std::wstring _getEnvironmentVariable(const wchar_t* name)
    {
        wchar_t* value = nullptr;
        size_t length = 0;
        if (_wdupenv_s(&value, &length, name) != 0 || !value || length == 0)
        {
            return {};
        }

        std::wstring result{ value };
        free(value);
        return result;
    }

    inline std::wstring _trimWhitespace(std::wstring_view value)
    {
        size_t start = 0;
        while (start < value.size() && iswspace(value[start]))
        {
            ++start;
        }

        size_t end = value.size();
        while (end > start && iswspace(value[end - 1]))
        {
            --end;
        }

        return std::wstring{ value.substr(start, end - start) };
    }

    inline std::wstring _toLower(std::wstring_view value)
    {
        std::wstring lowered;
        lowered.reserve(value.size());
        std::transform(value.begin(), value.end(), std::back_inserter(lowered), [](const wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        return lowered;
    }

    inline std::filesystem::path ResolveWorkspaceRootPath()
    {
        const auto userProfile = _getEnvironmentVariable(L"USERPROFILE");
        if (!userProfile.empty())
        {
            return std::filesystem::path{ userProfile } / L".wt";
        }
        return std::filesystem::current_path() / L".wt";
    }

    inline std::filesystem::path ResolveWorkspaceDefinitionsPath()
    {
        const auto root = ResolveWorkspaceRootPath();
        return root.empty() ? std::filesystem::path{} : root / std::filesystem::path{ WorkspaceDefinitionsDirectoryName };
    }

    inline std::filesystem::path ResolveLegacyWorkspaceDefinitionsPath()
    {
        return ResolveWorkspaceRootPath();
    }

    inline bool IsInvalidWorkspacePathCharacter(const wchar_t ch) noexcept
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
            return true;
        default:
            return ch < 32;
        }
    }

    inline bool IsReservedWorkspaceDirectoryName(std::wstring_view value) noexcept
    {
        const auto dot = value.find(L'.');
        const auto stem = _toLower(dot == std::wstring_view::npos ? value : value.substr(0, dot));
        static constexpr std::array<std::wstring_view, 22> reservedNames{
            L"con",
            L"prn",
            L"aux",
            L"nul",
            L"com1",
            L"com2",
            L"com3",
            L"com4",
            L"com5",
            L"com6",
            L"com7",
            L"com8",
            L"com9",
            L"lpt1",
            L"lpt2",
            L"lpt3",
            L"lpt4",
            L"lpt5",
            L"lpt6",
            L"lpt7",
            L"lpt8",
            L"lpt9",
        };
        return std::ranges::find(reservedNames, stem) != reservedNames.end();
    }

    inline std::wstring SanitizeWorkspaceDirectoryName(std::wstring_view value, std::wstring_view fallback)
    {
        std::wstring sanitized;
        sanitized.reserve(value.size());
        for (const auto ch : value)
        {
            sanitized.push_back(IsInvalidWorkspacePathCharacter(ch) ? L'_' : ch);
        }

        sanitized = _trimWhitespace(sanitized);
        while (!sanitized.empty() && (sanitized.back() == L'.' || sanitized.back() == L' '))
        {
            sanitized.pop_back();
        }

        while (!sanitized.empty() && sanitized.front() == L' ')
        {
            sanitized.erase(sanitized.begin());
        }

        if (sanitized.empty())
        {
            sanitized.assign(fallback);
        }

        if (sanitized.empty())
        {
            sanitized.assign(L"_");
        }

        if (IsReservedWorkspaceDirectoryName(sanitized))
        {
            sanitized.push_back(L'_');
        }

        return sanitized;
    }
}
