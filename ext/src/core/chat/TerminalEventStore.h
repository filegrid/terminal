// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <string>
#include <string_view>

namespace terminal::workspacechat
{
    struct TerminalEventEntry
    {
        std::wstring Timestamp;
        std::wstring WorkspaceId;
        std::wstring TabId;
        std::wstring PaneId;
        std::wstring EventId;
        std::wstring Kind;
        std::wstring Text;
        std::wstring CorrelationId;
        std::wstring WorkingDirectory;
        std::wstring Command;
    };

    class TerminalEventStore
    {
    public:
        bool AppendEvent(const TerminalEventEntry& entry, std::wstring_view tabKey) const;
    };
}
