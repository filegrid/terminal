#pragma once

namespace winrt::TerminalApp::implementation
{
    enum class WorkspaceChatSubmitTransport : uint8_t
    {
        SendInputInline = 0,
        WindowKeyboardInput = 1
    };

    struct TerminalRoutingContext
    {
        uint64_t ContentId{};
        std::wstring RoutingKey;
        std::wstring TabKey;
        std::wstring TabId;
        std::wstring PaneId;
    };

    struct PendingTerminalOutputCapture
    {
        winrt::weak_ref<winrt::Microsoft::Terminal::Control::TermControl> Control;
        std::wstring StateKey;
        std::wstring WorkspaceKey;
        std::wstring TabKey;
        std::wstring TabId;
        std::wstring PaneId;
        std::wstring CorrelationId;
        uint64_t DueTick{};
    };

    struct TerminalCaptureState
    {
        std::wstring PendingInput;
        std::wstring LastSubmittedInput;
        std::wstring LastBufferSnapshot;
        std::wstring LastReportedWorkingDirectory;
        bool HasBufferSnapshot{ false };
        bool PreferTwoPhaseSubmit{ false };
        terminal::workspacechat::TerminalInputState InputState;
    };

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
