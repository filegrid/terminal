// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

// Neutral host/extension ABI. This header is intentionally independent of the
// Glue implementation directory so TerminalApp and Glue have no physical
// reverse include relationship.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/chat/WorkspaceChatStateHelpers.h"
#include "GluePageStateTypes.h"
#include <winrt/base.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include "GluePageContractApi.h"

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
    struct Tab;
}

namespace winrt::TerminalApp
{
    struct IPaneContent;
}

namespace terminal::workspacechat
{
    class WorkspaceChatController;
}

namespace winrt::Microsoft::Terminal::Settings::Model
{
    struct ActionAndArgs;
    struct CascadiaSettings;
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
    struct UIElement;
}

namespace terminal::workspace
{
    struct WorkspaceProfileOption
    {
        winrt::hstring Guid;
        winrt::hstring DisplayName;
    };

    // Scalar snapshot crosses the DLL boundary; the Settings implementation remains Glue-owned.
    struct WorkspaceManagerEditorView
    {
        size_t WorkspaceCount{};
        size_t SelectedWorkspaceIndex{};
    };

    struct WorkspaceManagerMutationView
    {
        bool Changed{};
        size_t WorkspaceCount{};
        size_t SelectedWorkspaceIndex{};
        size_t SelectedWorkspaceNodeCount{};
    };

    enum class WorkspaceManagerWorkspaceTextField : uint8_t
    {
        Name,
        Description,
        DefaultStartupDirectory,
    };

    enum class WorkspaceManagerWorkspaceBoolField : uint8_t
    {
        DefaultShowInputPanel,
        DefaultUseNodeNameAsTabTitle,
        DefaultShowTab,
    };

    enum class WorkspaceManagerNodeTextField : uint8_t
    {
        Name,
        StartupDirectory,
        StartupAction,
    };

