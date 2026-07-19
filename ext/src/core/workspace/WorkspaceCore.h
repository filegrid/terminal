// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace terminal::workspace
{
    class WorkspaceManager;

    struct WorkspaceNode
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring ConnectionRef;
        std::wstring ProfileGuid;
        std::wstring TabColor;
        bool ShowTab{ true };
        std::wstring StartupDirectory;
        std::wstring StartupAction;
        std::wstring OperatingSystem;
        std::wstring ShellType;
        bool ShowInputPanel{ false };
        bool UseNodeNameAsTabTitle{ true };
    };

    struct Workspace
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring Description;
        std::wstring BackgroundColor;
        bool Locked{ true };
        std::vector<WorkspaceNode> Nodes;
    };

    struct WorkspaceStateWindow
    {
        uint64_t WindowId{};
        std::wstring WindowName;
        std::wstring WorkspaceId;
    };

    struct WorkspaceSavePlan
    {
        std::vector<Workspace> Workspaces;
        Workspace SavedWorkspace;
        size_t SavedWorkspaceIndex{};
    };

    enum class WorkspaceOpenDisposition
    {
        Missing,
        SummonExistingWindow,
        OpenInNewWindow,
        ReplaceCurrentWindow,
    };

    struct WorkspaceOpenPlan
    {
        WorkspaceOpenDisposition Disposition{ WorkspaceOpenDisposition::Missing };
        Workspace TargetWorkspace;
        std::optional<uint64_t> ExistingWindowId;
        bool ConfirmSaveCurrentWorkspace{ false };
        std::vector<std::wstring> PendingNodeIds;
        std::vector<bool> PendingNodeInputVisibility;
    };

    enum class WorkspaceOpenExecutionDisposition
    {
        Missing,
        SummonExistingWindow,
        NoStartupActions,
        OpenInNewWindow,
        ReplaceCurrentWindow,
    };

    struct WorkspaceOpenExecutionPlan
    {
        WorkspaceOpenExecutionDisposition Disposition{ WorkspaceOpenExecutionDisposition::Missing };
        std::optional<uint64_t> ExistingWindowId;
        bool ConfirmSaveCurrentWorkspace{ false };
        bool SetLastOpenedWorkspaceId{ false };
        bool UpdatePendingWorkspaceLaunch{ false };
        bool SetSaveBaseline{ false };
        bool SetCurrentWorkspaceBeforeActions{ false };
        bool ReplacePendingNodeQueues{ false };
        bool FocusActiveContentAfterActions{ false };
        bool RemoveCapturedTabsAfterActions{ false };
        bool SetCurrentWorkspaceAfterActions{ false };
    };

    struct WorkspaceSshStartupPlan
    {
        std::wstring StartupAction;
        std::wstring StartingDirectory;
        std::wstring OperatingSystem;
        std::wstring ShellType;
        std::vector<std::wstring> DeferredStartupInputs;
        bool StartupInputPending{ false };
        bool StartupInputDispatched{ false };
    };

    struct WorkspaceRuntimeMetadata
    {
        std::wstring OperatingSystem;
        std::wstring ShellType;
    };

    struct WorkspaceRuntimeLaunchState
    {
        std::wstring ExplicitCommandline;
        std::wstring StartingDirectory;
        std::wstring OperatingSystem;
        std::wstring ShellType;
        bool IsSshTransport{ false };
        bool HasSshTtyOption{ false };
    };

    struct WorkspaceNodeLaunchResolutionInput
    {
        std::optional<WorkspaceNode> PersistedNode;
        std::wstring ObservedStartupAction;
        std::wstring ObservedWorkingDirectory;
        std::wstring ObservedOperatingSystem;
        std::wstring ObservedShellType;
        std::wstring RuntimeStartupAction;
        std::wstring RuntimeExplicitCommandline;
        std::wstring RuntimeStartingDirectory;
        std::wstring RuntimeOperatingSystem;
        std::wstring RuntimeShellType;
        std::wstring ProfileSource;
        std::wstring ProfileCommandline;
        std::wstring TerminalCommandline;
        std::wstring TerminalStartingDirectory;
    };

    struct WorkspaceNodeLaunchResolution
    {
        std::wstring StartupAction;
        std::wstring StartingDirectory;
        std::wstring OperatingSystem;
        std::wstring ShellType;
    };

    struct WorkspaceTrackedDirectoryInput
    {
        std::wstring ReportedWorkingDirectory;
        std::wstring ProcessWorkingDirectory;
        std::wstring RuntimeStartingDirectory;
        std::wstring RuntimeOperatingSystem;
        std::wstring RuntimeShellType;
        bool IsSshTransport{ false };
    };

    struct WorkspaceLiveTabSnapshot
    {
        bool LoadsWorkspaceNode{ false };
        std::wstring RuntimeNodeId;
    };

    struct WorkspaceLiveTabCaptureState
    {
        std::optional<WorkspaceNode> PersistedNode;
        std::wstring LiveTabTitle;
        std::wstring StartupTabTitle;
        std::wstring GeneratedNodeName;
        std::wstring ProfileGuid;
        WorkspaceNodeLaunchResolution LaunchResolution;
        bool ShowInputPanel{ false };
        std::wstring TabColor;
    };

    enum class WorkspaceNodeMutationDisposition
    {
        NotFound,
        RemovedNode,
        RemovedWorkspace,
    };

    struct WorkspaceNodeMutationResult
    {
        WorkspaceNodeMutationDisposition Disposition{ WorkspaceNodeMutationDisposition::NotFound };
        size_t WorkspaceIndex{};
        size_t NodeIndex{};
    };

    struct WorkspaceCurrentState
    {
        bool Exists{ false };
        std::wstring DisplayName;
        std::wstring TabRowName;
        std::wstring BackgroundColor;
        bool Locked{ false };
    };

    struct WorkspaceStartupState
    {
        std::vector<std::wstring> PendingNodeIds;
        std::vector<bool> PendingNodeInputVisibility;
    };

    struct WorkspaceFlyoutEntry
    {
        Workspace Definition;
        bool IsOpen{ false };
    };

    struct WorkspaceFlyoutState
    {
        std::vector<WorkspaceFlyoutEntry> Entries;
        bool CurrentWorkspaceExists{ false };
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

        std::vector<Workspace>& Workspaces() noexcept;
        const std::vector<Workspace>& Workspaces() const noexcept;
        void SetWorkspaces(std::vector<Workspace> workspaces);

    protected:
        std::vector<Workspace> _workspaces;
    };

    struct WorkspaceEditorLoadState
    {
        WorkspaceManager Manager;
        size_t SelectedWorkspaceIndex{};
    };

    struct WorkspaceEditorSavePlan
    {
        std::wstring ResolvedCurrentWorkspaceId;
        bool CurrentWorkspaceExists{ false };
        bool LastOpenedWorkspaceExists{ true };
        size_t SelectedWorkspaceIndex{};
    };

    struct WorkspaceEditorDefinitionRemovalPlan
    {
        int32_t NavSelection{};
        bool RemovedCurrentWorkspace{ false };
        bool LastOpenedWorkspaceExists{ true };
        size_t SelectedWorkspaceIndex{};
    };

    struct WorkspaceEditorNodeRemovalPlan
    {
        WorkspaceNodeMutationDisposition Disposition{ WorkspaceNodeMutationDisposition::NotFound };
        int32_t NavSelection{};
        bool RemovedCurrentWorkspace{ false };
        bool LastOpenedWorkspaceExists{ true };
        size_t SelectedWorkspaceIndex{};
    };

    class WorkspaceStateManager;

    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes);
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
    WorkspaceSshStartupPlan PrepareSshStartupPlan(std::wstring_view pendingStartupAction,
                                                  std::wstring_view startingDirectory,
                                                  std::wstring_view operatingSystem,
                                                  std::wstring_view shellType,
                                                  const std::optional<WorkspaceNode>& workspaceNode);
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
    bool SetWorkspaceLocked(WorkspaceManager& manager, std::wstring_view workspaceId, bool locked);
    bool SetWorkspaceNodeInputVisibility(WorkspaceManager& manager, std::wstring_view workspaceId, size_t nodeIndex, bool showInputPanel);
    bool RemoveWorkspaceDefinition(WorkspaceManager& manager, std::wstring_view workspaceId, size_t* removedWorkspaceIndex = nullptr);
    WorkspaceNodeMutationResult RemoveWorkspaceNode(WorkspaceManager& manager, std::wstring_view workspaceId, std::wstring_view nodeId);
    void FinalizeWorkspaceManagerNames(WorkspaceManager& manager);
    std::optional<std::wstring> RenameWorkspace(WorkspaceManager& manager, std::wstring_view workspaceId, std::wstring_view newName);
    std::wstring ResolveWorkspaceSaveTargetId(std::wstring_view currentWorkspaceId, std::wstring_view lastWorkspaceId, const WorkspaceManager& manager);
    std::wstring ResolveWorkspaceSaveTargetName(std::wstring_view currentWorkspaceId, std::wstring_view lastWorkspaceId, const WorkspaceManager& manager);
    std::wstring SuggestWorkspaceSaveName(std::wstring_view resolvedTargetName,
                                          std::wstring_view windowName,
                                          std::wstring_view singleTabTitle,
                                          size_t workspaceCount,
                                          std::wstring_view generatedFallbackName);

    class WorkspaceStateManager
    {
    public:
        static std::filesystem::path DefaultPath();
        static WorkspaceStateManager Load();
        static WorkspaceStateManager LoadFromPath(const std::filesystem::path& path);

        bool Save() const;
        bool SaveToPath(const std::filesystem::path& path) const;

        const std::vector<WorkspaceStateWindow>& Windows() const noexcept;
        void SetWindows(std::vector<WorkspaceStateWindow> windows);
        void UpsertWindow(WorkspaceStateWindow window);
        void RemoveWindow(uint64_t windowId) noexcept;
        void UpdateWindowState(uint64_t windowId, std::wstring_view windowName, std::wstring_view workspaceId);
        std::optional<uint64_t> FindOpenWorkspaceWindowId(std::wstring_view workspaceId) const noexcept;

    protected:
        std::vector<WorkspaceStateWindow> _windows;
    };
}
