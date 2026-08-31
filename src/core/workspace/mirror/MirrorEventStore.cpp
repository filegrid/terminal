// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorEventStore.h"

#include <algorithm>

    WorkspaceMirrorWindowState* FindWorkspaceMirrorWindow(WorkspaceMirrorNodeSession& session,
                                                           const std::wstring_view commandId)
    {
        const auto it = std::find_if(session.Windows.begin(), session.Windows.end(), [&](const auto& window) {
            return window.CommandId == commandId;
        });
        return it == session.Windows.end() ? nullptr : &*it;
    }

    const WorkspaceMirrorWindowState* FindWorkspaceMirrorWindow(const WorkspaceMirrorNodeSession& session,
                                                                 const std::wstring_view commandId)
    {
        const auto it = std::find_if(session.Windows.cbegin(), session.Windows.cend(), [&](const auto& window) {
            return window.CommandId == commandId;
        });
        return it == session.Windows.cend() ? nullptr : &*it;
    }

    bool RecordWorkspaceMirrorEvent(WorkspaceMirrorNodeSession& session,
                                    const std::wstring_view commandId,
                                    WorkspaceMirrorEvent event,
                                    const size_t maximumEvents)
    {
        auto* window = FindWorkspaceMirrorWindow(session, commandId);
        if (!window || session.Closed || maximumEvents == 0)
        {
            return false;
        }

        event.Sequence = ++session.HeadSequence;
        window->HeadSequence = event.Sequence;
        if (event.Kind == WorkspaceMirrorEventKind::Gap)
        {
            window->HasGap = true;
        }
        window->Events.emplace_back(std::move(event));
        if (window->Events.size() > maximumEvents)
        {
            window->Events.erase(window->Events.begin());
            // The retained stream no longer starts at a known replay base.
            // A later atomic checkpoint is the only safe way to clear this.
            window->HasGap = true;
        }
        return true;
    }

    std::vector<WorkspaceMirrorEvent> WorkspaceMirrorEventsAfter(const WorkspaceMirrorWindowState& window,
                                                                  const uint64_t sequence)
    {
        std::vector<WorkspaceMirrorEvent> result;
        const auto first = std::find_if(window.Events.cbegin(), window.Events.cend(), [&](const auto& event) {
            return event.Sequence > sequence;
        });
        result.insert(result.end(), first, window.Events.cend());
        return result;
    }
