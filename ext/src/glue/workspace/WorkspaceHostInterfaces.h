// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../chat/WorkspaceChatStateHelpers.h"
#include <winrt/base.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include "WorkspaceDllApi.h"

namespace winrt::Microsoft::Terminal::Control
{
    struct ICoreState;
    struct TermControl;
}

namespace winrt::Microsoft::Terminal::TerminalConnection
{
    enum class ConnectionState : int32_t;
}

namespace winrt::TerminalApp::implementation
{
    enum class WorkspaceChatSubmitTransport : uint8_t;
    struct PendingTerminalOutputCapture;
    struct Tab;
    using TerminalCaptureState = terminal::workspacechat::TerminalCaptureState;
    struct WorkspaceChatUiState;
    struct WorkspaceNodeRuntimeState;
}

namespace terminal::workspacechat
{
    class WorkspaceChatController;
}

namespace winrt::Microsoft::Terminal::Settings::Model
{
    struct ActionAndArgs;
    struct NewTerminalArgs;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct Workspace;
    class WorkspaceManager;
}

namespace winrt::Windows::Foundation
{
    template<typename TResult>
    struct IAsyncOperation;
}

namespace winrt::Windows::UI::Xaml
{
    struct DispatcherTimer;
}

namespace terminal::workspace
{
    class TerminalPageBase
    {
    public:
        virtual ~TerminalPageBase() = default;

        virtual void InitializeWorkspaceTabRowUi() = 0;
        virtual void UpdateTerminalContentHostClip() = 0;
        virtual void RefreshWorkspaceUiAfterSettingsReload() = 0;
        virtual void ReplayPendingWorkspaceStartupInput(winrt::Microsoft::Terminal::Control::TermControl control,
                                                        winrt::Microsoft::Terminal::Control::ICoreState coreState) = 0;
        virtual void PersistWorkspaceInputPanelVisibilityFromFocusedTab(bool showInputPanel) = 0;
        virtual void ApplyWorkspaceChatStateForFocusedTab() = 0;
        virtual void FocusActiveTabSurface() = 0;
        virtual winrt::hstring CurrentWorkspaceId() const noexcept = 0;
        virtual void PrepareStartupWorkspaceState() = 0;
        virtual void ClearPendingWorkspaceStartupState() = 0;
        virtual winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmCloseWindowIfNeeded() = 0;
        virtual bool ShouldBlockSplitPaneForTab(const winrt::com_ptr<winrt::TerminalApp::implementation::Tab>& tab) const = 0;
        virtual void RegisterWorkspaceNodeRuntimeStateIfNeeded(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                                               const winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs) = 0;
    };

    class IWorkspaceTerminalPageExtension
    {
    public:
        virtual ~IWorkspaceTerminalPageExtension() = default;

