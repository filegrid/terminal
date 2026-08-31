// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorNodeRecorder.h"

WorkspaceMirrorNodeRecorder::WorkspaceMirrorNodeRecorder(const WorkspaceNodeMirrorConfiguration configuration,
                                                         std::wstring workspaceId,
                                                         std::wstring nodeId,
                                                         std::wstring nodeSessionId) :
    _configuration{ configuration },
    _session{ CreateWorkspaceMirrorNodeSession(std::move(workspaceId), std::move(nodeId), std::move(nodeSessionId)) }
{
}

bool WorkspaceMirrorNodeRecorder::Enabled() const noexcept
{
    return _configuration.Mode == WorkspaceNodeMirrorMode::NodeSession;
}

bool WorkspaceMirrorNodeRecorder::BeginWindow(std::wstring commandId, const uint32_t rows, const uint32_t columns, const uint64_t timestampMilliseconds)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && BeginWorkspaceMirrorWindow(_session, std::move(commandId), rows, columns, timestampMilliseconds, _configuration.MaximumEvents);
}

bool WorkspaceMirrorNodeRecorder::RecordInput(const std::wstring_view commandId,
                                               std::wstring text,
                                               const WorkspaceMirrorInputOrigin origin,
                                               std::wstring clientId,
                                               const uint64_t timestampMilliseconds)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && RecordWorkspaceMirrorInput(_session, commandId, std::move(text), origin, std::move(clientId), timestampMilliseconds, _configuration.MaximumEvents);
}

bool WorkspaceMirrorNodeRecorder::RecordOutput(const std::wstring_view commandId, std::vector<uint8_t> bytes, const uint64_t timestampMilliseconds)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && RecordWorkspaceMirrorOutput(_session, commandId, std::move(bytes), timestampMilliseconds, _configuration.MaximumEvents);
}

bool WorkspaceMirrorNodeRecorder::RecordResize(const std::wstring_view commandId,
                                                const uint32_t rows,
                                                const uint32_t columns,
                                                const uint64_t timestampMilliseconds)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && RecordWorkspaceMirrorResize(_session, commandId, rows, columns, timestampMilliseconds, _configuration.MaximumEvents);
}

bool WorkspaceMirrorNodeRecorder::RecordTitle(const std::wstring_view commandId, std::wstring title, const uint64_t timestampMilliseconds)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && RecordWorkspaceMirrorTitle(_session, commandId, std::move(title), timestampMilliseconds, _configuration.MaximumEvents);
}

bool WorkspaceMirrorNodeRecorder::AddCheckpoint(const std::wstring_view commandId, WorkspaceMirrorTerminalState state)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && AddWorkspaceMirrorCheckpoint(_session, commandId, std::move(state), _configuration.MaximumCheckpoints);
}

bool WorkspaceMirrorNodeRecorder::EndWindow(const std::wstring_view commandId, const uint64_t timestampMilliseconds)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && EndWorkspaceMirrorWindow(_session, commandId, timestampMilliseconds, _configuration.MaximumEvents);
}

WorkspaceMirrorRecoveryPlan WorkspaceMirrorNodeRecorder::PlanRecovery(const std::wstring_view commandId,
                                                                        const std::optional<uint64_t> resumeSequence) const
{
    std::scoped_lock lock{ _mutex };
    return Enabled() ? PlanWorkspaceMirrorRecovery(_session, commandId, resumeSequence) : WorkspaceMirrorRecoveryPlan{};
}

bool WorkspaceMirrorNodeRecorder::GrantControl(std::wstring clientId,
                                                std::wstring leaseId,
                                                const uint64_t nowMilliseconds,
                                                const uint64_t leaseDurationMilliseconds)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && GrantWorkspaceMirrorControl(_session, std::move(clientId), std::move(leaseId), nowMilliseconds, leaseDurationMilliseconds);
}

bool WorkspaceMirrorNodeRecorder::RevokeControl(const std::wstring_view clientId)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && RevokeWorkspaceMirrorControl(_session, clientId);
}

bool WorkspaceMirrorNodeRecorder::TryCreateRemoteInputEffect(const std::wstring_view commandId,
                                                              const std::wstring_view clientId,
                                                              const std::wstring_view leaseId,
                                                              std::wstring text,
                                                              const uint64_t nowMilliseconds,
                                                              WorkspaceMirrorEffect& effect)
{
    std::scoped_lock lock{ _mutex };
    return Enabled() && TryCreateWorkspaceMirrorInputEffect(_session,
                                                              commandId,
                                                              clientId,
                                                              leaseId,
                                                              std::move(text),
                                                              nowMilliseconds,
                                                              effect,
                                                              _configuration.MaximumEvents);
}

void WorkspaceMirrorNodeRecorder::Close()
{
    std::scoped_lock lock{ _mutex };
    _session.Closed = true;
}

WorkspaceMirrorNodeSession WorkspaceMirrorNodeRecorder::Snapshot() const
{
    std::scoped_lock lock{ _mutex };
    return _session;
}
