#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace terminal::workspacechat
{
    struct TerminalInputState
    {
        std::wstring LastWorkingDirectory;
        std::wstring PreviousWorkingDirectory;
        std::vector<std::wstring> DirectoryStack;
        std::wstring LastCommand;
        std::wstring OperatingSystem;
        std::wstring ShellType;
        std::array<std::wstring, 26> LastWorkingDirectoryByDrive{};
    };

    struct TerminalInputSnapshot
    {
        std::wstring WorkingDirectory;
        std::wstring Command;
        std::wstring OperatingSystem;
        std::wstring ShellType;
    };

    std::vector<std::wstring> SplitTerminalInputLines(std::wstring_view text);
    TerminalInputSnapshot TrackTerminalInput(TerminalInputState& state,
                                             std::wstring_view line,
                                             std::wstring_view knownWorkingDirectory = {});
}
