// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorTypes.h"

#include <span>

namespace terminal::workspace
{
    // Payload carried inside terminal-server's opaque TerminalIntent/Effect
    // envelope. All integer fields are big-endian; text is UTF-8.
    enum class WorkspaceMirrorTerminalMessageKind : uint8_t
    {
        Resume = 1,
        Input = 2,
        Output = 3,
        Resize = 4,
        Checkpoint = 5,
        Closed = 6,
        Rejected = 7,
    };

    struct WorkspaceMirrorTerminalMessage
    {
        WorkspaceMirrorTerminalMessageKind Kind{};
        std::wstring CommandId;
        std::wstring LeaseId;
        uint64_t Sequence{};
        uint32_t Rows{};
        uint32_t Columns{};
        std::wstring Text;
        std::vector<uint8_t> Bytes;
    };

    inline constexpr uint8_t WorkspaceMirrorTerminalProtocolMajor = 1;
    inline constexpr size_t WorkspaceMirrorTerminalMessageHeaderLength = 32;

    bool EncodeWorkspaceMirrorTerminalMessage(const WorkspaceMirrorTerminalMessage& message,
                                              std::vector<uint8_t>& output);
    bool DecodeWorkspaceMirrorTerminalMessage(std::span<const uint8_t> bytes,
                                              WorkspaceMirrorTerminalMessage& message);
}
