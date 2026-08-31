// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    bool GrantWorkspaceMirrorControl(WorkspaceMirrorNodeSession& session,
                                     std::wstring clientId,
                                     std::wstring leaseId,
                                     uint64_t nowMilliseconds,
                                     uint64_t leaseDurationMilliseconds);
    bool RevokeWorkspaceMirrorControl(WorkspaceMirrorNodeSession& session,
                                      std::wstring_view clientId = {});
    bool HasWorkspaceMirrorControl(const WorkspaceMirrorNodeSession& session,
                                   std::wstring_view clientId,
                                   std::wstring_view leaseId,
                                   uint64_t nowMilliseconds);
}
