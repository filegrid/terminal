// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorNodeAgent.h"

namespace terminal::workspace
{
    // Binary-message adapter for terminal-server's /v1/device-tunnel. The
    // WebSocket implementation owns connection/retry concerns; this class
    // owns no socket and therefore remains deterministic and testable.
    class WorkspaceMirrorDeviceTunnelSession
    {
    public:
        explicit WorkspaceMirrorDeviceTunnelSession(std::shared_ptr<WorkspaceMirrorNodeAgent> agent);

        bool HandleBinary(std::span<const uint8_t> inbound,
                          uint64_t nowMilliseconds,
                          std::vector<std::vector<uint8_t>>& outbound);
        bool PublishOutput(std::wstring_view commandId,
                           std::vector<uint8_t> bytes,
                           uint64_t timestampMilliseconds,
                           std::vector<std::vector<uint8_t>>& outbound);
        // The WebSocket owner sends this as a binary message before the
        // server's heartbeat timeout. The server authenticates DeviceId from
        // the tunnel identity and replies with Pong.
        bool BuildHeartbeat(const std::array<uint8_t, 16>& deviceId,
                            std::vector<uint8_t>& outbound) const;

    private:
        static std::wstring _clientId(const WorkspaceMirrorRelayFrame& frame);
        static bool _encode(const std::vector<WorkspaceMirrorRelayFrame>& frames,
                            std::vector<std::vector<uint8_t>>& outbound);

        std::shared_ptr<WorkspaceMirrorNodeAgent> _agent;
    };
}
