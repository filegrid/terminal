// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    bool RecordWorkspaceMirrorEvent(WorkspaceMirrorNodeSession& session,
                                    std::wstring_view commandId,
                                    WorkspaceMirrorEvent event,
                                    size_t maximumEvents);
    std::vector<WorkspaceMirrorEvent> WorkspaceMirrorEventsAfter(const WorkspaceMirrorWindowState& window,
                                                                  uint64_t sequence);
    WorkspaceMirrorWindowState* FindWorkspaceMirrorWindow(WorkspaceMirrorNodeSession& session,
                                                           std::wstring_view commandId);
    const WorkspaceMirrorWindowState* FindWorkspaceMirrorWindow(const WorkspaceMirrorNodeSession& session,
                                                                 std::wstring_view commandId);
}
