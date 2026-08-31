// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorCheckpoint.h"

#include "MirrorEventStore.h"

    bool AddWorkspaceMirrorCheckpoint(WorkspaceMirrorNodeSession& session,
                                      const std::wstring_view commandId,
                                      WorkspaceMirrorTerminalState state,
                                      const size_t maximumCheckpoints)
    {
        auto* window = FindWorkspaceMirrorWindow(session, commandId);
        if (!window || session.Closed || maximumCheckpoints == 0 || state.Rows == 0 || state.Columns == 0 || state.Bytes.empty())
        {
            return false;
        }

        const auto sequence = window->HeadSequence;
        window->Checkpoints.push_back({ sequence, std::move(state) });
        if (window->Checkpoints.size() > maximumCheckpoints)
        {
            window->Checkpoints.erase(window->Checkpoints.begin());
        }
        window->HasGap = false;
        return true;
    }
