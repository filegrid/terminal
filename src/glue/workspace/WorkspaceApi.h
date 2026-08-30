// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "WorkspaceDataTypes.h"
#include "WorkspaceRuntimeTypes.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <winrt/Microsoft.Terminal.Settings.Model.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    const std::vector<std::wstring_view>& WorkspaceColorPalette() noexcept;
    std::wstring PickWorkspacePaletteColor(const std::unordered_set<std::wstring>& usedColors,
                                           size_t fallbackIndex,
                                           std::wstring_view excludedColor = {});

    class WorkspaceManager;

    // A command launch belongs to one workspace node. The host uses this to
    // create child terminal hosts without turning commands into first-level
    // TerminalApp::Tab instances.
    struct WorkspaceNodeCommandLaunch
    {
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs TerminalArgs;
        std::wstring StartupInput;
    };

    class WorkspaceManager
    {
    public:
        static std::filesystem::path DefaultPath();
        static WorkspaceManager Load();
        static WorkspaceManager LoadFromPath(const std::filesystem::path& path);

        bool Save() const;
        bool SaveToPath(const std::filesystem::path& path) const;

        const Workspace* FindById(std::wstring_view id) const noexcept;
        bool ReorderWorkspaceNodes(std::wstring_view workspaceId, const std::vector<std::wstring>& orderedNodeIds);
        std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> BuildStartupActions(const Workspace& workspace, const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings) const;
        std::vector<WorkspaceNodeCommandLaunch> BuildNodeCommandLaunches(const Workspace& workspace,
                                                                          size_t nodeIndex,
                                                                          const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings) const;

        std::vector<Workspace>& Workspaces() noexcept;
        const std::vector<Workspace>& Workspaces() const noexcept;
        void SetWorkspaces(std::vector<Workspace> workspaces);

    private:
        std::vector<Workspace> _workspaces;
    };

    class WorkspaceStateManager;

    #include "WorkspaceEditorTypes.h"

    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes);
    bool MoveWorkspaceManagerVisibleNode(Workspace& workspace, size_t nodeIndex, int offset);
    bool ApplyWorkspaceManagerNodeTemplate(Workspace& workspace, size_t templateIndex, size_t newNodeIndex);
    std::optional<Workspace> PrepareWorkspaceForCapture(const std::optional<Workspace>& currentWorkspaceDefinition,
                                                        std::vector<WorkspaceNode> capturedNodes);
    std::optional<Workspace> ResolveWorkspaceDefinition(std::wstring_view currentWorkspaceId,
                                                        const std::optional<Workspace>& selectedWorkspace,
                                                        const WorkspaceManager& manager);
    bool WorkspaceNodeLoadsTab(const WorkspaceNode& node) noexcept;
    std::optional<WorkspaceNode> FindWorkspaceNodeById(const Workspace& workspace, std::wstring_view nodeId);
    std::optional<WorkspaceNode> FindWorkspaceNodeById(const WorkspaceManager& manager, std::wstring_view workspaceId, std::wstring_view nodeId);
    std::optional<WorkspaceNode> ResolveCurrentWorkspaceNode(std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace,
                                                             const WorkspaceManager& manager,
                                                             std::wstring_view nodeId);
    std::optional<size_t> FindWorkspaceNodeIndexById(const Workspace& workspace, std::wstring_view nodeId);
    std::optional<size_t> FindWorkspaceVisibleNodeIndex(const Workspace& workspace, size_t visibleOrdinal);
    std::optional<size_t> ResolveWorkspaceBackedNodeIndex(const std::optional<Workspace>& workspaceDefinition,
                                                          std::wstring_view runtimeNodeId,
                                                          std::optional<size_t> visibleOrdinal);
    std::optional<WorkspaceNode> ResolveWorkspaceBackedNode(const std::optional<Workspace>& workspaceDefinition,
                                                            std::wstring_view runtimeNodeId,
                                                            std::optional<size_t> visibleOrdinal);
    std::vector<std::wstring> VisibleWorkspaceNodeIds(const Workspace& workspace);
    std::vector<bool> VisibleWorkspaceNodeInputVisibility(const Workspace& workspace);
    WorkspaceStartupState ResolveWorkspaceStartupState(const Workspace& workspace,
                                                       const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);
    int32_t WorkspaceManagerNavSelectionForWorkspace(size_t workspaceIndex) noexcept;
    int32_t WorkspaceManagerNavSelectionForWorkspaceNode(size_t workspaceIndex, size_t nodeIndex) noexcept;
    std::optional<size_t> ResolveWorkspaceIndexFromManagerNavSelection(int32_t navSelection) noexcept;
    std::optional<size_t> ResolveWorkspaceNodeIndexFromManagerNavSelection(int32_t navSelection) noexcept;
    int32_t ResolveWorkspaceManagerNavSelectionForEditor(size_t workspaceCount, size_t selectedWorkspaceIndex) noexcept;
    int32_t ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(int32_t previousNavSelection,
                                                                     std::wstring_view selectedWorkspaceId,
                                                                     std::wstring_view removedWorkspaceId,
                                                                     size_t removedWorkspaceIndex,
                                                                     size_t remainingWorkspaceCount) noexcept;
    int32_t ResolveWorkspaceManagerNavSelectionAfterNodeRemoval(int32_t previousNavSelection,
                                                                std::wstring_view selectedWorkspaceId,
                                                                std::wstring_view workspaceId,
                                                                size_t selectedWorkspaceIndex,
                                                                size_t removedNodeIndex) noexcept;
    size_t ResolveWorkspaceEditorSelectedIndex(const WorkspaceManager& manager,
                                               std::wstring_view selectedWorkspaceId,
                                               std::wstring_view currentWorkspaceId,
                                               size_t fallbackIndex) noexcept;
    WorkspaceEditorLoadState LoadWorkspaceEditorState(std::wstring_view selectedWorkspaceId,
                                                      std::wstring_view currentWorkspaceId,
                                                      size_t fallbackIndex);
    WorkspaceEditorSavePlan PrepareWorkspaceEditorForSave(WorkspaceManager& editedManager,
                                                          const WorkspaceManager& persistedManager,
                                                          std::wstring_view currentWorkspaceId,
                                                          std::wstring_view lastOpenedWorkspaceId,
                                                          size_t fallbackSelectedWorkspaceIndex);
    std::optional<WorkspaceEditorDefinitionRemovalPlan> PrepareWorkspaceDefinitionRemoval(WorkspaceManager& manager,
                                                                                          std::wstring_view workspaceId,
                                                                                          std::wstring_view selectedWorkspaceId,
                                                                                          std::wstring_view currentWorkspaceId,
                                                                                          size_t fallbackSelectedWorkspaceIndex,
                                                                                          int32_t previousNavSelection,
                                                                                          std::wstring_view lastOpenedWorkspaceId);
    WorkspaceEditorNodeRemovalPlan PrepareWorkspaceNodeRemoval(WorkspaceManager& manager,
                                                               std::wstring_view workspaceId,
                                                               std::wstring_view nodeId,
                                                               std::wstring_view selectedWorkspaceId,
                                                               std::wstring_view currentWorkspaceId,
                                                               size_t fallbackSelectedWorkspaceIndex,
                                                               int32_t previousNavSelection,
                                                               std::wstring_view lastOpenedWorkspaceId);
    WorkspaceCurrentState ResolveWorkspaceCurrentState(std::wstring_view currentWorkspaceId,
                                                       const WorkspaceManager& manager,
                                                       std::wstring_view defaultDisplayName,
                                                       std::wstring_view unsavedTabRowName);
    WorkspaceStartupState PrepareWorkspaceStartupState(std::wstring_view workspaceId, const WorkspaceManager& manager);
    WorkspaceFlyoutState BuildWorkspaceFlyoutState(std::wstring_view currentWorkspaceId,
                                                   const WorkspaceManager& manager,
                                                   const WorkspaceStateManager& stateManager);
    LoadedWorkspaceFlyoutState LoadWorkspaceFlyoutState(std::wstring_view currentWorkspaceId);
    LoadedWorkspaceOpenState LoadWorkspaceOpenState(std::wstring_view workspaceId,
                                                    bool openInNewWindow,
                                                    std::wstring_view currentWorkspaceId,
                                                    bool currentWorkspaceNeedsSave);
    LoadedWorkspaceOpenExecutionState LoadWorkspaceOpenExecutionState(std::wstring_view workspaceId,
                                                                      bool openInNewWindow,
                                                                      std::wstring_view currentWorkspaceId,
                                                                      bool currentWorkspaceNeedsSave,
                                                                      bool hasTabsToReplace,
                                                                      const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);
    std::optional<Workspace> LoadWorkspaceDefinition(std::wstring_view workspaceId);
    std::optional<Workspace> LoadResolvedWorkspaceDefinition(std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace);
    std::optional<WorkspaceNode> LoadResolvedWorkspaceNode(std::wstring_view currentWorkspaceId,
                                                           const std::optional<Workspace>& selectedWorkspace,
                                                           std::wstring_view nodeId);
    WorkspaceCurrentState LoadCurrentWorkspaceState(std::wstring_view currentWorkspaceId,
                                                    std::wstring_view defaultDisplayName,
                                                    std::wstring_view unsavedTabRowName);
    WorkspaceSaveTargetState LoadWorkspaceSaveTargetState(std::wstring_view currentWorkspaceId,
                                                          std::wstring_view lastWorkspaceId);
    WorkspaceStartupState LoadWorkspaceStartupState(std::wstring_view workspaceId);
    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromProfile(std::wstring_view source);
    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromCommandline(std::wstring_view value);
    bool IsWorkspaceSshCommandline(std::wstring_view value);
    bool HasWorkspaceSshTtyOption(std::wstring_view commandline);
    bool IsWorkspaceSshTransport(std::wstring_view profileSource,
                                 std::wstring_view profileCommandline,
                                 std::wstring_view commandline);
    WorkspaceRuntimeLaunchState PrepareWorkspaceRuntimeLaunchState(std::wstring_view startingDirectory,
                                                                   std::wstring_view profileSource,
                                                                   std::wstring_view profileCommandline,
                                                                   std::wstring_view commandline);
    WorkspaceNodeLaunchResolution ResolveWorkspaceNodeLaunchResolution(const WorkspaceNodeLaunchResolutionInput& input);
    WorkspaceNodeLaunchResolution ResolveWorkspaceNodeLaunchResolution(const WorkspaceNodeLaunchResolutionPlanInput& input);
    std::wstring ResolveTrackedWorkspaceDirectory(const WorkspaceTrackedDirectoryInput& input);
    bool IsWorkspaceDirty(const Workspace& capturedWorkspace,
                          std::wstring_view currentWorkspaceId,
                          const std::optional<Workspace>& baselineWorkspace,
                          const std::optional<Workspace>& persistedWorkspace);
    std::wstring SanitizeWorkspaceDirectoryName(std::wstring_view value, std::wstring_view fallback = {}) noexcept;
    std::wstring NormalizeWorkspaceColor(std::wstring_view color) noexcept;
    std::wstring PickUnusedWorkspaceColor(const std::vector<Workspace>& workspaces);
    std::wstring MakeUniquePersistedName(std::wstring_view baseName, std::unordered_set<std::wstring>& usedNames);
    void NormalizeWorkspacePersistableNames(Workspace& workspace);
    bool WorkspaceNodeEquivalent(const WorkspaceNode& lhs, const WorkspaceNode& rhs);
    bool WorkspaceLayoutEquivalent(const Workspace& lhs, const Workspace& rhs);
    WorkspaceSavePlan PrepareWorkspaceForSave(const Workspace& capturedWorkspace,
                                              const std::vector<Workspace>& existingWorkspaces,
                                              std::wstring_view targetWorkspaceId,
                                              std::wstring_view explicitWorkspaceName,
                                              std::wstring_view fallbackWindowName,
                                              std::wstring_view fallbackTargetName,
                                              std::wstring_view fallbackSingleTabTitle,
                                              std::wstring_view generatedFallbackName);
    WorkspaceOpenPlan PrepareWorkspaceForOpen(std::wstring_view workspaceId,
                                              bool openInNewWindow,
                                              std::wstring_view currentWorkspaceId,
                                              bool currentWorkspaceNeedsSave,
                                              const WorkspaceManager& manager,
                                              const WorkspaceStateManager& stateManager);
    WorkspaceOpenExecutionPlan ResolveWorkspaceOpenExecutionPlan(const WorkspaceOpenPlan& openPlan,
                                                                 bool hasStartupActions,
                                                                 bool hasTabsToReplace);
    WorkspaceNodeRuntimeStatePlan PrepareWorkspaceNodeRuntimeState(const WorkspaceNodeRuntimeRegistrationInput& input);
    WorkspaceSshStartupPlan PrepareSshStartupPlan(std::wstring_view pendingStartupAction,
                                                  std::wstring_view startingDirectory,
                                                  std::wstring_view operatingSystem,
                                                  std::wstring_view shellType,
                                                  const std::optional<WorkspaceNode>& workspaceNode);
    WorkspaceSshStartupPlan LoadWorkspaceSshStartupPlan(std::wstring_view currentWorkspaceId,
                                                        const std::optional<Workspace>& selectedWorkspace,
                                                        std::wstring_view workspaceNodeId,
                                                        std::wstring_view pendingStartupAction,
                                                        std::wstring_view startingDirectory,
                                                        std::wstring_view operatingSystem,
                                                        std::wstring_view shellType);
    WorkspaceCapturedNodeInput BuildWorkspaceCapturedNodeInput(const WorkspaceCapturedNodePlanInput& input);
    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceCapturedNodePlanInput& input);
    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceCapturedNodeInput& input);
    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceLiveTabCaptureState& state);
    std::optional<size_t> ResolveWorkspaceBackedTabIndex(const std::optional<Workspace>& workspaceDefinition,
                                                         const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                         size_t targetTabIndex);
    std::optional<WorkspaceNode> ResolveWorkspaceBackedTabNode(const std::optional<Workspace>& workspaceDefinition,
                                                               const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                               size_t targetTabIndex);
    std::optional<size_t> FindWorkspaceBackedTabSnapshotIndex(const std::optional<Workspace>& workspaceDefinition,
                                                              const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                              size_t nodeIndex);
    std::optional<size_t> LoadResolvedWorkspaceBackedTabIndex(std::wstring_view currentWorkspaceId,
                                                              const std::optional<Workspace>& selectedWorkspace,
                                                              const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                              size_t targetTabIndex);
    std::optional<WorkspaceNode> LoadResolvedWorkspaceBackedTabNode(std::wstring_view currentWorkspaceId,
                                                                    const std::optional<Workspace>& selectedWorkspace,
                                                                    const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                                    size_t targetTabIndex);
    std::optional<size_t> FindResolvedWorkspaceBackedTabSnapshotIndex(std::wstring_view currentWorkspaceId,
                                                                      const std::optional<Workspace>& selectedWorkspace,
                                                                      const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                                      size_t nodeIndex);
    bool IsWorkspaceLocked(std::wstring_view workspaceId);
    bool SetWorkspaceLocked(WorkspaceManager& manager, std::wstring_view workspaceId, bool locked);
    bool SetWorkspaceNodeInputVisibility(WorkspaceManager& manager, std::wstring_view workspaceId, size_t nodeIndex, bool showInputPanel);
    bool PersistWorkspaceLockedState(std::wstring_view workspaceId, bool locked);
    bool RemoveWorkspaceDefinition(WorkspaceManager& manager, std::wstring_view workspaceId, size_t* removedWorkspaceIndex = nullptr);
    WorkspaceNodeMutationResult RemoveWorkspaceNode(WorkspaceManager& manager, std::wstring_view workspaceId, std::wstring_view nodeId);
    void FinalizeWorkspaceManagerNames(WorkspaceManager& manager);
    std::optional<std::wstring> RenameWorkspace(WorkspaceManager& manager, std::wstring_view workspaceId, std::wstring_view newName);
    std::optional<uint64_t> FindPersistedOpenWorkspaceWindowId(std::wstring_view workspaceId);
    std::optional<PersistedWorkspaceRename> PersistWorkspaceRename(std::wstring_view workspaceId, std::wstring_view newName);
    std::optional<PersistedWorkspaceEditorSave> PersistWorkspaceEditorState(const WorkspaceManager& editorManager,
                                                                            std::wstring_view currentWorkspaceId,
                                                                            std::wstring_view lastOpenedWorkspaceId,
                                                                            size_t fallbackSelectedWorkspaceIndex);
    WorkspaceCurrentIdChangePlan PrepareWorkspaceCurrentIdChange(std::wstring_view previousWorkspaceId,
                                                                 std::wstring_view nextWorkspaceId,
                                                                 std::wstring_view lastWorkspaceId,
                                                                 std::wstring_view currentBaselineWorkspaceId);
    WorkspaceWindowRefreshPlan PrepareWorkspaceWindowRefresh(std::uint64_t windowId,
                                                             std::wstring_view currentWorkspaceId);
    WorkspaceWindowRefreshPlan RefreshWorkspaceWindowState(std::uint64_t windowId,
                                                           std::wstring_view currentWorkspaceId);
    void RemovePersistedWorkspaceWindowState(uint64_t windowId);
    void RefreshPersistedWorkspaceWindowState(uint64_t windowId,
                                              std::wstring_view processName,
                                              std::wstring_view workspaceId);
    std::optional<WorkspaceEditorDefinitionAddResult> AddWorkspaceDefinition(WorkspaceManager& manager,
                                                                             std::wstring_view generatedName,
                                                                             std::optional<size_t> templateIndex);
    WorkspaceEditorNodeAddResult AddWorkspaceNode(WorkspaceManager& manager,
                                                  size_t workspaceIndex,
                                                  std::wstring_view generatedName,
                                                  std::wstring_view defaultProfileGuid,
                                                  std::wstring_view defaultProfileName);
    std::optional<WorkspaceManager> PersistWorkspaceNodeInputVisibility(const WorkspaceManager& preferredManager,
                                                                        std::wstring_view workspaceId,
                                                                        size_t nodeIndex,
                                                                        bool showInputPanel);
    std::optional<WorkspaceManager> PersistWorkspaceNodeOrder(std::wstring_view workspaceId,
                                                              const std::vector<std::wstring>& orderedNodeIds);
    std::wstring ResolveWorkspaceSaveTargetId(std::wstring_view currentWorkspaceId, std::wstring_view lastWorkspaceId, const WorkspaceManager& manager);
    std::wstring ResolveWorkspaceSaveTargetName(std::wstring_view currentWorkspaceId, std::wstring_view lastWorkspaceId, const WorkspaceManager& manager);
    std::wstring SuggestWorkspaceSaveName(std::wstring_view resolvedTargetName,
                                          std::wstring_view windowName,
                                          std::wstring_view singleTabTitle,
                                          size_t workspaceCount,
                                          std::wstring_view generatedFallbackName);
    std::optional<winrt::Windows::UI::Color> ResolveWorkspaceNodeTabColor(const Workspace& workspace,
                                                                           size_t nodeIndex,
                                                                           const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);
    void EnsureWorkspaceNodeTabColors(Workspace& workspace, const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

    class WorkspaceStateManager
    {
    public:
        static std::filesystem::path DefaultPath();
        static WorkspaceStateManager Load();
        static WorkspaceStateManager LoadFromPath(const std::filesystem::path& path);
        static WorkspaceStateManager LoadRuntime();
        static uint64_t RuntimeHeartbeatIntervalMs() noexcept;
        static bool RemoveRuntimeWindowState(uint64_t windowId);
        static bool RefreshRuntimeWindowState(uint64_t windowId, std::wstring_view windowName, std::wstring_view workspaceId);

        bool Save() const;
        bool SaveToPath(const std::filesystem::path& path) const;

        const std::vector<WorkspaceStateWindow>& Windows() const noexcept;
        void SetWindows(std::vector<WorkspaceStateWindow> windows);
        void UpsertWindow(WorkspaceStateWindow window);
        void RemoveWindow(uint64_t windowId) noexcept;
        void UpdateWindowState(uint64_t windowId, std::wstring_view windowName, std::wstring_view workspaceId);
        bool HasOpenWorkspace(std::wstring_view workspaceId) const noexcept;
        std::optional<uint64_t> FindOpenWorkspaceWindowId(std::wstring_view workspaceId) const noexcept;

    private:
        std::vector<WorkspaceStateWindow> _windows;
    };
}
