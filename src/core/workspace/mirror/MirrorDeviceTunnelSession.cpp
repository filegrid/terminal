// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorDeviceTunnelSession.h"

#include <cwchar>

// MirrorCore.cpp includes this implementation inside terminal::workspace.

WorkspaceMirrorDeviceTunnelSession::WorkspaceMirrorDeviceTunnelSession(std::shared_ptr<WorkspaceMirrorNodeAgent> agent) :
    _agent{ std::move(agent) }
{
}

std::wstring WorkspaceMirrorDeviceTunnelSession::_clientId(const WorkspaceMirrorRelayFrame& frame)
{
    const auto& value = frame.ClientId;
    wchar_t text[37]{};
    const auto written = swprintf_s(text,
                                    L"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                                    value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7],
                                    value[8], value[9], value[10], value[11], value[12], value[13], value[14], value[15]);
    return written == 36 ? std::wstring{ text } : std::wstring{};
}

bool WorkspaceMirrorDeviceTunnelSession::_encode(const std::vector<WorkspaceMirrorRelayFrame>& frames,
                                                  std::vector<std::vector<uint8_t>>& outbound)
{
    for (const auto& frame : frames)
    {
        std::vector<uint8_t> bytes;
        if (!EncodeWorkspaceMirrorRelayFrame(frame, bytes))
        {
            return false;
        }
        outbound.emplace_back(std::move(bytes));
    }
    return true;
}

bool WorkspaceMirrorDeviceTunnelSession::HandleBinary(const std::span<const uint8_t> inbound,
                                                       const uint64_t nowMilliseconds,
                                                       std::vector<std::vector<uint8_t>>& outbound)
{
    if (!_agent)
    {
        return false;
    }
    WorkspaceMirrorRelayFrame frame;
    if (!DecodeWorkspaceMirrorRelayFrame(inbound, frame))
    {
        return false;
    }
    std::vector<WorkspaceMirrorRelayFrame> response;
    return _agent->HandleServerFrame(frame, _clientId(frame), nowMilliseconds, response) && _encode(response, outbound);
}

bool WorkspaceMirrorDeviceTunnelSession::PublishOutput(const std::wstring_view commandId,
                                                        std::vector<uint8_t> bytes,
                                                        const uint64_t timestampMilliseconds,
                                                        std::vector<std::vector<uint8_t>>& outbound)
{
    if (!_agent)
    {
        return false;
    }
    std::vector<WorkspaceMirrorRelayFrame> response;
    return _agent->PublishOutput(commandId, std::move(bytes), timestampMilliseconds, response) && _encode(response, outbound);
}

bool WorkspaceMirrorDeviceTunnelSession::BuildHeartbeat(const std::array<uint8_t, 16>& deviceId,
                                                         std::vector<uint8_t>& outbound) const
{
    WorkspaceMirrorRelayFrame ping;
    ping.Kind = WorkspaceMirrorRelayFrameKind::Ping;
    ping.DeviceId = deviceId;
    return EncodeWorkspaceMirrorRelayFrame(ping, outbound);
}
