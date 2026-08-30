// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "WorkspaceChatTextHelpers.h"

#include <cwctype>

namespace terminal::workspacechat
{
    namespace
    {
        std::wstring _trim(std::wstring_view value)
        {
            const auto first = value.find_first_not_of(L" \t\r\n");
            if (first == std::wstring_view::npos)
            {
                return {};
            }

            const auto last = value.find_last_not_of(L" \t\r\n");
            return std::wstring{ value.substr(first, last - first + 1) };
        }

        std::wstring _toLower(std::wstring_view value)
        {
            std::wstring lowered;
            lowered.reserve(value.size());
            for (const auto ch : value)
            {
                lowered.push_back(static_cast<wchar_t>(std::towlower(ch)));
            }
            return lowered;
        }

        bool _containsInsensitive(std::wstring_view haystack, std::wstring_view needle)
        {
            if (needle.empty())
            {
                return true;
            }

            const auto haystackLower = _toLower(haystack);
            const auto needleLower = _toLower(needle);
            return haystackLower.find(needleLower) != std::wstring::npos;
        }

        bool _isLineBreakCharacter(const wchar_t ch) noexcept
        {
            return ch == L'\r' || ch == L'\n';
        }
    }

    size_t CountTrailingLineBreaks(std::wstring_view value) noexcept
    {
        size_t count = 0;
        auto index = value.size();
        while (index > 0)
        {
            const auto ch = value[index - 1];
            if (ch == L'\n')
            {
                --index;
                if (index > 0 && value[index - 1] == L'\r')
                {
                    --index;
                }
                ++count;
            }
            else if (ch == L'\r')
            {
                --index;
                ++count;
            }
            else
            {
                break;
            }
        }
        return count;
    }

    std::wstring TrimTrailingLineBreaksOnly(std::wstring_view text)
    {
        auto end = text.size();
        while (end > 0 && _isLineBreakCharacter(text[end - 1]))
        {
            --end;
        }
        return std::wstring{ text.substr(0, end) };
    }

    std::wstring TrimWorkspaceChatText(std::wstring_view text)
    {
        return _trim(text);
    }

    std::wstring NormalizeTerminalInput(std::wstring_view text)
    {
        std::wstring normalized;
        normalized.reserve(text.size());
        for (const auto ch : text)
        {
            if (ch != L'\r')
            {
                normalized.push_back(ch);
            }
        }
        return TrimWorkspaceChatText(normalized);
    }

    std::wstring NormalizeWorkspaceChatSubmitPreviewText(const std::wstring_view text)
    {
        std::wstring normalized;
        normalized.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i)
        {
            const auto ch = text[i];
            if (ch == L'\r')
            {
                if (i + 1 < text.size() && text[i + 1] == L'\n')
                {
                    ++i;
                }
                normalized.append(L"<br>");
            }
            else if (ch == L'\n')
            {
                normalized.append(L"<br>");
            }
            else
            {
                normalized.push_back(ch);
            }
        }
        return normalized;
    }

    bool ContainsLineBreak(std::wstring_view text) noexcept
    {
        return text.find(L'\n') != std::wstring_view::npos ||
               text.find(L'\r') != std::wstring_view::npos;
    }

    std::wstring SummarizeTerminalOutput(std::wstring_view currentBuffer, std::wstring_view previousBuffer)
    {
        if (currentBuffer.empty())
        {
            return {};
        }

        std::wstring_view delta = currentBuffer;
        if (!previousBuffer.empty() && currentBuffer.starts_with(previousBuffer))
        {
            delta = currentBuffer.substr(previousBuffer.size());
        }

        auto summary = TrimWorkspaceChatText(delta);
        if (summary.empty())
        {
            summary = TrimWorkspaceChatText(currentBuffer);
        }

        constexpr size_t maxSummaryLength = 4000;
        if (summary.size() > maxSummaryLength)
        {
            summary = summary.substr(summary.size() - maxSummaryLength);
        }
        return summary;
    }

    bool IsInteractiveCliCommand(std::wstring_view candidate)
    {
        const auto trimmed = _trim(candidate);
        if (trimmed.empty())
        {
            return false;
        }

        return _containsInsensitive(trimmed, L"copilot") ||
               _containsInsensitive(trimmed, L"claude");
    }
}
