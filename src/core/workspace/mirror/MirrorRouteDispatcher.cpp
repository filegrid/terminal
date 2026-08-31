// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorRouteDispatcher.h"

    WorkspaceMirrorRouteDispatcher::WorkspaceMirrorRouteDispatcher(std::shared_ptr<WorkspaceMirrorNodeRecorder> recorder,
                                                                     InputEffectSink inputEffectSink) :
        _recorder{ std::move(recorder) },
        _inputEffectSink{ std::move(inputEffectSink) }
    {
    }

    bool WorkspaceMirrorRouteDispatcher::_emitMessage(const WorkspaceMirrorRelayFrame& inbound,
                                                       WorkspaceMirrorTerminalMessage message,
                                                       std::vector<WorkspaceMirrorRelayFrame>& outbound) const
    {
        WorkspaceMirrorRelayFrame effect = inbound;
        effect.Kind = WorkspaceMirrorRelayFrameKind::TerminalEffect;
        effect.Flags = 0;
        return EncodeWorkspaceMirrorTerminalMessage(message, effect.Payload) && (outbound.emplace_back(std::move(effect)), true);
    }

    bool WorkspaceMirrorRouteDispatcher::_emitRecovery(const WorkspaceMirrorRelayFrame& inbound,
                                                        const WorkspaceMirrorTerminalMessage& request,
                                                        std::vector<WorkspaceMirrorRelayFrame>& outbound) const
    {
        const auto plan = _recorder->PlanRecovery(request.CommandId, request.Sequence ? std::optional{ request.Sequence } : std::nullopt);
        if (plan.Kind == WorkspaceMirrorRecoveryKind::Unavailable)
        {
            return _emitMessage(inbound, { .Kind = WorkspaceMirrorTerminalMessageKind::Rejected, .CommandId = request.CommandId, .Text = L"resync-unavailable" }, outbound);
        }
        if (plan.Checkpoint && !_emitMessage(inbound, { .Kind = WorkspaceMirrorTerminalMessageKind::Checkpoint, .CommandId = request.CommandId, .Sequence = plan.BaseSequence, .Rows = plan.Checkpoint->Rows, .Columns = plan.Checkpoint->Columns, .Bytes = plan.Checkpoint->Bytes }, outbound)) return false;
        for (const auto& event : plan.Events)
        {
            WorkspaceMirrorTerminalMessage message{ .CommandId = request.CommandId, .Sequence = event.Sequence, .Rows = event.Rows, .Columns = event.Columns, .Text = event.Text, .Bytes = event.Bytes };
            switch (event.Kind)
            {
            case WorkspaceMirrorEventKind::Output: message.Kind = WorkspaceMirrorTerminalMessageKind::Output; break;
            case WorkspaceMirrorEventKind::Resize: message.Kind = WorkspaceMirrorTerminalMessageKind::Resize; break;
            case WorkspaceMirrorEventKind::Exited: message.Kind = WorkspaceMirrorTerminalMessageKind::Closed; break;
            default: continue;
            }
            if (!_emitMessage(inbound, std::move(message), outbound)) return false;
        }
        return true;
    }

    bool WorkspaceMirrorRouteDispatcher::HandleInbound(const WorkspaceMirrorRelayFrame& inbound,
                                                        const std::wstring_view clientId,
                                                        const uint64_t nowMilliseconds,
                                                        std::vector<WorkspaceMirrorRelayFrame>& outbound)
    {
        if (!_recorder || !_recorder->Enabled()) return false;
        if (inbound.Kind == WorkspaceMirrorRelayFrameKind::RouteOpen)
        {
            auto accepted = inbound;
            accepted.Kind = WorkspaceMirrorRelayFrameKind::RouteAccepted;
            accepted.Payload.clear();
            outbound.emplace_back(std::move(accepted));
            return true;
        }
        if (inbound.Kind == WorkspaceMirrorRelayFrameKind::Ping)
        {
            auto pong = inbound;
            pong.Kind = WorkspaceMirrorRelayFrameKind::Pong;
            pong.Payload.clear();
            outbound.emplace_back(std::move(pong));
            return true;
        }
        if (inbound.Kind != WorkspaceMirrorRelayFrameKind::TerminalIntent) return false;
        WorkspaceMirrorTerminalMessage request;
        if (!DecodeWorkspaceMirrorTerminalMessage(inbound.Payload, request)) return false;
        if (request.Kind == WorkspaceMirrorTerminalMessageKind::Resume) return _emitRecovery(inbound, request, outbound);
        if (request.Kind != WorkspaceMirrorTerminalMessageKind::Input) return false;
        WorkspaceMirrorEffect effect;
        if (!_recorder->TryCreateRemoteInputEffect(request.CommandId, clientId, request.LeaseId, std::move(request.Text), nowMilliseconds, effect))
        {
            return _emitMessage(inbound, { .Kind = WorkspaceMirrorTerminalMessageKind::Rejected, .CommandId = request.CommandId, .Text = L"input-rejected" }, outbound);
        }
        if (_inputEffectSink) _inputEffectSink(effect);
        return true;
    }