    enum class WorkspaceManagerNodeBoolField : uint8_t
    {
        ShowTab,
        ShowInputPanel,
        UseNodeNameAsTabTitle,
    };

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
        virtual winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Xaml::Controls::ContentDialogResult> ShowWorkspaceDialog(
            winrt::Windows::UI::Xaml::Controls::ContentDialog dialog) = 0;
        virtual winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspacePath(bool pickFolder) = 0;
        virtual winrt::hstring WorkspaceManagerWorkspaceIconForEditing() const = 0;
        virtual winrt::hstring WorkspaceManagerNodeIconForEditing(size_t nodeIndex) const = 0;
        // Resolves an explicit node icon, then its selected profile icon. Glue only renders this value.
        virtual winrt::hstring WorkspaceManagerNodeIconPreviewForEditing(size_t nodeIndex) const = 0;
        virtual void ApplyWorkspaceManagerWorkspaceIcon(const winrt::hstring& icon) = 0;
        virtual void ApplyWorkspaceManagerNodeIcon(size_t nodeIndex, const winrt::hstring& icon) = 0;
        virtual bool RemoveWorkspaceManagerWorkspace(const winrt::hstring& workspaceId) = 0;
        virtual bool RemoveWorkspaceManagerNode(const winrt::hstring& workspaceId, const winrt::hstring& nodeId) = 0;
        virtual void SelectWorkspaceManagerWorkspace(size_t index) = 0;
        virtual void RefreshWorkspaceManagerContent() = 0;
        virtual bool SaveWorkspaceManagerEdits() = 0;
        virtual WorkspaceManagerEditorView ReloadWorkspaceManagerEdits(bool preserveSelection) = 0;
        virtual WorkspaceManagerMutationView AddWorkspaceManagerDefinition(std::optional<size_t> templateIndex) = 0;
        virtual WorkspaceManagerMutationView AddWorkspaceManagerNode(size_t workspaceIndex) = 0;
        virtual bool UpdateWorkspaceManagerWorkspaceText(WorkspaceManagerWorkspaceTextField field, const winrt::hstring& value) = 0;
        virtual bool UpdateWorkspaceManagerWorkspaceBool(WorkspaceManagerWorkspaceBoolField field, bool value) = 0;
        virtual bool UpdateWorkspaceManagerDefaultProfile(const winrt::hstring& guid, const winrt::hstring& name) = 0;
        virtual winrt::hstring WorkspaceManagerWorkspaceBackgroundColor() const = 0;
        virtual bool ApplyWorkspaceManagerWorkspaceBackgroundColor(const winrt::hstring& color) = 0;
        virtual winrt::hstring RotateWorkspaceManagerWorkspaceBackgroundColor() = 0;
        virtual bool UpdateWorkspaceManagerNodeText(size_t nodeIndex, WorkspaceManagerNodeTextField field, const winrt::hstring& value) = 0;
        virtual bool UpdateWorkspaceManagerNodeBool(size_t nodeIndex, WorkspaceManagerNodeBoolField field, bool value) = 0;
        virtual bool UpdateWorkspaceManagerNodeProfile(size_t nodeIndex, const winrt::hstring& guid, const winrt::hstring& name) = 0;
        virtual bool ReorderWorkspaceManagerVisibleNodes(const std::vector<winrt::hstring>& orderedNodeIds) = 0;
        virtual winrt::hstring WorkspaceManagerNodeTabColor(size_t nodeIndex) const = 0;
        // Resolved through Settings in the host; Glue renders the resulting color text only.
        virtual winrt::hstring WorkspaceManagerNodeTabColorPreview(size_t nodeIndex) const = 0;
        virtual bool ApplyWorkspaceManagerNodeTabColor(size_t nodeIndex, const winrt::hstring& color) = 0;
        virtual bool RotateWorkspaceManagerNodeTabColor(size_t nodeIndex) = 0;
        virtual std::vector<WorkspaceProfileOption> WorkspaceManagerProfileOptionsForEditing(const winrt::hstring& currentGuid,
                                                                                               const winrt::hstring& currentName) const = 0;
        // Value return keeps the host's mutable settings field and implementation details private.
        virtual winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings WorkspaceSettings() const = 0;
        virtual winrt::hstring CurrentWorkspaceId() const noexcept = 0;
        virtual uint64_t WorkspaceWindowId() const noexcept = 0;
        virtual void CommitWorkspaceWindowRefresh(bool clearPendingWorkspaceLaunch,
                                                  const winrt::hstring& workspaceId) = 0;
        virtual void ConfigureWorkspaceStateHeartbeat(bool start) = 0;
        virtual void ApplyWorkspaceCurrentIdChange() = 0;
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
        virtual void SetCurrentWorkspaceId(const winrt::hstring& value) = 0;
        virtual void RefreshWorkspaceWindowState() = 0;
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
        virtual terminal::workspace::WorkspaceChatUiState& WorkspaceChatUiState() = 0;
        virtual const terminal::workspace::WorkspaceChatUiState& WorkspaceChatUiState() const = 0;
        virtual terminal::workspace::TerminalCaptureState& GetOrCreateWorkspaceChatTerminalState(std::wstring_view stateKey) = 0;
        virtual terminal::workspace::TerminalCaptureState* FindWorkspaceChatTerminalState(std::wstring_view stateKey) = 0;
        virtual const terminal::workspace::TerminalCaptureState* FindWorkspaceChatTerminalState(std::wstring_view stateKey) const = 0;
        virtual void UpsertWorkspaceChatPendingOutputCapture(terminal::workspace::PendingTerminalOutputCapture pendingCapture) = 0;
        virtual std::vector<terminal::workspace::PendingTerminalOutputCapture> TakeReadyWorkspaceChatPendingOutputCaptures(uint64_t now) = 0;
        virtual bool HasWorkspaceChatPendingOutputCaptures() const = 0;
        virtual terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() = 0;
        virtual const terminal::workspacechat::WorkspaceChatController& WorkspaceChatController() const = 0;
        virtual terminal::workspace::WorkspaceChatSubmitTransport WorkspaceChatSubmitTransport() const = 0;
        virtual void SetWorkspaceChatSubmitTransport(terminal::workspace::WorkspaceChatSubmitTransport transport) = 0;
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
        virtual int32_t WorkspaceManagerWorkspaceNavSelection(size_t workspaceIndex) const = 0;
        virtual int32_t WorkspaceManagerWorkspaceNodeNavSelection(size_t workspaceIndex, size_t nodeIndex) const = 0;
        virtual int32_t WorkspaceManagerEditorNavSelection(size_t workspaceCount, size_t selectedWorkspaceIndex) const = 0;
        virtual std::optional<size_t> ApplyWorkspaceManagerNavSelection(int32_t navSelection,
                                                                         size_t workspaceCount,
                                                                         size_t selectedWorkspaceIndex) = 0;
        virtual void NavigateWorkspaceManager(int32_t navSelection,
                                              size_t workspaceCount,
                                              size_t selectedWorkspaceIndex,
                                              bool rebuild = true) = 0;
        virtual bool SaveWorkspaceManagerEdits() = 0;
        virtual void ResetWorkspaceManagerEdits() = 0;
        virtual WorkspaceManagerMutationView AddWorkspaceManagerDefinition(std::optional<size_t> templateIndex) = 0;
        virtual WorkspaceManagerMutationView AddWorkspaceManagerNode(size_t workspaceIndex) = 0;
        virtual bool UpdateWorkspaceManagerWorkspaceText(WorkspaceManagerWorkspaceTextField field, const winrt::hstring& value) = 0;
        virtual bool UpdateWorkspaceManagerWorkspaceBool(WorkspaceManagerWorkspaceBoolField field, bool value) = 0;
        virtual bool UpdateWorkspaceManagerDefaultProfile(const winrt::hstring& guid, const winrt::hstring& name) = 0;
        virtual winrt::hstring WorkspaceManagerWorkspaceBackgroundColor() const = 0;
        virtual winrt::hstring WorkspaceManagerWorkspaceIconForEditing() const = 0;
        virtual winrt::hstring WorkspaceManagerNodeIconPreviewForEditing(size_t nodeIndex) const = 0;
        virtual bool ApplyWorkspaceManagerWorkspaceBackgroundColor(const winrt::hstring& color) = 0;
        virtual winrt::hstring RotateWorkspaceManagerWorkspaceBackgroundColor() = 0;
        virtual bool UpdateWorkspaceManagerNodeText(size_t nodeIndex, WorkspaceManagerNodeTextField field, const winrt::hstring& value) = 0;
        virtual bool UpdateWorkspaceManagerNodeBool(size_t nodeIndex, WorkspaceManagerNodeBoolField field, bool value) = 0;
        virtual bool UpdateWorkspaceManagerNodeProfile(size_t nodeIndex, const winrt::hstring& guid, const winrt::hstring& name) = 0;
        virtual bool ReorderWorkspaceManagerVisibleNodes(const std::vector<winrt::hstring>& orderedNodeIds) = 0;
        virtual winrt::hstring WorkspaceManagerNodeTabColor(size_t nodeIndex) const = 0;
        virtual winrt::hstring WorkspaceManagerNodeTabColorPreview(size_t nodeIndex) const = 0;
        virtual bool ApplyWorkspaceManagerNodeTabColor(size_t nodeIndex, const winrt::hstring& color) = 0;
        virtual bool RotateWorkspaceManagerNodeTabColor(size_t nodeIndex) = 0;
        virtual std::vector<WorkspaceProfileOption> WorkspaceManagerProfileOptionsForEditing(const winrt::hstring& currentGuid,
                                                                                               const winrt::hstring& currentName) const = 0;
        virtual std::optional<size_t> ResolveWorkspaceManagerWorkspaceIndex(int32_t navSelection) const = 0;
        virtual std::optional<size_t> ResolveWorkspaceManagerNodeIndex(int32_t navSelection) const = 0;
        virtual bool& WorkspaceEditorEditMode() = 0;
        virtual const bool& WorkspaceEditorEditMode() const = 0;
        virtual bool& WorkspaceDefinitionsDirty() = 0;
        virtual const bool& WorkspaceDefinitionsDirty() const = 0;
        virtual winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings WorkspaceSettings() const = 0;
        virtual void UpsertWorkspaceNodeRuntimeState(uint64_t contentId, const terminal::workspace::WorkspaceNodeRuntimeState& state) = 0;
        virtual void RemoveWorkspaceNodeRuntimeState(uint64_t contentId) = 0;
        virtual terminal::workspace::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(uint64_t contentId) = 0;
        virtual const terminal::workspace::WorkspaceNodeRuntimeState* FindWorkspaceNodeRuntimeState(uint64_t contentId) const = 0;
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

