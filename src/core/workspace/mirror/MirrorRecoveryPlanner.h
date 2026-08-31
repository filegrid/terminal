// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    WorkspaceMirrorRecoveryPlan PlanWorkspaceMirrorRecovery(const WorkspaceMirrorNodeSession& session,
                                                             std::wstring_view commandId,
                                                             std::optional<uint64_t> resumeSequence);
}
