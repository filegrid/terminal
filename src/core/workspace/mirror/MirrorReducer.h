// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    bool TryCreateWorkspaceMirrorInputEffect(WorkspaceMirrorNodeSession& session,
                                              std::wstring_view commandId,
                                              std::wstring_view clientId,
                                              std::wstring_view leaseId,
                                              std::wstring text,
                                              uint64_t nowMilliseconds,
                                              WorkspaceMirrorEffect& effect,
                                              size_t maximumEvents = 4096);
}
