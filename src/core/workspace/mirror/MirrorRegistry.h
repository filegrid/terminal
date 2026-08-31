// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

namespace terminal::workspace
{
    class WorkspaceMirrorRegistry
    {
    public:
        bool Add(WorkspaceMirrorNodeSession session);
        WorkspaceMirrorNodeSession* Find(std::wstring_view nodeSessionId);
        const WorkspaceMirrorNodeSession* Find(std::wstring_view nodeSessionId) const;
        bool Remove(std::wstring_view nodeSessionId);

    private:
        std::vector<WorkspaceMirrorNodeSession> _sessions;
    };
}
