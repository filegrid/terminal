// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    bool AddWorkspaceMirrorCheckpoint(WorkspaceMirrorNodeSession& session,
                                      std::wstring_view commandId,
                                      WorkspaceMirrorTerminalState state,
                                      size_t maximumCheckpoints);
}
