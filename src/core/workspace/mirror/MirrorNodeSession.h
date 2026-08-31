// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    WorkspaceMirrorNodeSession CreateWorkspaceMirrorNodeSession(std::wstring workspaceId,
                                                                 std::wstring nodeId,
                                                                 std::wstring nodeSessionId);
    bool BeginWorkspaceMirrorWindow(WorkspaceMirrorNodeSession& session,
                                    std::wstring commandId,
                                    uint32_t rows,
                                    uint32_t columns,
                                    uint64_t timestampMilliseconds,
                                    size_t maximumEvents = 4096);
    bool RecordWorkspaceMirrorInput(WorkspaceMirrorNodeSession& session,
                                    std::wstring_view commandId,
                                    std::wstring text,
                                    WorkspaceMirrorInputOrigin origin,
                                    std::wstring clientId,
                                    uint64_t timestampMilliseconds,
                                    size_t maximumEvents = 4096);
    bool RecordWorkspaceMirrorOutput(WorkspaceMirrorNodeSession& session,
                                     std::wstring_view commandId,
                                     std::vector<uint8_t> bytes,
                                     uint64_t timestampMilliseconds,
                                     size_t maximumEvents = 4096);
    bool RecordWorkspaceMirrorResize(WorkspaceMirrorNodeSession& session,
                                     std::wstring_view commandId,
                                     uint32_t rows,
                                     uint32_t columns,
                                     uint64_t timestampMilliseconds,
                                     size_t maximumEvents = 4096);
    bool RecordWorkspaceMirrorTitle(WorkspaceMirrorNodeSession& session,
                                    std::wstring_view commandId,
                                    std::wstring title,
                                    uint64_t timestampMilliseconds,
                                    size_t maximumEvents = 4096);
    bool EndWorkspaceMirrorWindow(WorkspaceMirrorNodeSession& session,
                                  std::wstring_view commandId,
                                  uint64_t timestampMilliseconds,
                                  size_t maximumEvents = 4096);
}
