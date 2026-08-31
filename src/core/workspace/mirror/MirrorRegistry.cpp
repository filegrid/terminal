// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorRegistry.h"

#include <algorithm>

    bool WorkspaceMirrorRegistry::Add(WorkspaceMirrorNodeSession session)
    {
        if (session.NodeSessionId.empty() || Find(session.NodeSessionId))
        {
            return false;
        }
        _sessions.emplace_back(std::move(session));
        return true;
    }

    WorkspaceMirrorNodeSession* WorkspaceMirrorRegistry::Find(const std::wstring_view nodeSessionId)
    {
        const auto it = std::find_if(_sessions.begin(), _sessions.end(), [&](const auto& session) {
            return session.NodeSessionId == nodeSessionId;
        });
        return it == _sessions.end() ? nullptr : &*it;
    }

    const WorkspaceMirrorNodeSession* WorkspaceMirrorRegistry::Find(const std::wstring_view nodeSessionId) const
    {
        const auto it = std::find_if(_sessions.cbegin(), _sessions.cend(), [&](const auto& session) {
            return session.NodeSessionId == nodeSessionId;
        });
        return it == _sessions.cend() ? nullptr : &*it;
    }

    bool WorkspaceMirrorRegistry::Remove(const std::wstring_view nodeSessionId)
    {
        const auto it = std::find_if(_sessions.begin(), _sessions.end(), [&](const auto& session) {
            return session.NodeSessionId == nodeSessionId;
        });
        if (it == _sessions.end())
        {
            return false;
        }
        _sessions.erase(it);
        return true;
    }
