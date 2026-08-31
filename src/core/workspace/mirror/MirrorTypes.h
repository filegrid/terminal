// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace terminal::workspace
{
    enum class WorkspaceMirrorEventKind
    {
        Started,
        Input,
        Output,
        Resize,
        Title,
        Checkpoint,
        Exited,
        Gap,
    };

    enum class WorkspaceMirrorInputOrigin
    {
        Host,
        Remote,
        Startup,
        System,
    };

    enum class WorkspaceMirrorPermission
    {
        View,
        Control,
    };

    enum class WorkspaceMirrorRecoveryKind
    {
        Resume,
        Checkpoint,
        ReplayFromStart,
        Unavailable,
    };

    enum class WorkspaceNodeMirrorMode
    {
        Disabled,
        NodeSession,
    };

    // Kept below terminal-server's 1 MiB relay limit so a Core event maps to
    // one bounded transport payload without transport-side fragmentation.
    inline constexpr size_t WorkspaceMirrorMaximumOutputChunkBytes = 64 * 1024;

    // Persisted Node policy. NodeSession means that every configured command
    // gets a recorder before its ConPTY is started; there is intentionally no
    // runtime "start recording" switch that could lose an active TUI state.
    struct WorkspaceNodeMirrorConfiguration
    {
        WorkspaceNodeMirrorMode Mode{ WorkspaceNodeMirrorMode::Disabled };
        uint32_t MaximumEvents{ 4096 };
        uint32_t MaximumCheckpoints{ 4 };
    };

    struct WorkspaceMirrorTerminalState
    {
        uint32_t Rows{};
        uint32_t Columns{};
        std::vector<uint8_t> Bytes;
    };

    struct WorkspaceMirrorEvent
    {
        uint64_t Sequence{};
        uint64_t TimestampMilliseconds{};
        WorkspaceMirrorEventKind Kind{};
        WorkspaceMirrorInputOrigin Origin{ WorkspaceMirrorInputOrigin::System };
        std::wstring ClientId;
        std::wstring Text;
        std::vector<uint8_t> Bytes;
        uint32_t Rows{};
        uint32_t Columns{};
    };

    struct WorkspaceMirrorCheckpoint
    {
        uint64_t Sequence{};
        WorkspaceMirrorTerminalState State;
    };

    struct WorkspaceMirrorWindowState
    {
        std::wstring CommandId;
        bool Started{ false };
        bool Exited{ false };
        bool HasGap{ false };
        uint64_t HeadSequence{};
        uint32_t Rows{};
        uint32_t Columns{};
        std::wstring Title;
        std::vector<WorkspaceMirrorEvent> Events;
        std::vector<WorkspaceMirrorCheckpoint> Checkpoints;
    };

    struct WorkspaceMirrorControlLease
    {
        std::wstring ClientId;
        std::wstring LeaseId;
        uint64_t ExpiresAtMilliseconds{};
    };

    struct WorkspaceMirrorCapability
    {
        std::wstring UserId;
        std::wstring DeviceId;
        std::wstring NodeSessionId;
        WorkspaceMirrorPermission Permission{ WorkspaceMirrorPermission::View };
        uint64_t ExpiresAtMilliseconds{};
        uint64_t PolicyVersion{};
    };

    struct WorkspaceMirrorNodeSession
    {
        std::wstring WorkspaceId;
        std::wstring NodeId;
        std::wstring NodeSessionId;
        uint64_t HeadSequence{};
        bool Closed{ false };
        std::vector<WorkspaceMirrorWindowState> Windows;
        std::optional<WorkspaceMirrorControlLease> ControlLease;
    };

    struct WorkspaceMirrorRecoveryPlan
    {
        WorkspaceMirrorRecoveryKind Kind{ WorkspaceMirrorRecoveryKind::Unavailable };
        uint64_t BaseSequence{};
        uint64_t HeadSequence{};
        std::optional<WorkspaceMirrorTerminalState> Checkpoint;
        std::vector<WorkspaceMirrorEvent> Events;
    };

    struct WorkspaceMirrorEffect
    {
        enum class Kind
        {
            WriteInput,
            PublishWindow,
            RevokeControl,
        } Type{};

        std::wstring CommandId;
        std::wstring ClientId;
        std::wstring Text;
    };
}
