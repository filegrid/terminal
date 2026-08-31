// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

#include <array>
#include <span>

namespace terminal::workspace
{
    // This is the binary envelope spoken by terminal-server's device tunnel.
    // The server deliberately does not interpret the terminal payload: it only
    // authorizes a route and relays an intent/effect between terminal hosts.
    enum class WorkspaceMirrorRelayFrameKind : uint8_t
    {
        RouteOpen = 1,
        RouteAccepted = 2,
        RouteClose = 3,
        TerminalIntent = 4,
        TerminalEffect = 5,
        PolicyChanged = 6,
        Ping = 7,
        Pong = 8,
        Error = 9,
    };

    struct WorkspaceMirrorRelayFrame
    {
        WorkspaceMirrorRelayFrameKind Kind{};
        uint16_t Flags{};
        std::array<uint8_t, 16> RouteId{};
        std::array<uint8_t, 16> DeviceId{};
        std::array<uint8_t, 16> NodeSessionId{};
        std::array<uint8_t, 16> ClientId{};
        std::vector<uint8_t> Payload;
    };

    inline constexpr uint8_t WorkspaceMirrorRelayProtocolMajor = 1;
    inline constexpr size_t WorkspaceMirrorRelayHeaderLength = 72;
    inline constexpr size_t WorkspaceMirrorMaximumControlPayload = 64 * 1024;
    inline constexpr size_t WorkspaceMirrorMaximumTerminalPayload = 1024 * 1024;

    bool EncodeWorkspaceMirrorRelayFrame(const WorkspaceMirrorRelayFrame& frame, std::vector<uint8_t>& output);
    bool DecodeWorkspaceMirrorRelayFrame(std::span<const uint8_t> bytes, WorkspaceMirrorRelayFrame& frame);
}
