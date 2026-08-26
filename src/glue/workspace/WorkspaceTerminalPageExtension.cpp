// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "../../contracts/GluePageHostContract.h"
#include "WorkspaceApi.h"
#include "WorkspaceExtLoader.h"
#include "WorkspaceManagerPaneContent.h"
#include "WorkspaceManagerIconPickerDialog.h"
#include "WorkspaceManagerDeleteConfirmationDialog.h"
#include "WorkspaceManagerColorPickerDialog.h"
#include "WorkspaceManagerPathPicker.h"
#include "WorkspaceManagerProfilePicker.h"
#include "..\chat\TerminalInputHarness.h"
#include "..\chat\WorkspaceChatController.h"
#include "../../core/chat/WorkspaceDiagnosticLog.h"
#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"
#include "../TerminalSettingsModel/Workspace.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace terminal::workspace
{
    namespace
    {
        // This TU owns the workspace terminal glue include chain, status writes, and process-based diagnostics.
        void _logWorkspaceChatControllerSubmit(const terminal::workspacechat::ChatMessageEntry& entry)
        {
            Json::Value payload{ Json::objectValue };
            payload["windowId"] = Json::UInt64{ entry.WindowId };
            payload["messageId"] = terminal::workspacechat::DiagnosticUtf8(entry.MessageId);
            payload["correlationId"] = terminal::workspacechat::DiagnosticUtf8(entry.CorrelationId);
            payload["workspaceKey"] = terminal::workspacechat::DiagnosticUtf8(entry.WorkspaceId);
            payload["tabId"] = terminal::workspacechat::DiagnosticUtf8(entry.TabId);
            payload["paneId"] = terminal::workspacechat::DiagnosticUtf8(entry.PaneId);
            terminal::workspacechat::AddDiagnosticTextFields(payload, "submittedText", entry.Text);
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"chat_controller_submit", payload);
        }
    }

    class WorkspaceTerminalPageExtension final : public IWorkspaceTerminalPageExtension
    {
    public:
        explicit WorkspaceTerminalPageExtension(TerminalPageBase& host) :
            _host{ host },
            _extCore{}
        {
            _workspaceChatController.SetSubmitDiagnosticHook(&_logWorkspaceChatControllerSubmit);
        }

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

        void SetCurrentWorkspaceId(const winrt::hstring& value) override
        {
            if (_currentWorkspaceId == value)
            {
                return;
            }

            Json::Value payload{ Json::objectValue };
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "previousWorkspaceId", _currentWorkspaceId.c_str());
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "nextWorkspaceId", value.c_str());
            payload["windowId"] = Json::UInt64{ _host.WorkspaceWindowId() };
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_current_id_changed", payload);

            const auto currentBaselineWorkspaceId = _currentWorkspaceSaveBaseline ?
                                                        _currentWorkspaceSaveBaseline->Id :
                                                        std::wstring{};
            const auto changePlan = _extCore.PrepareWorkspaceCurrentIdChange(_currentWorkspaceId.c_str(),
                                                                              value.c_str(),
                                                                              _lastWorkspaceId,
                                                                              currentBaselineWorkspaceId);
            _lastWorkspaceId = changePlan.LastWorkspaceId;
            if (changePlan.ResetSaveBaseline)
            {
                _currentWorkspaceSaveBaseline.reset();
            }

            _currentWorkspaceId = value;
            _host.ConfigureWorkspaceStateHeartbeat(changePlan.StartHeartbeat);
            _host.ApplyWorkspaceCurrentIdChange();
            RefreshWorkspaceWindowState();
        }

        void RefreshWorkspaceWindowState() override
        {
            const auto refreshPlan = _extCore.RefreshWorkspaceWindowState(_host.WorkspaceWindowId(),
                                                                           _host.CurrentWorkspaceId().c_str());
            if (refreshPlan.SkipRefresh)
            {
                Json::Value payload{ Json::objectValue };
                terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", _host.CurrentWorkspaceId().c_str());
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_window_state_refresh_skipped_missing_window", payload);
                return;
            }

            _host.CommitWorkspaceWindowRefresh(refreshPlan.ClearPendingWorkspaceLaunch,
                                               winrt::hstring{ refreshPlan.WorkspaceId });
            Json::Value payload{ Json::objectValue };
            payload["processId"] = Json::UInt64{ refreshPlan.ProcessId };
            payload["refreshed"] = true;
            terminal::workspacechat::AddOptionalDiagnosticString(payload, "workspaceId", refreshPlan.WorkspaceId);
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_window_state_refreshed", payload);
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
            return !_host.CurrentWorkspaceId().empty();
        }

        void OnPreparingStartupActions() override
        {
            if (_startupWorkspaceId.empty())
            {
                return;
            }

            SetCurrentWorkspaceId(_startupWorkspaceId);
            const auto startupPlan = _extCore.PrepareWorkspaceStartup(_startupWorkspaceId.c_str());
            ReplacePendingWorkspaceNodeInputVisibility(startupPlan.PendingNodeInputVisibility);
            ReplacePendingWorkspaceNodeIds(startupPlan.PendingNodeIds);
            _startupWorkspaceId.clear();
        }

        void OnStartupActionsCompleted() override
        {
            ClearPendingWorkspaceNodeQueues();
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

        terminal::workspace::WorkspaceChatUiState& WorkspaceChatUiState() override
        {
            return _workspaceChatUiState;
        }

        const terminal::workspace::WorkspaceChatUiState& WorkspaceChatUiState() const override
        {
            return _workspaceChatUiState;
        }

        terminal::workspace::TerminalCaptureState& GetOrCreateWorkspaceChatTerminalState(const std::wstring_view stateKey) override
        {
            return terminal::workspacechat::GetOrCreateTerminalCaptureState(_workspaceChatTerminalStates, stateKey);
        }

        terminal::workspace::TerminalCaptureState* FindWorkspaceChatTerminalState(const std::wstring_view stateKey) override
        {
            return terminal::workspacechat::FindTerminalCaptureState(_workspaceChatTerminalStates, stateKey);
        }

        const terminal::workspace::TerminalCaptureState* FindWorkspaceChatTerminalState(const std::wstring_view stateKey) const override
        {
            return terminal::workspacechat::FindTerminalCaptureState(_workspaceChatTerminalStates, stateKey);
        }

        void UpsertWorkspaceChatPendingOutputCapture(terminal::workspace::PendingTerminalOutputCapture pendingCapture) override
        {
            terminal::workspacechat::UpsertPendingTerminalOutputCapture(_workspaceChatPendingOutputCaptures, std::move(pendingCapture));
        }

        std::vector<terminal::workspace::PendingTerminalOutputCapture> TakeReadyWorkspaceChatPendingOutputCaptures(const uint64_t now) override
        {
            return terminal::workspacechat::TakeReadyPendingTerminalOutputCaptures(_workspaceChatPendingOutputCaptures, now);
        }

        bool HasWorkspaceChatPendingOutputCaptures() const override
        {
            return !_workspaceChatPendingOutputCaptures.empty();
        }

        terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() override
        {
            return _workspaceChatController;
        }

        const terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() const override
        {
            return _workspaceChatController;
        }

        terminal::workspace::WorkspaceChatSubmitTransport WorkspaceChatSubmitTransport() const override
        {
            return _workspaceChatSubmitTransport;
        }

        void SetWorkspaceChatSubmitTransport(const terminal::workspace::WorkspaceChatSubmitTransport transport) override
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

        int32_t WorkspaceManagerWorkspaceNavSelection(const size_t workspaceIndex) const override
        {
            return _extCore.PrepareWorkspaceManagerNavigation(workspaceIndex, 0, 0, 0, _workspaceManagerNavSelection).WorkspaceSelection;
        }

        int32_t WorkspaceManagerWorkspaceNodeNavSelection(const size_t workspaceIndex, const size_t nodeIndex) const override
        {
            return _extCore.PrepareWorkspaceManagerNavigation(workspaceIndex, nodeIndex, 0, 0, _workspaceManagerNavSelection).WorkspaceNodeSelection;
        }

        int32_t WorkspaceManagerEditorNavSelection(const size_t workspaceCount, const size_t selectedWorkspaceIndex) const override
        {
            return _extCore.PrepareWorkspaceManagerNavigation(0, 0, workspaceCount, selectedWorkspaceIndex, _workspaceManagerNavSelection).EditorSelection;
        }

        std::optional<size_t> ApplyWorkspaceManagerNavSelection(const int32_t navSelection,
                                                                 const size_t workspaceCount,
                                                                 const size_t selectedWorkspaceIndex) override
        {
            const auto plan = _extCore.PrepareWorkspaceManagerNavigation(0, 0, workspaceCount, selectedWorkspaceIndex, navSelection);
            _workspaceManagerNavSelection = navSelection;
            return plan.ResolvedWorkspaceIndex;
        }

        void NavigateWorkspaceManager(const int32_t navSelection,
                                      const size_t workspaceCount,
                                      const size_t selectedWorkspaceIndex,
                                      const bool rebuild) override
        {
            if (const auto workspaceIndex = ApplyWorkspaceManagerNavSelection(navSelection, workspaceCount, selectedWorkspaceIndex))
            {
                _host.SelectWorkspaceManagerWorkspace(*workspaceIndex);
            }
            if (rebuild)
            {
                _host.RefreshWorkspaceManagerContent();
            }
        }

        bool SaveWorkspaceManagerEdits() override
        {
            return _host.SaveWorkspaceManagerEdits();
        }

        void ResetWorkspaceManagerEdits() override
        {
            _workspaceDefinitionsDirty = false;
            _workspaceEditorEditMode = true;
            const auto view = _host.ReloadWorkspaceManagerEdits(false);
            NavigateWorkspaceManager(WorkspaceManagerEditorNavSelection(view.WorkspaceCount, view.SelectedWorkspaceIndex),
                                     view.WorkspaceCount,
                                     view.SelectedWorkspaceIndex,
                                     true);
        }

        WorkspaceManagerMutationView AddWorkspaceManagerDefinition(const std::optional<size_t> templateIndex) override
        {
            return _host.AddWorkspaceManagerDefinition(templateIndex);
        }

        WorkspaceManagerMutationView AddWorkspaceManagerNode(const size_t workspaceIndex) override
        {
            return _host.AddWorkspaceManagerNode(workspaceIndex);
        }

        bool UpdateWorkspaceManagerWorkspaceText(const WorkspaceManagerWorkspaceTextField field, const winrt::hstring& value) override
        {
            return _host.UpdateWorkspaceManagerWorkspaceText(field, value);
        }

        bool UpdateWorkspaceManagerWorkspaceBool(const WorkspaceManagerWorkspaceBoolField field, const bool value) override
        {
            return _host.UpdateWorkspaceManagerWorkspaceBool(field, value);
        }

        bool UpdateWorkspaceManagerDefaultProfile(const winrt::hstring& guid, const winrt::hstring& name) override
        {
            return _host.UpdateWorkspaceManagerDefaultProfile(guid, name);
        }

        winrt::hstring WorkspaceManagerWorkspaceBackgroundColor() const override
        {
            return _host.WorkspaceManagerWorkspaceBackgroundColor();
        }

        winrt::hstring WorkspaceManagerWorkspaceIconForEditing() const override
        {
            return _host.WorkspaceManagerWorkspaceIconForEditing();
        }

        winrt::hstring WorkspaceManagerNodeIconPreviewForEditing(const size_t nodeIndex) const override
        {
            return _host.WorkspaceManagerNodeIconPreviewForEditing(nodeIndex);
        }

        bool ApplyWorkspaceManagerWorkspaceBackgroundColor(const winrt::hstring& color) override
        {
            return _host.ApplyWorkspaceManagerWorkspaceBackgroundColor(color);
        }

        winrt::hstring RotateWorkspaceManagerWorkspaceBackgroundColor() override
        {
            return _host.RotateWorkspaceManagerWorkspaceBackgroundColor();
        }

        bool UpdateWorkspaceManagerNodeText(const size_t nodeIndex, const WorkspaceManagerNodeTextField field, const winrt::hstring& value) override
        {
            return _host.UpdateWorkspaceManagerNodeText(nodeIndex, field, value);
        }

        bool UpdateWorkspaceManagerNodeBool(const size_t nodeIndex, const WorkspaceManagerNodeBoolField field, const bool value) override { return _host.UpdateWorkspaceManagerNodeBool(nodeIndex, field, value); }
        bool UpdateWorkspaceManagerNodeProfile(const size_t nodeIndex, const winrt::hstring& guid, const winrt::hstring& name) override { return _host.UpdateWorkspaceManagerNodeProfile(nodeIndex, guid, name); }
        bool ReorderWorkspaceManagerVisibleNodes(const std::vector<winrt::hstring>& orderedNodeIds) override { return _host.ReorderWorkspaceManagerVisibleNodes(orderedNodeIds); }
        std::vector<WorkspaceProfileOption> WorkspaceManagerProfileOptionsForEditing(const winrt::hstring& currentGuid, const winrt::hstring& currentName) const override
        {
            return _host.WorkspaceManagerProfileOptionsForEditing(currentGuid, currentName);
        }
        winrt::hstring WorkspaceManagerNodeTabColor(const size_t nodeIndex) const override { return _host.WorkspaceManagerNodeTabColor(nodeIndex); }
        winrt::hstring WorkspaceManagerNodeTabColorPreview(const size_t nodeIndex) const override { return _host.WorkspaceManagerNodeTabColorPreview(nodeIndex); }
        bool ApplyWorkspaceManagerNodeTabColor(const size_t nodeIndex, const winrt::hstring& color) override { return _host.ApplyWorkspaceManagerNodeTabColor(nodeIndex, color); }
        bool RotateWorkspaceManagerNodeTabColor(const size_t nodeIndex) override { return _host.RotateWorkspaceManagerNodeTabColor(nodeIndex); }

        std::optional<size_t> ResolveWorkspaceManagerWorkspaceIndex(const int32_t navSelection) const override
        {
            return _extCore.PrepareWorkspaceManagerNavigation(0, 0, 0, 0, navSelection).ResolvedWorkspaceIndex;
        }

        std::optional<size_t> ResolveWorkspaceManagerNodeIndex(const int32_t navSelection) const override
        {
            return _extCore.PrepareWorkspaceManagerNavigation(0, 0, 0, 0, navSelection).ResolvedNodeIndex;
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

        winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings WorkspaceSettings() const override
        {
            return _host.WorkspaceSettings();
        }

        void UpsertWorkspaceNodeRuntimeState(const uint64_t contentId, const terminal::workspace::WorkspaceNodeRuntimeState& state) override
        {
            _workspaceNodeRuntimeStates[contentId] = state;
        }

        void RemoveWorkspaceNodeRuntimeState(const uint64_t contentId) override
        {
            _workspaceNodeRuntimeStates.erase(contentId);
        }

        terminal::workspace::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(const uint64_t contentId) override
        {
            if (const auto it = _workspaceNodeRuntimeStates.find(contentId); it != _workspaceNodeRuntimeStates.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        const terminal::workspace::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(const uint64_t contentId) const override
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

        winrt::TerminalApp::IPaneContent CreateWorkspaceManagerPaneContent(
            const winrt::Windows::UI::Xaml::UIElement& content,
            const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings) override
        {
            auto pane = winrt::make_self<winrt::TerminalApp::implementation::WorkspaceManagerPaneContent>(content, settings);
            return pane.as<winrt::TerminalApp::IPaneContent>();
        }

        void UpdateWorkspaceManagerPaneContent(
            const winrt::TerminalApp::IPaneContent& paneContent,
            const winrt::Windows::UI::Xaml::UIElement& content) override
        {
            winrt::get_self<winrt::TerminalApp::implementation::WorkspaceManagerPaneContent>(paneContent)->UpdateContent(content);
        }

        void UpdateWorkspaceManagerPaneSettings(
            const winrt::TerminalApp::IPaneContent& paneContent,
            const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings) override
        {
            winrt::get_self<winrt::TerminalApp::implementation::WorkspaceManagerPaneContent>(paneContent)->UpdateSettings(settings);
        }

        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerIcon(
            std::wstring initialIcon,
            const std::optional<size_t> nodeIndex) override
        {
            co_return co_await terminal::workspace::PickWorkspaceManagerIcon(_host, std::move(initialIcon), nodeIndex);
        }

        winrt::Windows::Foundation::IAsyncAction ShowWorkspaceManagerWorkspaceIconPicker() override
        {
            const auto selected = co_await PickWorkspaceManagerIcon(_host.WorkspaceManagerWorkspaceIconForEditing().c_str(), std::nullopt);
            if (!selected.empty())
            {
                _host.ApplyWorkspaceManagerWorkspaceIcon(selected);
            }
        }

        winrt::Windows::Foundation::IAsyncAction ShowWorkspaceManagerNodeIconPicker(const size_t nodeIndex) override
        {
            Json::Value startPayload{ Json::objectValue };
            startPayload["nodeIndex"] = nodeIndex;
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_click", startPayload);

            const auto previous = _host.WorkspaceManagerNodeIconForEditing(nodeIndex);
            const auto selected = co_await PickWorkspaceManagerIcon(previous.c_str(), nodeIndex);
            Json::Value payload{ Json::objectValue };
            payload["nodeIndex"] = nodeIndex;
            terminal::workspacechat::AddDiagnosticTextFields(payload, "selectedIcon", selected.c_str());
            terminal::workspacechat::AddDiagnosticTextFields(payload, "previousIcon", previous.c_str());
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_selected", payload);
            if (selected == previous)
            {
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_noop_same_icon", payload);
            }
            else if (!selected.empty())
            {
                _host.ApplyWorkspaceManagerNodeIcon(nodeIndex, selected);
            }
        }

        winrt::Windows::Foundation::IAsyncAction DeleteWorkspaceManagerWorkspace(winrt::hstring workspaceId) override
        {
            if (co_await ConfirmWorkspaceManagerDeletion(false))
            {
                _host.RemoveWorkspaceManagerWorkspace(workspaceId);
            }
        }

        winrt::Windows::Foundation::IAsyncAction DeleteWorkspaceManagerNode(winrt::hstring workspaceId, winrt::hstring nodeId) override
        {
            if (co_await ConfirmWorkspaceManagerDeletion(true))
            {
                _host.RemoveWorkspaceManagerNode(workspaceId, nodeId);
            }
        }

        winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmWorkspaceManagerDeletion(const bool deletingNode) override
        {
            co_return co_await terminal::workspace::ConfirmWorkspaceManagerDeletion(_host, deletingNode);
        }

        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerPath(const bool pickFolder) override
        {
            co_return co_await terminal::workspace::PickWorkspaceManagerPath(_host, pickFolder);
        }

        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerColor(std::wstring initialColor) override
        {
            co_return co_await terminal::workspace::PickWorkspaceManagerColor(_host, std::move(initialColor));
        }

        winrt::Windows::UI::Xaml::Controls::ComboBox CreateWorkspaceManagerProfilePicker(
            const std::vector<WorkspaceProfileOption>& profiles,
            std::wstring selectedProfileGuid,
            const bool enabled) override
        {
            return terminal::workspace::CreateWorkspaceManagerProfilePicker(profiles, std::move(selectedProfileGuid), enabled);
        }

    private:
        TerminalPageBase& _host;
        WorkspaceExtLoader _extCore;
        std::deque<bool> _pendingWorkspaceNodeInputVisibility;
        std::deque<std::wstring> _pendingWorkspaceNodeIds;
        terminal::workspace::WorkspaceChatUiState _workspaceChatUiState;
        std::unordered_map<std::wstring, terminal::workspace::TerminalCaptureState> _workspaceChatTerminalStates;
        std::vector<terminal::workspace::PendingTerminalOutputCapture> _workspaceChatPendingOutputCaptures;
        terminal::workspacechat::WorkspaceChatController _workspaceChatController;
        terminal::workspace::WorkspaceChatSubmitTransport _workspaceChatSubmitTransport{ terminal::workspace::WorkspaceChatSubmitTransport::SendInputInline };
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
        std::unordered_map<uint64_t, terminal::workspace::WorkspaceNodeRuntimeState> _workspaceNodeRuntimeStates;
        std::optional<std::wstring> _pendingWorkspaceNodeStartupAction;
        bool _skipNextWorkspaceNodeStartupSendInput{ false };
    };

    extern "C" IGlueTerminalPageExtension* WINAPI CreateGlueTerminalPageExtension(TerminalPageBase* host)
    {
        if (!host)
        {
            return nullptr;
        }

        return new WorkspaceTerminalPageExtension(*host);
    }

    extern "C" void WINAPI DestroyGlueTerminalPageExtension(IGlueTerminalPageExtension* extension)
    {
        delete extension;
    }

    extern "C" IWorkspaceTerminalPageExtension* WINAPI CreateWorkspaceTerminalPageExtension(TerminalPageBase* host)
    {
        return CreateGlueTerminalPageExtension(host);
    }

    extern "C" void WINAPI DestroyWorkspaceTerminalPageExtension(IWorkspaceTerminalPageExtension* extension)
    {
        DestroyGlueTerminalPageExtension(extension);
    }
}
