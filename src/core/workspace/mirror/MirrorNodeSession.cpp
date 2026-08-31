// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorNodeSession.h"

#include "MirrorEventStore.h"

#include <algorithm>
#include <utility>

    WorkspaceMirrorNodeSession CreateWorkspaceMirrorNodeSession(std::wstring workspaceId,
                                                                 std::wstring nodeId,
                                                                 std::wstring nodeSessionId)
    {
        return { std::move(workspaceId), std::move(nodeId), std::move(nodeSessionId) };
    }

    bool BeginWorkspaceMirrorWindow(WorkspaceMirrorNodeSession& session,
                                    std::wstring commandId,
                                    const uint32_t rows,
                                    const uint32_t columns,
                                    const uint64_t timestampMilliseconds,
                                    const size_t maximumEvents)
    {
        if (session.Closed || commandId.empty() || rows == 0 || columns == 0 || FindWorkspaceMirrorWindow(session, commandId))
        {
            return false;
        }
        session.Windows.push_back({ .CommandId = std::move(commandId), .Started = true, .Rows = rows, .Columns = columns });
        return RecordWorkspaceMirrorEvent(session,
                                          session.Windows.back().CommandId,
                                          { .TimestampMilliseconds = timestampMilliseconds, .Kind = WorkspaceMirrorEventKind::Started, .Rows = rows, .Columns = columns },
                                          maximumEvents);
    }

    bool RecordWorkspaceMirrorInput(WorkspaceMirrorNodeSession& session,
                                    const std::wstring_view commandId,
                                    std::wstring text,
                                    const WorkspaceMirrorInputOrigin origin,
                                    std::wstring clientId,
                                    const uint64_t timestampMilliseconds,
                                    const size_t maximumEvents)
    {
        return !text.empty() && RecordWorkspaceMirrorEvent(session, commandId, { .TimestampMilliseconds = timestampMilliseconds, .Kind = WorkspaceMirrorEventKind::Input, .Origin = origin, .ClientId = std::move(clientId), .Text = std::move(text) }, maximumEvents);
    }

    bool RecordWorkspaceMirrorOutput(WorkspaceMirrorNodeSession& session,
                                     const std::wstring_view commandId,
                                     std::vector<uint8_t> bytes,
                                     const uint64_t timestampMilliseconds,
                                     const size_t maximumEvents)
    {
        if (bytes.empty())
        {
            return false;
        }

        for (size_t offset{}; offset < bytes.size(); offset += WorkspaceMirrorMaximumOutputChunkBytes)
        {
            const auto count = std::min(WorkspaceMirrorMaximumOutputChunkBytes, bytes.size() - offset);
            std::vector<uint8_t> chunk;
            chunk.insert(chunk.end(), bytes.begin() + offset, bytes.begin() + offset + count);
            if (!RecordWorkspaceMirrorEvent(session,
                                             commandId,
                                             { .TimestampMilliseconds = timestampMilliseconds, .Kind = WorkspaceMirrorEventKind::Output, .Bytes = std::move(chunk) },
                                             maximumEvents))
            {
                return false;
            }
        }
        return true;
    }

    bool RecordWorkspaceMirrorResize(WorkspaceMirrorNodeSession& session,
                                     const std::wstring_view commandId,
                                     const uint32_t rows,
                                     const uint32_t columns,
                                     const uint64_t timestampMilliseconds,
                                     const size_t maximumEvents)
    {
        auto* window = FindWorkspaceMirrorWindow(session, commandId);
        if (!window || rows == 0 || columns == 0)
        {
            return false;
        }
        window->Rows = rows;
        window->Columns = columns;
        return RecordWorkspaceMirrorEvent(session, commandId, { .TimestampMilliseconds = timestampMilliseconds, .Kind = WorkspaceMirrorEventKind::Resize, .Rows = rows, .Columns = columns }, maximumEvents);
    }

    bool RecordWorkspaceMirrorTitle(WorkspaceMirrorNodeSession& session,
                                    const std::wstring_view commandId,
                                    std::wstring title,
                                    const uint64_t timestampMilliseconds,
                                    const size_t maximumEvents)
    {
        auto* window = FindWorkspaceMirrorWindow(session, commandId);
        if (!window)
        {
            return false;
        }
        window->Title = title;
        return RecordWorkspaceMirrorEvent(session, commandId, { .TimestampMilliseconds = timestampMilliseconds, .Kind = WorkspaceMirrorEventKind::Title, .Text = std::move(title) }, maximumEvents);
    }

    bool EndWorkspaceMirrorWindow(WorkspaceMirrorNodeSession& session,
                                  const std::wstring_view commandId,
                                  const uint64_t timestampMilliseconds,
                                  const size_t maximumEvents)
    {
        auto* window = FindWorkspaceMirrorWindow(session, commandId);
        if (!window || window->Exited)
        {
            return false;
        }
        window->Exited = true;
        return RecordWorkspaceMirrorEvent(session, commandId, { .TimestampMilliseconds = timestampMilliseconds, .Kind = WorkspaceMirrorEventKind::Exited }, maximumEvents);
    }
