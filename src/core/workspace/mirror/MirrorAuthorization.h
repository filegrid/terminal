// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    bool IsWorkspaceMirrorCapabilityValid(const WorkspaceMirrorCapability& capability,
                                          const WorkspaceMirrorNodeSession& session,
                                          std::wstring_view expectedDeviceId,
                                          WorkspaceMirrorPermission requiredPermission,
                                          uint64_t nowMilliseconds,
                                          uint64_t currentPolicyVersion);
}
