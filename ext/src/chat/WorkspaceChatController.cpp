#include "pch.h"
#include "WorkspaceChatController.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <atomic>
#include <chrono>

namespace terminal::workspacechat
{
    namespace
    {
        std::wstring _timestampNow()
        {
            SYSTEMTIME localTime{};
            GetLocalTime(&localTime);

            TIME_ZONE_INFORMATION timeZoneInfo{};
            const auto timeZoneState = GetTimeZoneInformation(&timeZoneInfo);
            long biasMinutes = timeZoneInfo.Bias;
            if (timeZoneState == TIME_ZONE_ID_DAYLIGHT)
            {
                biasMinutes += timeZoneInfo.DaylightBias;
            }
            else if (timeZoneState == TIME_ZONE_ID_STANDARD)
            {
                biasMinutes += timeZoneInfo.StandardBias;
            }

            const auto utcOffsetMinutes = -biasMinutes;
            const auto offsetHours = utcOffsetMinutes / 60;
            const auto offsetMinutes = std::abs(utcOffsetMinutes % 60);

            return fmt::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}{:+03}:{:02}",
                               localTime.wYear,
                               localTime.wMonth,
                               localTime.wDay,
                               localTime.wHour,
                               localTime.wMinute,
                               localTime.wSecond,
                               localTime.wMilliseconds,
                               offsetHours,
                               offsetMinutes);
        }

        std::wstring _nextId(std::wstring_view prefix)
        {
            static std::atomic_uint64_t sequence{ 0 };
            const auto counter = sequence.fetch_add(1, std::memory_order_relaxed) + 1;

            FILETIME fileTime{};
            GetSystemTimePreciseAsFileTime(&fileTime);

            ULARGE_INTEGER timestamp{};
            timestamp.LowPart = fileTime.dwLowDateTime;
            timestamp.HighPart = fileTime.dwHighDateTime;

            return fmt::format(L"{}-{:016X}-{:04X}", prefix, timestamp.QuadPart, counter & 0xFFFF);
        }
    }

    WorkspaceChatSnapshot WorkspaceChatController::LoadWorkspace(std::wstring_view workspaceKey, const size_t maxMessages) const
    {
        return _chatStore.LoadSnapshot(workspaceKey, maxMessages);
    }

    std::wstring WorkspaceChatController::LoadDraft(std::wstring_view workspaceKey) const
    {
        return _chatStore.LoadDraft(workspaceKey);
    }

    bool WorkspaceChatController::SaveDraft(std::wstring_view workspaceKey, std::wstring_view draft) const
    {
        return _chatStore.SaveDraft(workspaceKey, draft);
    }

    ChatMessageEntry WorkspaceChatController::SubmitUserMessage(std::wstring_view workspaceKey,
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

        _chatStore.AppendMessage(entry);
        _pendingCorrelationId = entry.CorrelationId;
        _chatStore.SaveDraft(workspaceKey, L"");
        return entry;
    }

    bool WorkspaceChatController::LogTerminalInput(std::wstring_view workspaceKey,
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
        return _terminalStore.AppendEvent(entry);
    }

    bool WorkspaceChatController::LogTerminalOutput(std::wstring_view workspaceKey,
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
        return _terminalStore.AppendEvent(entry);
    }

    std::wstring WorkspaceChatController::ConsumePendingCorrelationId()
    {
        auto pending = std::move(_pendingCorrelationId);
        _pendingCorrelationId.clear();
        return pending;
    }
}
