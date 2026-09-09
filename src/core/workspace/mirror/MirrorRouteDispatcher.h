// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorNodeRecorder.h"
#include "MirrorRelayProtocol.h"
#include "MirrorTerminalProtocol.h"

#include <functional>
#include <span>

namespace terminal::workspace
{
    // Core route state machine. The tunnel supplies/consumes RelayFrames; the
    // only Host-specific operation is an injected effect sink.
    class WorkspaceMirrorRouteDispatcher
    {
    public:
        using InputEffectSink = std::function<void(const WorkspaceMirrorEffect&)>;

        WorkspaceMirrorRouteDispatcher(std::shared_ptr<WorkspaceMirrorNodeRecorder> recorder,
                                       InputEffectSink inputEffectSink);

        bool HandleInbound(const WorkspaceMirrorRelayFrame& inbound,
                           std::wstring_view clientId,
                           uint64_t nowMilliseconds,
                           std::vector<WorkspaceMirrorRelayFrame>& outbound);

        // The terminal host calls this after receiving ConPTY output. Core
        // records the bytes once, then fans the resulting sequenced events to
        // every currently bound server route. This deliberately keeps output
        // fan-out out of the ConPTY callback and out of terminal-server.
        bool RecordOutputAndBuildEffects(std::wstring_view commandId,
                                         std::vector<uint8_t> bytes,
                                         uint64_t timestampMilliseconds,
                                         std::span<const WorkspaceMirrorRelayFrame> routes,
                                         std::vector<WorkspaceMirrorRelayFrame>& outbound);

    private:
        bool _emitRecovery(const WorkspaceMirrorRelayFrame& inbound,
                           const WorkspaceMirrorTerminalMessage& request,
                           std::vector<WorkspaceMirrorRelayFrame>& outbound) const;
        bool _emitMessage(const WorkspaceMirrorRelayFrame& inbound,
                          WorkspaceMirrorTerminalMessage message,
                          std::vector<WorkspaceMirrorRelayFrame>& outbound) const;

        std::shared_ptr<WorkspaceMirrorNodeRecorder> _recorder;
        InputEffectSink _inputEffectSink;
    };
}
