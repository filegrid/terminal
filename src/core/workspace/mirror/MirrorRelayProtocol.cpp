// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorRelayProtocol.h"

#include <algorithm>

namespace
{
    bool _isKnownKind(const WorkspaceMirrorRelayFrameKind kind) noexcept
    {
        return kind >= WorkspaceMirrorRelayFrameKind::RouteOpen && kind <= WorkspaceMirrorRelayFrameKind::Error;
    }

    size_t _payloadLimit(const WorkspaceMirrorRelayFrameKind kind) noexcept
    {
        return kind == WorkspaceMirrorRelayFrameKind::TerminalIntent || kind == WorkspaceMirrorRelayFrameKind::TerminalEffect ?
                   WorkspaceMirrorMaximumTerminalPayload :
                   WorkspaceMirrorMaximumControlPayload;
    }

    void _appendBigEndian16(std::vector<uint8_t>& output, const uint16_t value)
    {
        output.emplace_back(static_cast<uint8_t>(value >> 8));
        output.emplace_back(static_cast<uint8_t>(value));
    }

    void _appendBigEndian32(std::vector<uint8_t>& output, const uint32_t value)
    {
        output.emplace_back(static_cast<uint8_t>(value >> 24));
        output.emplace_back(static_cast<uint8_t>(value >> 16));
        output.emplace_back(static_cast<uint8_t>(value >> 8));
        output.emplace_back(static_cast<uint8_t>(value));
    }

    uint16_t _readBigEndian16(const std::span<const uint8_t> bytes, const size_t offset) noexcept
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
    }

    uint32_t _readBigEndian32(const std::span<const uint8_t> bytes, const size_t offset) noexcept
    {
        return (static_cast<uint32_t>(bytes[offset]) << 24) |
               (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
               (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
               bytes[offset + 3];
    }
}

bool EncodeWorkspaceMirrorRelayFrame(const WorkspaceMirrorRelayFrame& frame, std::vector<uint8_t>& output)
{
    if (!_isKnownKind(frame.Kind) || frame.Payload.size() > _payloadLimit(frame.Kind))
    {
        return false;
    }

    output.clear();
    output.reserve(WorkspaceMirrorRelayHeaderLength + frame.Payload.size());
    output.emplace_back(WorkspaceMirrorRelayProtocolMajor);
    output.emplace_back(static_cast<uint8_t>(frame.Kind));
    _appendBigEndian16(output, frame.Flags);
    output.insert(output.end(), frame.RouteId.begin(), frame.RouteId.end());
    output.insert(output.end(), frame.DeviceId.begin(), frame.DeviceId.end());
    output.insert(output.end(), frame.NodeSessionId.begin(), frame.NodeSessionId.end());
    output.insert(output.end(), frame.ClientId.begin(), frame.ClientId.end());
    _appendBigEndian32(output, gsl::narrow_cast<uint32_t>(frame.Payload.size()));
    output.insert(output.end(), frame.Payload.begin(), frame.Payload.end());
    return true;
}

bool DecodeWorkspaceMirrorRelayFrame(const std::span<const uint8_t> bytes, WorkspaceMirrorRelayFrame& frame)
{
    if (bytes.size() < WorkspaceMirrorRelayHeaderLength || bytes[0] != WorkspaceMirrorRelayProtocolMajor)
    {
        return false;
    }

    const auto kind = static_cast<WorkspaceMirrorRelayFrameKind>(bytes[1]);
    const auto payloadLength = _readBigEndian32(bytes, 68);
    if (!_isKnownKind(kind) ||
        payloadLength > _payloadLimit(kind) ||
        bytes.size() != WorkspaceMirrorRelayHeaderLength + payloadLength)
    {
        return false;
    }

    frame.Kind = kind;
    frame.Flags = _readBigEndian16(bytes, 2);
    std::copy_n(bytes.begin() + 4, frame.RouteId.size(), frame.RouteId.begin());
    std::copy_n(bytes.begin() + 20, frame.DeviceId.size(), frame.DeviceId.begin());
    std::copy_n(bytes.begin() + 36, frame.NodeSessionId.size(), frame.NodeSessionId.begin());
    std::copy_n(bytes.begin() + 52, frame.ClientId.size(), frame.ClientId.begin());
    frame.Payload.assign(bytes.begin() + WorkspaceMirrorRelayHeaderLength, bytes.end());
    return true;
}
