// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorReducer.h"

#include "MirrorControlLease.h"
#include "MirrorNodeSession.h"

    bool TryCreateWorkspaceMirrorInputEffect(WorkspaceMirrorNodeSession& session,
                                              const std::wstring_view commandId,
                                              const std::wstring_view clientId,
                                              const std::wstring_view leaseId,
                                              std::wstring text,
                                              const uint64_t nowMilliseconds,
                                              WorkspaceMirrorEffect& effect,
                                              const size_t maximumEvents)
    {
        if (text.empty() || !HasWorkspaceMirrorControl(session, clientId, leaseId, nowMilliseconds) ||
            !RecordWorkspaceMirrorInput(session,
                                        commandId,
                                        text,
                                        WorkspaceMirrorInputOrigin::Remote,
                                        std::wstring{ clientId },
                                        nowMilliseconds,
                                        maximumEvents))
        {
            return false;
        }
        effect = { WorkspaceMirrorEffect::Kind::WriteInput, std::wstring{ commandId }, std::wstring{ clientId }, std::move(text) };
        return true;
    }
