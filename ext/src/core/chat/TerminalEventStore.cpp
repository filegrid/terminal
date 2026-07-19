// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "TerminalEventStore.h"

#include "ChatStorageUtils.h"
#include "WorkspaceStoragePaths.h"

namespace terminal::workspacechat
{
    namespace
    {
        constexpr std::wstring_view _terminalDirectoryName{ L"terminal" };
    }

    bool TerminalEventStore::AppendEvent(const TerminalEventEntry& entry, std::wstring_view tabKey) const
    {
        const auto path = ResolveWorkspaceArtifactDirectory(entry.WorkspaceId, tabKey) / _terminalDirectoryName / (std::filesystem::path{ core::LocalDateStamp() + L".jsonl" });

        Json::Value value{ Json::objectValue };
        value["ts"] = core::ToUtf8(entry.Timestamp);
        value["workspaceId"] = core::ToUtf8(entry.WorkspaceId);
        value["tabId"] = core::ToUtf8(entry.TabId);
        value["paneId"] = core::ToUtf8(entry.PaneId);
        value["eventId"] = core::ToUtf8(entry.EventId);
        value["kind"] = core::ToUtf8(entry.Kind);
        value["text"] = core::ToUtf8(entry.Text);
        value["correlationId"] = core::ToUtf8(entry.CorrelationId);
        if (!entry.WorkingDirectory.empty())
        {
            value["cwd"] = core::ToUtf8(entry.WorkingDirectory);
        }
        if (!entry.Command.empty())
        {
            value["cmd"] = core::ToUtf8(entry.Command);
        }

        return core::AppendJsonLine(path, value);
    }
}
