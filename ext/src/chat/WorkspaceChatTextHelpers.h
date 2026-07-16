#pragma once

#include <string>
#include <string_view>

namespace terminal::workspacechat
{
    size_t CountTrailingLineBreaks(std::wstring_view value) noexcept;
    std::wstring TrimTrailingLineBreaksOnly(std::wstring_view text);
    std::wstring TrimWorkspaceChatText(std::wstring_view text);
    std::wstring NormalizeTerminalInput(std::wstring_view text);
    bool ContainsLineBreak(std::wstring_view text) noexcept;
    std::wstring SummarizeTerminalOutput(std::wstring_view currentBuffer, std::wstring_view previousBuffer);
    bool IsInteractiveCliCommand(std::wstring_view candidate);
}