        virtual void OnCreateCompleted() = 0;
        virtual void OnTerminalContentHostResized() = 0;
        virtual void OnSettingsReloaded() = 0;
        virtual void OnConnectionStateChanged(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                              const winrt::Microsoft::Terminal::Control::ICoreState& coreState,
                                              winrt::Microsoft::Terminal::TerminalConnection::ConnectionState newConnectionState) = 0;
        virtual void OnWorkspaceInputPanelChanged(bool showInputPanel, bool focusedTabChanged) = 0;
        virtual bool CanDragTabs() const = 0;
        virtual bool IsCurrentWorkspaceLocked() const = 0;
        virtual void OnPreparingStartupActions() = 0;
        virtual void OnStartupActionsCompleted() = 0;
        virtual void ReplacePendingWorkspaceNodeInputVisibility(std::vector<bool> values) = 0;
        virtual bool HasPendingWorkspaceNodeInputVisibility() const = 0;
        virtual bool ConsumePendingWorkspaceNodeInputVisibility() = 0;
        virtual void ReplacePendingWorkspaceNodeIds(std::vector<std::wstring> values) = 0;
        virtual std::wstring ConsumePendingWorkspaceNodeId() = 0;
        virtual void ClearPendingWorkspaceNodeQueues() = 0;
        virtual winrt::TerminalApp::implementation::WorkspaceChatUiState& WorkspaceChatUiState() = 0;
        virtual const winrt::TerminalApp::implementation::WorkspaceChatUiState& WorkspaceChatUiState() const = 0;
        virtual winrt::TerminalApp::implementation::TerminalCaptureState& GetOrCreateWorkspaceChatTerminalState(std::wstring_view stateKey) = 0;
        virtual winrt::TerminalApp::implementation::TerminalCaptureState* FindWorkspaceChatTerminalState(std::wstring_view stateKey) = 0;
        virtual const winrt::TerminalApp::implementation::TerminalCaptureState* FindWorkspaceChatTerminalState(std::wstring_view stateKey) const = 0;
        virtual void UpsertWorkspaceChatPendingOutputCapture(winrt::TerminalApp::implementation::PendingTerminalOutputCapture pendingCapture) = 0;
        virtual std::vector<winrt::TerminalApp::implementation::PendingTerminalOutputCapture> TakeReadyWorkspaceChatPendingOutputCaptures(uint64_t now) = 0;
        virtual bool HasWorkspaceChatPendingOutputCaptures() const = 0;
        virtual terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() = 0;
        virtual const terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() const = 0;
        virtual winrt::TerminalApp::implementation::WorkspaceChatSubmitTransport WorkspaceChatSubmitTransport() const = 0;
        virtual void SetWorkspaceChatSubmitTransport(winrt::TerminalApp::implementation::WorkspaceChatSubmitTransport transport) = 0;
        virtual winrt::Windows::UI::Xaml::DispatcherTimer& WorkspaceNameTapTimer() = 0;
        virtual bool WorkspaceNamePressedEnter() const = 0;
        virtual void SetWorkspaceNamePressedEnter(bool pressed) = 0;
        virtual winrt::hstring& CurrentWorkspaceIdState() = 0;
        virtual const winrt::hstring& CurrentWorkspaceIdState() const = 0;
        virtual std::wstring& LastWorkspaceIdState() = 0;
        virtual const std::wstring& LastWorkspaceIdState() const = 0;
        virtual std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace>& CurrentWorkspaceSaveBaseline() = 0;
        virtual const std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace>& CurrentWorkspaceSaveBaseline() const = 0;
        virtual winrt::hstring& StartupWorkspaceIdState() = 0;
        virtual const winrt::hstring& StartupWorkspaceIdState() const = 0;
        virtual winrt::Windows::UI::Xaml::Controls::TextBox::LayoutUpdated_revoker& WorkspaceSaverLayoutUpdatedRevoker() = 0;
        virtual winrt::Windows::UI::Xaml::DispatcherTimer& WorkspaceChatOutputCaptureTimer() = 0;
        virtual int& WorkspaceSaverLayoutCount() = 0;
        virtual bool& WorkspaceSaverPressedEnter() = 0;
        virtual winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& WorkspaceEditorManager() = 0;
        virtual const winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& WorkspaceEditorManager() const = 0;
        virtual size_t& WorkspaceEditorSelectedIndex() = 0;
        virtual const size_t& WorkspaceEditorSelectedIndex() const = 0;
        virtual int32_t& WorkspaceManagerNavSelection() = 0;
        virtual const int32_t& WorkspaceManagerNavSelection() const = 0;
        virtual bool& WorkspaceEditorEditMode() = 0;
        virtual const bool& WorkspaceEditorEditMode() const = 0;
        virtual bool& WorkspaceDefinitionsDirty() = 0;
        virtual const bool& WorkspaceDefinitionsDirty() const = 0;
        virtual void UpsertWorkspaceNodeRuntimeState(uint64_t contentId, const winrt::TerminalApp::implementation::WorkspaceNodeRuntimeState& state) = 0;
        virtual void RemoveWorkspaceNodeRuntimeState(uint64_t contentId) = 0;
        virtual winrt::TerminalApp::implementation::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(uint64_t contentId) = 0;
        virtual const winrt::TerminalApp::implementation::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(uint64_t contentId) const = 0;
        virtual void SetWorkspaceNodeRuntimeNodeId(uint64_t contentId, std::wstring nodeId) = 0;
        virtual void TrackWorkspaceNodeStartupAction(const winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs& action,
                                                     const std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions,
                                                     size_t index) = 0;
        virtual bool ConsumeWorkspaceStartupActionSkip(const winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs& action) = 0;
        virtual void ClearPendingWorkspaceNodeStartupAction() = 0;
        virtual std::optional<std::wstring> TakePendingWorkspaceNodeStartupAction() = 0;
        virtual void SetPendingWorkspaceNodeStartupSendInputSkip(bool skip) = 0;
        virtual bool ShouldSkipStartupAction(const winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs& action,
                                             const std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions,
                                             size_t index) = 0;
        virtual winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmCloseWindowIfNeeded() = 0;
        virtual bool ShouldBlockSplitPaneForTab(const winrt::com_ptr<winrt::TerminalApp::implementation::Tab>& tab) const = 0;
        virtual void OnTerminalControlCreated(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                              const winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs) = 0;
    };

    using CreateWorkspaceTerminalPageExtensionFn = IWorkspaceTerminalPageExtension* (WINAPI*)(TerminalPageBase* host);
    using DestroyWorkspaceTerminalPageExtensionFn = void (WINAPI*)(IWorkspaceTerminalPageExtension* extension);

    inline constexpr char CreateWorkspaceTerminalPageExtensionSymbol[] = "CreateWorkspaceTerminalPageExtension";
    inline constexpr char DestroyWorkspaceTerminalPageExtensionSymbol[] = "DestroyWorkspaceTerminalPageExtension";

    extern "C"
    {
        WT_WORKSPACE_EXT_API IWorkspaceTerminalPageExtension* WINAPI CreateWorkspaceTerminalPageExtension(TerminalPageBase* host);
        WT_WORKSPACE_EXT_API void WINAPI DestroyWorkspaceTerminalPageExtension(IWorkspaceTerminalPageExtension* extension);
    }
}
