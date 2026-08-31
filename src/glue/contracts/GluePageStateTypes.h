// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "../../core/chat/WorkspaceChatStateHelpers.h"

namespace terminal::workspace
{
    // These are contract-owned page state records. They deliberately use only
    // stable control/settings contracts and never TerminalApp implementation objects.
    enum class WorkspaceChatSubmitTransport : uint8_t
    {
        SendInputInline = 0,
        WindowKeyboardInput = 1
    };

    struct PendingTerminalOutputCapture
    {
        winrt::weak_ref<winrt::Microsoft::Terminal::Control::TermControl> Control;
        uint64_t ContentId{};
        std::wstring StateKey;
        std::wstring WorkspaceKey;
        std::wstring TabKey;
        std::wstring TabId;
        std::wstring PaneId;
        std::wstring CorrelationId;
        uint64_t DueTick{};
    };

    using TerminalCaptureState = terminal::workspacechat::TerminalCaptureState;

    struct WorkspaceChatUiState
    {
        bool EnabledForActiveTab{ false };
        bool Collapsed{ false };
        bool DraftUpdateInProgress{ false };
        bool SubmitInProgress{ false };
        bool ResizeActive{ false };
        double ExpandedHeight{ 76.0 };
        double ResizeStartHeight{ 76.0 };
        double ResizeStartPointerY{ 0.0 };
    };

    struct WorkspaceNodeRuntimeState
    {
        std::wstring WorkspaceNodeId;
        std::wstring StartupAction;
        std::wstring ExplicitCommandline;
        std::wstring StartingDirectory;
        std::wstring OperatingSystem;
        std::wstring ShellType;
        bool IsSshTransport{ false };
        bool HasSshTtyOption{ false };
        std::vector<std::wstring> DeferredStartupInputs;
        std::wstring DeferredStartupInput;
        bool StartupInputPending{ false };
        bool StartupInputDispatched{ false };
    };
}
