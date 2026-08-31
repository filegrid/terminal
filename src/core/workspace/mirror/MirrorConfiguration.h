// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    bool ApplyWorkspaceMirrorConfigurationField(WorkspaceNodeMirrorConfiguration& configuration,
                                                std::wstring_view key,
                                                std::wstring_view value);
    std::wstring WorkspaceMirrorModeToString(WorkspaceNodeMirrorMode mode);
}
