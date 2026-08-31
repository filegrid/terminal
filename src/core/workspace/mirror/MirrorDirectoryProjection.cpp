// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorDirectoryProjection.h"

#include <algorithm>
#include <limits>

WorkspaceMirrorDirectoryProjection BuildWorkspaceMirrorDirectoryProjection(const WorkspaceMirrorNodeSession& session,
                                                                            std::wstring displayName,
                                                                            const uint64_t projectionVersion)
{
    WorkspaceMirrorDirectoryProjection result;
    result.WorkspaceId = session.WorkspaceId;
    result.NodeId = session.NodeId;
    result.NodeSessionId = session.NodeSessionId;
    result.DisplayName = std::move(displayName);
    result.Available = !session.Closed && !session.Windows.empty();
    result.WindowCount = gsl::narrow_cast<uint16_t>(std::min<size_t>(session.Windows.size(), std::numeric_limits<uint16_t>::max()));
    result.ProjectionVersion = projectionVersion;
    if (result.Available)
    {
        result.Capabilities = { L"view", L"control", L"checkpoint", L"multi-window" };
    }
    return result;
}
