// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    // Transport-neutral metadata sent to terminal-server's directory endpoint.
    // It deliberately excludes commands, I/O, checkpoints, titles and tokens.
    struct WorkspaceMirrorDirectoryProjection
    {
        std::wstring WorkspaceId;
        std::wstring NodeId;
        std::wstring NodeSessionId;
        std::wstring DisplayName;
        bool Available{ false };
        uint16_t WindowCount{};
        std::vector<std::wstring> Capabilities;
        uint64_t ProjectionVersion{};
    };

    WorkspaceMirrorDirectoryProjection BuildWorkspaceMirrorDirectoryProjection(const WorkspaceMirrorNodeSession& session,
                                                                                std::wstring displayName,
                                                                                uint64_t projectionVersion);
}
