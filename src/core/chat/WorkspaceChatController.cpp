// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "WorkspaceChatController.h"

#include <atomic>
#include <chrono>
#include <cstdio>

namespace terminal::workspacechat
{
    namespace
    {
        std::wstring _timestampNow()
        {
            using namespace std::chrono;

            const auto now = system_clock::now();
            const auto millisecondsPart = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            const auto time = system_clock::to_time_t(now);

            std::tm localTime{};
            localtime_s(&localTime, &time);

            wchar_t timestamp[32]{};
            std::swprintf(timestamp,
                          sizeof(timestamp) / sizeof(timestamp[0]),
                          L"%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                          localTime.tm_year + 1900,
                          localTime.tm_mon + 1,
                          localTime.tm_mday,
                          localTime.tm_hour,
                          localTime.tm_min,
                          localTime.tm_sec,
                          static_cast<int>(millisecondsPart.count()));
            return timestamp;
        }

        std::wstring _nextId(std::wstring_view prefix)
        {
            static std::atomic_uint64_t sequence{ 0 };
            const auto counter = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
            const auto ticks = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());

            wchar_t value[64]{};
            std::swprintf(value,
                          sizeof(value) / sizeof(value[0]),
                          L"%.*ls-%016llX-%04llX",
                          static_cast<int>(prefix.size()),
                          prefix.data(),
                          ticks,
                          static_cast<unsigned long long>(counter & 0xFFFF));
            return value;
        }
    }

    WorkspaceChatSnapshot WorkspaceChatController::LoadWorkspace(std::wstring_view workspaceKey, std::wstring_view tabKey, const size_t maxMessages) const
    {
        return _chatStore.LoadSnapshot(workspaceKey, tabKey, maxMessages);
    }

    std::wstring WorkspaceChatController::LoadDraft(std::wstring_view workspaceKey, std::wstring_view tabKey) const
    {
        return _chatStore.LoadDraft(workspaceKey, tabKey);
    }

    bool WorkspaceChatController::SaveDraft(std::wstring_view workspaceKey, std::wstring_view tabKey, std::wstring_view draft) const
    {
        return _chatStore.SaveDraft(workspaceKey, tabKey, draft);
    }

    void WorkspaceChatController::SetSubmitDiagnosticHook(const WorkspaceChatSubmitDiagnosticHook hook) noexcept
    {
        _submitDiagnosticHook = hook;
    }

    ChatMessageEntry WorkspaceChatController::SubmitUserMessage(std::wstring_view workspaceKey,
                                                                std::wstring_view tabKey,
                                                                const uint64_t windowId,
                                                                std::wstring_view tabId,
                                                                std::wstring_view paneId,
                                                                std::wstring_view text)
    {
        ChatMessageEntry entry;
        entry.Timestamp = _timestampNow();
        entry.WorkspaceId = std::wstring{ workspaceKey };
        entry.WindowId = windowId;
        entry.MessageId = _nextId(L"chat");
        entry.Role = L"user";
        entry.Text = std::wstring{ text };
        entry.CorrelationId = _nextId(L"corr");
        entry.TabId = std::wstring{ tabId };
        entry.PaneId = std::wstring{ paneId };

        if (_submitDiagnosticHook)
        {
            _submitDiagnosticHook(entry);
        }

        _chatStore.AppendMessage(entry, tabKey);
        _pendingCorrelationId = entry.CorrelationId;
        _chatStore.SaveDraft(workspaceKey, tabKey, L"");
        return entry;
    }

    bool WorkspaceChatController::LogTerminalInput(std::wstring_view workspaceKey,
                                                   std::wstring_view tabKey,
                                                   std::wstring_view tabId,
                                                   std::wstring_view paneId,
                                                   std::wstring_view text,
                                                   std::wstring_view workingDirectory,
                                                   std::wstring_view command,
                                                   std::wstring correlationId)
    {
        TerminalEventEntry entry;
        entry.Timestamp = _timestampNow();
        entry.WorkspaceId = std::wstring{ workspaceKey };
        entry.TabId = std::wstring{ tabId };
        entry.PaneId = std::wstring{ paneId };
        entry.EventId = _nextId(L"term");
        entry.Kind = L"input";
        entry.Text = std::wstring{ text };
        entry.WorkingDirectory = std::wstring{ workingDirectory };
        entry.Command = std::wstring{ command };
        entry.CorrelationId = std::move(correlationId);
        return _terminalStore.AppendEvent(entry, tabKey);
    }

    bool WorkspaceChatController::LogTerminalOutput(std::wstring_view workspaceKey,
                                                    std::wstring_view tabKey,
                                                    std::wstring_view tabId,
                                                    std::wstring_view paneId,
                                                    std::wstring_view text,
                                                    std::wstring_view workingDirectory,
                                                    std::wstring_view command,
                                                    std::wstring correlationId)
    {
        TerminalEventEntry entry;
        entry.Timestamp = _timestampNow();
        entry.WorkspaceId = std::wstring{ workspaceKey };
        entry.TabId = std::wstring{ tabId };
        entry.PaneId = std::wstring{ paneId };
        entry.EventId = _nextId(L"term");
        entry.Kind = L"output";
        entry.Text = std::wstring{ text };
        entry.WorkingDirectory = std::wstring{ workingDirectory };
        entry.Command = std::wstring{ command };
        entry.CorrelationId = std::move(correlationId);
        return _terminalStore.AppendEvent(entry, tabKey);
    }

    std::wstring WorkspaceChatController::ConsumePendingCorrelationId()
    {
        auto pending = std::move(_pendingCorrelationId);
        _pendingCorrelationId.clear();
        return pending;
    }
}