        // The host owns tab placement; Glue owns the workspace manager page
        // implementation behind this WinRT interface.
        virtual winrt::TerminalApp::IPaneContent CreateWorkspaceManagerPaneContent(
            const winrt::Windows::UI::Xaml::UIElement& content,
            const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings) = 0;
        virtual void UpdateWorkspaceManagerPaneContent(
            const winrt::TerminalApp::IPaneContent& paneContent,
            const winrt::Windows::UI::Xaml::UIElement& content) = 0;
        virtual void UpdateWorkspaceManagerPaneSettings(
            const winrt::TerminalApp::IPaneContent& paneContent,
            const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings) = 0;
        virtual winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerIcon(
            std::wstring initialIcon,
            std::optional<size_t> nodeIndex) = 0;
        virtual winrt::Windows::Foundation::IAsyncAction ShowWorkspaceManagerWorkspaceIconPicker() = 0;
        virtual winrt::Windows::Foundation::IAsyncAction ShowWorkspaceManagerNodeIconPicker(size_t nodeIndex) = 0;
        virtual winrt::Windows::Foundation::IAsyncAction DeleteWorkspaceManagerWorkspace(winrt::hstring workspaceId) = 0;
        virtual winrt::Windows::Foundation::IAsyncAction DeleteWorkspaceManagerNode(winrt::hstring workspaceId, winrt::hstring nodeId) = 0;
        virtual winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmWorkspaceManagerDeletion(bool deletingNode) = 0;
        virtual winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerPath(bool pickFolder) = 0;
        virtual winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerColor(
            std::wstring initialColor) = 0;
        virtual winrt::Windows::UI::Xaml::Controls::ComboBox CreateWorkspaceManagerProfilePicker(
            const std::vector<WorkspaceProfileOption>& profiles,
            std::wstring selectedProfileGuid,
            bool enabled) = 0;
    };

    using CreateWorkspaceTerminalPageExtensionFn = IWorkspaceTerminalPageExtension* (WINAPI*)(TerminalPageBase* host);
    using DestroyWorkspaceTerminalPageExtensionFn = void (WINAPI*)(IWorkspaceTerminalPageExtension* extension);

    // Glue is a generic page container. The legacy Workspace names stay available
    // while its first page is migrated, but TerminalApp consumes only this generic
    // factory contract so future pages use the same container boundary.
    using IGlueTerminalPageExtension = IWorkspaceTerminalPageExtension;
    using CreateGlueTerminalPageExtensionFn = IGlueTerminalPageExtension* (WINAPI*)(TerminalPageBase* host);
    using DestroyGlueTerminalPageExtensionFn = void (WINAPI*)(IGlueTerminalPageExtension* extension);

    inline constexpr char CreateGlueTerminalPageExtensionSymbol[] = "CreateGlueTerminalPageExtension";
    inline constexpr char DestroyGlueTerminalPageExtensionSymbol[] = "DestroyGlueTerminalPageExtension";
    inline constexpr char CreateWorkspaceTerminalPageExtensionSymbol[] = "CreateWorkspaceTerminalPageExtension";
    inline constexpr char DestroyWorkspaceTerminalPageExtensionSymbol[] = "DestroyWorkspaceTerminalPageExtension";

    extern "C"
    {
        WT_WORKSPACE_EXT_API IGlueTerminalPageExtension* WINAPI CreateGlueTerminalPageExtension(TerminalPageBase* host);
        WT_WORKSPACE_EXT_API void WINAPI DestroyGlueTerminalPageExtension(IGlueTerminalPageExtension* extension);
        WT_WORKSPACE_EXT_API IWorkspaceTerminalPageExtension* WINAPI CreateWorkspaceTerminalPageExtension(TerminalPageBase* host);
        WT_WORKSPACE_EXT_API void WINAPI DestroyWorkspaceTerminalPageExtension(IWorkspaceTerminalPageExtension* extension);
    }
}
