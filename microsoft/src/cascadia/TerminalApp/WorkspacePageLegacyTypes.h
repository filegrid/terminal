#pragma once

#include "..\..\..\..\src\glue\contracts\GluePageStateTypes.h"

namespace winrt::TerminalApp::implementation
{
    using WorkspaceChatSubmitTransport = terminal::workspace::WorkspaceChatSubmitTransport;

    struct TerminalRoutingContext
    {
        uint64_t ContentId{};
        std::wstring RoutingKey;
        std::wstring TabKey;
        std::wstring TabId;
        std::wstring PaneId;
    };

    using PendingTerminalOutputCapture = terminal::workspace::PendingTerminalOutputCapture;
    using TerminalCaptureState = terminal::workspace::TerminalCaptureState;
    using WorkspaceChatUiState = terminal::workspace::WorkspaceChatUiState;
    using WorkspaceNodeRuntimeState = terminal::workspace::WorkspaceNodeRuntimeState;
}
