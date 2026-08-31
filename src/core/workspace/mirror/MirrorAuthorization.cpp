// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorAuthorization.h"

    bool IsWorkspaceMirrorCapabilityValid(const WorkspaceMirrorCapability& capability,
                                          const WorkspaceMirrorNodeSession& session,
                                          const std::wstring_view expectedDeviceId,
                                          const WorkspaceMirrorPermission requiredPermission,
                                          const uint64_t nowMilliseconds,
                                          const uint64_t currentPolicyVersion)
    {
        if (session.Closed || capability.UserId.empty() || capability.DeviceId != expectedDeviceId ||
            capability.NodeSessionId != session.NodeSessionId || capability.ExpiresAtMilliseconds <= nowMilliseconds ||
            capability.PolicyVersion != currentPolicyVersion)
        {
            return false;
        }
        return capability.Permission == WorkspaceMirrorPermission::Control || requiredPermission == WorkspaceMirrorPermission::View;
    }
