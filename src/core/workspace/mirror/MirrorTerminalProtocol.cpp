// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorTerminalProtocol.h"

#include <Windows.h>

namespace
{
    using namespace terminal::workspace;

    void _append16(std::vector<uint8_t>& output, const uint16_t value)
    {
        output.emplace_back(gsl::narrow_cast<uint8_t>(value >> 8));
        output.emplace_back(gsl::narrow_cast<uint8_t>(value));
    }

    void _append32(std::vector<uint8_t>& output, const uint32_t value)
    {
        for (auto shift = 24; shift >= 0; shift -= 8)
        {
            output.emplace_back(gsl::narrow_cast<uint8_t>(value >> shift));
        }
    }

    void _append64(std::vector<uint8_t>& output, const uint64_t value)
    {
        for (auto shift = 56; shift >= 0; shift -= 8)
        {
            output.emplace_back(gsl::narrow_cast<uint8_t>(value >> shift));
        }
    }

    uint16_t _read16(const std::span<const uint8_t> bytes, const size_t offset)
    {
        return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
    }

    uint32_t _read32(const std::span<const uint8_t> bytes, const size_t offset)
    {
        uint32_t value{};
        for (size_t index{}; index < 4; ++index) value = (value << 8) | bytes[offset + index];
        return value;
    }

    uint64_t _read64(const std::span<const uint8_t> bytes, const size_t offset)
    {
        uint64_t value{};
        for (size_t index{}; index < 8; ++index) value = (value << 8) | bytes[offset + index];
        return value;
    }

    bool _toUtf8(const std::wstring_view value, std::vector<uint8_t>& output)
    {
        const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), gsl::narrow<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size < 0 || size > UINT16_MAX) return false;
        const auto begin = output.size();
        output.resize(begin + gsl::narrow<size_t>(size));
        return !size || WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), gsl::narrow<int>(value.size()), reinterpret_cast<char*>(output.data() + begin), size, nullptr, nullptr) == size;
    }

    bool _fromUtf8(const std::span<const uint8_t> value, std::wstring& output)
    {
        const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(value.data()), gsl::narrow<int>(value.size()), nullptr, 0);
        if (size < 0) return false;
        output.resize(gsl::narrow<size_t>(size));
        return !size || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(value.data()), gsl::narrow<int>(value.size()), output.data(), size) == size;
    }

    bool _known(const WorkspaceMirrorTerminalMessageKind kind)
    {
        return kind >= WorkspaceMirrorTerminalMessageKind::Resume && kind <= WorkspaceMirrorTerminalMessageKind::Rejected;
    }
}

bool terminal::workspace::EncodeWorkspaceMirrorTerminalMessage(const WorkspaceMirrorTerminalMessage& message,
                                                                std::vector<uint8_t>& output)
{
    if (!_known(message.Kind) || message.Bytes.size() > WorkspaceMirrorMaximumTerminalPayload) return false;
    std::vector<uint8_t> command;
    std::vector<uint8_t> lease;
    std::vector<uint8_t> text;
    if (!_toUtf8(message.CommandId, command) || !_toUtf8(message.LeaseId, lease) || !_toUtf8(message.Text, text) ||
        command.size() > UINT16_MAX || lease.size() > UINT16_MAX || text.size() > WorkspaceMirrorMaximumControlPayload) return false;
    output.clear();
    output.reserve(WorkspaceMirrorTerminalMessageHeaderLength + command.size() + lease.size() + text.size() + message.Bytes.size());
    output.emplace_back(WorkspaceMirrorTerminalProtocolMajor);
    output.emplace_back(static_cast<uint8_t>(message.Kind));
    _append16(output, gsl::narrow<uint16_t>(command.size()));
    _append16(output, gsl::narrow<uint16_t>(lease.size()));
    _append16(output, 0);
    _append64(output, message.Sequence);
    _append32(output, message.Rows);
    _append32(output, message.Columns);
    _append32(output, gsl::narrow<uint32_t>(text.size()));
    _append32(output, gsl::narrow<uint32_t>(message.Bytes.size()));
    output.insert(output.end(), command.begin(), command.end());
    output.insert(output.end(), lease.begin(), lease.end());
    output.insert(output.end(), text.begin(), text.end());
    output.insert(output.end(), message.Bytes.begin(), message.Bytes.end());
    return true;
}

bool terminal::workspace::DecodeWorkspaceMirrorTerminalMessage(const std::span<const uint8_t> bytes,
                                                                WorkspaceMirrorTerminalMessage& message)
{
    if (bytes.size() < WorkspaceMirrorTerminalMessageHeaderLength || bytes[0] != WorkspaceMirrorTerminalProtocolMajor) return false;
    const auto kind = static_cast<WorkspaceMirrorTerminalMessageKind>(bytes[1]);
    const auto commandLength = _read16(bytes, 2);
    const auto leaseLength = _read16(bytes, 4);
    const auto textLength = _read32(bytes, 24);
    const auto payloadLength = _read32(bytes, 28);
    const auto total = WorkspaceMirrorTerminalMessageHeaderLength + commandLength + leaseLength + static_cast<size_t>(textLength) + payloadLength;
    if (!_known(kind) || textLength > WorkspaceMirrorMaximumControlPayload || payloadLength > WorkspaceMirrorMaximumTerminalPayload || total != bytes.size()) return false;
    size_t offset = WorkspaceMirrorTerminalMessageHeaderLength;
    WorkspaceMirrorTerminalMessage decoded;
    decoded.Kind = kind;
    decoded.Sequence = _read64(bytes, 8);
    decoded.Rows = _read32(bytes, 16);
    decoded.Columns = _read32(bytes, 20);
    if (!_fromUtf8(bytes.subspan(offset, commandLength), decoded.CommandId)) return false;
    offset += commandLength;
    if (!_fromUtf8(bytes.subspan(offset, leaseLength), decoded.LeaseId)) return false;
    offset += leaseLength;
    if (!_fromUtf8(bytes.subspan(offset, textLength), decoded.Text)) return false;
    offset += textLength;
    decoded.Bytes.assign(bytes.begin() + offset, bytes.end());
    message = std::move(decoded);
    return true;
}
