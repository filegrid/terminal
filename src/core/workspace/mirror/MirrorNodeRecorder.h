// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorCheckpoint.h"
#include "MirrorControlLease.h"
#include "MirrorNodeSession.h"
#include "MirrorRecoveryPlanner.h"
#include "MirrorReducer.h"

#include <mutex>

namespace terminal::workspace
{
    // Thread-safe ownership boundary between a terminal connection's I/O
    // threads and the Core session state. Glue only forwards lifecycle and
    // byte events; it cannot mutate a session directly.
    class WorkspaceMirrorNodeRecorder
    {
    public:
        WorkspaceMirrorNodeRecorder(WorkspaceNodeMirrorConfiguration configuration,
                                    std::wstring workspaceId,
                                    std::wstring nodeId,
                                    std::wstring nodeSessionId);

        bool Enabled() const noexcept;
        bool BeginWindow(std::wstring commandId, uint32_t rows, uint32_t columns, uint64_t timestampMilliseconds);
        bool RecordInput(std::wstring_view commandId,
                         std::wstring text,
                         WorkspaceMirrorInputOrigin origin,
                         std::wstring clientId,
                         uint64_t timestampMilliseconds);
        bool RecordOutput(std::wstring_view commandId, std::vector<uint8_t> bytes, uint64_t timestampMilliseconds);
        bool RecordResize(std::wstring_view commandId, uint32_t rows, uint32_t columns, uint64_t timestampMilliseconds);
        bool RecordTitle(std::wstring_view commandId, std::wstring title, uint64_t timestampMilliseconds);
        bool AddCheckpoint(std::wstring_view commandId, WorkspaceMirrorTerminalState state);
        bool EndWindow(std::wstring_view commandId, uint64_t timestampMilliseconds);
        WorkspaceMirrorRecoveryPlan PlanRecovery(std::wstring_view commandId,
                                                  std::optional<uint64_t> resumeSequence) const;
        bool GrantControl(std::wstring clientId,
                          std::wstring leaseId,
                          uint64_t nowMilliseconds,
                          uint64_t leaseDurationMilliseconds);
        bool RevokeControl(std::wstring_view clientId = {});
        bool TryCreateRemoteInputEffect(std::wstring_view commandId,
                                        std::wstring_view clientId,
                                        std::wstring_view leaseId,
                                        std::wstring text,
                                        uint64_t nowMilliseconds,
                                        WorkspaceMirrorEffect& effect);
        void Close();

        WorkspaceMirrorNodeSession Snapshot() const;

    private:
        WorkspaceNodeMirrorConfiguration _configuration;
        mutable std::mutex _mutex;
        WorkspaceMirrorNodeSession _session;
    };
}
