// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceHostInterfaces.h"
#include "..\chat\TerminalInputHarness.h"
#include "TerminalPageWorkspaceTypes.h"
#include "..\chat\WorkspaceChatController.h"
#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"
#include "../TerminalSettingsModel/Workspace.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace terminal::workspace
{
    class WorkspaceTerminalPageExtension final : public IWorkspaceTerminalPageExtension
    {
    public:
        explicit WorkspaceTerminalPageExtension(TerminalPageBase& host) noexcept :
            _host{ host } {}

        void OnCreateCompleted() override
        {
            _host.InitializeWorkspaceTabRowUi();
        }

        void OnTerminalContentHostResized() override
        {
            _host.UpdateTerminalContentHostClip();
        }

        void OnSettingsReloaded() override
        {
            _host.RefreshWorkspaceUiAfterSettingsReload();
        }

        void OnConnectionStateChanged(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                      const winrt::Microsoft::Terminal::Control::ICoreState& coreState,
                                      const winrt::Microsoft::Terminal::TerminalConnection::ConnectionState newConnectionState) override
        {
            using winrt::Microsoft::Terminal::TerminalConnection::ConnectionState;
            if (newConnectionState >= ConnectionState::Connected &&
                newConnectionState < ConnectionState::Closed)
            {
                _host.ReplayPendingWorkspaceStartupInput(control, coreState);
            }
        }

        void OnWorkspaceInputPanelChanged(const bool showInputPanel, const bool focusedTabChanged) override
        {
            _host.PersistWorkspaceInputPanelVisibilityFromFocusedTab(showInputPanel);
            if (focusedTabChanged)
            {
                _host.ApplyWorkspaceChatStateForFocusedTab();
                _host.FocusActiveTabSurface();
            }
        }

        bool CanDragTabs() const override
        {
            return !IsCurrentWorkspaceLocked();
        }

        bool IsCurrentWorkspaceLocked() const override
        {
            const auto workspaceId = _host.CurrentWorkspaceId();
            if (workspaceId.empty())
            {
                return false;
            }

            const auto manager = winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
            if (const auto workspace = manager.FindById(workspaceId.c_str()))
            {
                return workspace->Locked;
            }

            return false;
        }

        void OnPreparingStartupActions() override
        {
            _host.PrepareStartupWorkspaceState();
        }

        void OnStartupActionsCompleted() override
        {
            _host.ClearPendingWorkspaceStartupState();
        }

        void ReplacePendingWorkspaceNodeInputVisibility(std::vector<bool> values) override
        {
            _pendingWorkspaceNodeInputVisibility.clear();
            for (const auto value : values)
            {
                _pendingWorkspaceNodeInputVisibility.emplace_back(value);
            }
        }

        bool HasPendingWorkspaceNodeInputVisibility() const override
        {
            return !_pendingWorkspaceNodeInputVisibility.empty();
        }

        bool ConsumePendingWorkspaceNodeInputVisibility() override
        {
            if (_pendingWorkspaceNodeInputVisibility.empty())
            {
                return false;
            }

            const auto showInputPanel = _pendingWorkspaceNodeInputVisibility.front();
            _pendingWorkspaceNodeInputVisibility.pop_front();
            return showInputPanel;
        }

        void ReplacePendingWorkspaceNodeIds(std::vector<std::wstring> values) override
        {
            _pendingWorkspaceNodeIds.clear();
            for (auto& value : values)
            {
                _pendingWorkspaceNodeIds.emplace_back(std::move(value));
            }
        }

        std::wstring ConsumePendingWorkspaceNodeId() override
        {
            if (_pendingWorkspaceNodeIds.empty())
            {
                return {};
            }

            auto nodeId = std::move(_pendingWorkspaceNodeIds.front());
            _pendingWorkspaceNodeIds.pop_front();
            return nodeId;
        }

        void ClearPendingWorkspaceNodeQueues() override
        {
            _pendingWorkspaceNodeInputVisibility.clear();
            _pendingWorkspaceNodeIds.clear();
        }

        winrt::TerminalApp::implementation::WorkspaceChatUiState& WorkspaceChatUiState() override
        {
            return _workspaceChatUiState;
        }

        const winrt::TerminalApp::implementation::WorkspaceChatUiState& WorkspaceChatUiState() const override
        {
            return _workspaceChatUiState;
        }

        std::unordered_map<std::wstring, winrt::TerminalApp::implementation::TerminalCaptureState>& WorkspaceChatTerminalStates() override
        {
            return _workspaceChatTerminalStates;
        }

        const std::unordered_map<std::wstring, winrt::TerminalApp::implementation::TerminalCaptureState>& WorkspaceChatTerminalStates() const override
        {
            return _workspaceChatTerminalStates;
        }

        std::vector<winrt::TerminalApp::implementation::PendingTerminalOutputCapture>& WorkspaceChatPendingOutputCaptures() override
        {
            return _workspaceChatPendingOutputCaptures;
        }

        const std::vector<winrt::TerminalApp::implementation::PendingTerminalOutputCapture>& WorkspaceChatPendingOutputCaptures() const override
        {
            return _workspaceChatPendingOutputCaptures;
        }

        terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() override
        {
            return _workspaceChatController;
        }

        const terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() const override
        {
            return _workspaceChatController;
        }

        winrt::TerminalApp::implementation::WorkspaceChatSubmitTransport WorkspaceChatSubmitTransport() const override
        {
            return _workspaceChatSubmitTransport;
        }

        void SetWorkspaceChatSubmitTransport(const winrt::TerminalApp::implementation::WorkspaceChatSubmitTransport transport) override
        {
            _workspaceChatSubmitTransport = transport;
        }

        winrt::Windows::UI::Xaml::DispatcherTimer& WorkspaceNameTapTimer() override
        {
            return _workspaceNameTapTimer;
        }

        bool WorkspaceNamePressedEnter() const override
        {
            return _workspaceNamePressedEnter;
        }

        void SetWorkspaceNamePressedEnter(const bool pressed) override
        {
            _workspaceNamePressedEnter = pressed;
        }

        winrt::hstring& CurrentWorkspaceIdState() override
        {
            return _currentWorkspaceId;
        }

        const winrt::hstring& CurrentWorkspaceIdState() const override
        {
            return _currentWorkspaceId;
        }

        std::wstring& LastWorkspaceIdState() override
        {
            return _lastWorkspaceId;
        }

        const std::wstring& LastWorkspaceIdState() const override
        {
            return _lastWorkspaceId;
        }

        std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace>& CurrentWorkspaceSaveBaseline() override
        {
            return _currentWorkspaceSaveBaseline;
        }

        const std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace>& CurrentWorkspaceSaveBaseline() const override
        {
            return _currentWorkspaceSaveBaseline;
        }

        winrt::hstring& StartupWorkspaceIdState() override
        {
            return _startupWorkspaceId;
        }

        const winrt::hstring& StartupWorkspaceIdState() const override
        {
            return _startupWorkspaceId;
        }

        winrt::Windows::UI::Xaml::Controls::TextBox::LayoutUpdated_revoker& WorkspaceSaverLayoutUpdatedRevoker() override
        {
            return _workspaceSaverLayoutUpdatedRevoker;
        }

        winrt::Windows::UI::Xaml::DispatcherTimer& WorkspaceChatOutputCaptureTimer() override
        {
            return _workspaceChatOutputCaptureTimer;
        }

        int& WorkspaceSaverLayoutCount() override
        {
            return _workspaceSaverLayoutCount;
        }

        bool& WorkspaceSaverPressedEnter() override
        {
            return _workspaceSaverPressedEnter;
        }

        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& WorkspaceEditorManager() override
        {
            return _workspaceEditorManager;
        }

        const winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& WorkspaceEditorManager() const override
        {
            return _workspaceEditorManager;
        }

        size_t& WorkspaceEditorSelectedIndex() override
        {
            return _workspaceEditorSelectedIndex;
        }

        const size_t& WorkspaceEditorSelectedIndex() const override
        {
            return _workspaceEditorSelectedIndex;
        }

        int32_t& WorkspaceManagerNavSelection() override
        {
            return _workspaceManagerNavSelection;
        }

        const int32_t& WorkspaceManagerNavSelection() const override
        {
            return _workspaceManagerNavSelection;
        }

        bool& WorkspaceEditorEditMode() override
        {
            return _workspaceEditorEditMode;
        }

        const bool& WorkspaceEditorEditMode() const override
        {
            return _workspaceEditorEditMode;
        }

        bool& WorkspaceDefinitionsDirty() override
        {
            return _workspaceDefinitionsDirty;
        }

        const bool& WorkspaceDefinitionsDirty() const override
        {
            return _workspaceDefinitionsDirty;
        }

        void UpsertWorkspaceNodeRuntimeState(const uint64_t contentId, const winrt::TerminalApp::implementation::WorkspaceNodeRuntimeState& state) override
        {
            _workspaceNodeRuntimeStates[contentId] = state;
        }

        void RemoveWorkspaceNodeRuntimeState(const uint64_t contentId) override
        {
            _workspaceNodeRuntimeStates.erase(contentId);
        }

        winrt::TerminalApp::implementation::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(const uint64_t contentId) override
        {
            if (const auto it = _workspaceNodeRuntimeStates.find(contentId); it != _workspaceNodeRuntimeStates.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        const winrt::TerminalApp::implementation::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(const uint64_t contentId) const override
        {
            if (const auto it = _workspaceNodeRuntimeStates.find(contentId); it != _workspaceNodeRuntimeStates.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        void SetWorkspaceNodeRuntimeNodeId(const uint64_t contentId, std::wstring nodeId) override
        {
            if (contentId == 0)
            {
                return;
            }

            _workspaceNodeRuntimeStates[contentId].WorkspaceNodeId = std::move(nodeId);
        }

        void TrackWorkspaceNodeStartupAction(const winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs& action,
                                             const std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions,
                                             const size_t index) override
        {
            if (action.Action() == ShortcutAction::SendInput)
            {
                return;
            }

            _pendingWorkspaceNodeStartupAction.reset();
            _skipNextWorkspaceNodeStartupSendInput = false;

            if (action.Action() != ShortcutAction::NewTab || index + 1 >= actions.size())
            {
                return;
            }

            const auto nextArgs = actions[index + 1].Args().try_as<SendInputArgs>();
            if (!nextArgs)
            {
                return;
            }

            auto startupAction = std::wstring{ nextArgs.Input().c_str() };
            while (!startupAction.empty() && (startupAction.back() == L'\r' || startupAction.back() == L'\n'))
            {
                startupAction.pop_back();
            }

            if (!startupAction.empty())
            {
                _pendingWorkspaceNodeStartupAction = std::move(startupAction);
            }
        }

        bool ConsumeWorkspaceStartupActionSkip(const winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs& action) override
        {
            if (_skipNextWorkspaceNodeStartupSendInput && action.Action() == ShortcutAction::SendInput)
            {
                _skipNextWorkspaceNodeStartupSendInput = false;
                return true;
            }

            return false;
        }

        void ClearPendingWorkspaceNodeStartupAction() override
        {
            _pendingWorkspaceNodeStartupAction.reset();
        }

        std::optional<std::wstring> TakePendingWorkspaceNodeStartupAction() override
        {
            auto pending = std::move(_pendingWorkspaceNodeStartupAction);
            _pendingWorkspaceNodeStartupAction.reset();
            return pending;
        }

        void SetPendingWorkspaceNodeStartupSendInputSkip(const bool skip) override
        {
            _skipNextWorkspaceNodeStartupSendInput = skip;
        }

        bool ShouldSkipStartupAction(const winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs& action,
                                     const std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions,
                                     const size_t index) override
        {
            TrackWorkspaceNodeStartupAction(action, actions, index);
            return ConsumeWorkspaceStartupActionSkip(action);
        }

        winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmCloseWindowIfNeeded() override
        {
            co_return co_await _host.ConfirmCloseWindowIfNeeded();
        }

        bool ShouldBlockSplitPaneForTab(const winrt::com_ptr<winrt::TerminalApp::implementation::Tab>& tab) const override
        {
            return _host.ShouldBlockSplitPaneForTab(tab);
        }

        void OnTerminalControlCreated(const winrt::Microsoft::Terminal::Control::TermControl& control,
                                      const winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs) override
        {
            _host.RegisterWorkspaceNodeRuntimeStateIfNeeded(control, newTerminalArgs);
        }

    private:
        TerminalPageBase& _host;
        std::deque<bool> _pendingWorkspaceNodeInputVisibility;
        std::deque<std::wstring> _pendingWorkspaceNodeIds;
        winrt::TerminalApp::implementation::WorkspaceChatUiState _workspaceChatUiState;
        std::unordered_map<std::wstring, winrt::TerminalApp::implementation::TerminalCaptureState> _workspaceChatTerminalStates;
        std::vector<winrt::TerminalApp::implementation::PendingTerminalOutputCapture> _workspaceChatPendingOutputCaptures;
        terminal::workspacechat::WorkspaceChatController _workspaceChatController;
        winrt::TerminalApp::implementation::WorkspaceChatSubmitTransport _workspaceChatSubmitTransport{ winrt::TerminalApp::implementation::WorkspaceChatSubmitTransport::SendInputInline };
        winrt::Windows::UI::Xaml::DispatcherTimer _workspaceNameTapTimer{ nullptr };
        bool _workspaceNamePressedEnter{ false };
        winrt::hstring _currentWorkspaceId{};
        std::wstring _lastWorkspaceId{};
        std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace> _currentWorkspaceSaveBaseline;
        winrt::hstring _startupWorkspaceId{};
        winrt::Windows::UI::Xaml::DispatcherTimer _workspaceChatOutputCaptureTimer{ nullptr };
        winrt::Windows::UI::Xaml::Controls::TextBox::LayoutUpdated_revoker _workspaceSaverLayoutUpdatedRevoker;
        int _workspaceSaverLayoutCount{ 0 };
        bool _workspaceSaverPressedEnter{ false };
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager _workspaceEditorManager{};
        size_t _workspaceEditorSelectedIndex{ 0 };
        int32_t _workspaceManagerNavSelection{ 0 };
        bool _workspaceEditorEditMode{ false };
        bool _workspaceDefinitionsDirty{ false };
        std::unordered_map<uint64_t, winrt::TerminalApp::implementation::WorkspaceNodeRuntimeState> _workspaceNodeRuntimeStates;
        std::optional<std::wstring> _pendingWorkspaceNodeStartupAction;
        bool _skipNextWorkspaceNodeStartupSendInput{ false };
    };

    extern "C" IWorkspaceTerminalPageExtension* WINAPI CreateWorkspaceTerminalPageExtension(TerminalPageBase* host)
    {
        if (!host)
        {
            return nullptr;
        }

        return new WorkspaceTerminalPageExtension(*host);
    }

    extern "C" void WINAPI DestroyWorkspaceTerminalPageExtension(IWorkspaceTerminalPageExtension* extension)
    {
        delete extension;
    }
}
