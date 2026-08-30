// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

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

    // This is deliberately separate from a terminal title/runtime instance. A
    // command identifies a configured window; its live title belongs to Glue's
    // session state in the next phase.
    struct WorkspaceNodeCommand
    {
        std::wstring Id;
        std::wstring Icon;
        std::wstring Name;
        // An empty command is valid: it opens the selected profile unchanged.
        std::wstring Command;
    };

    enum class WorkspaceWindowDisplayMode
    {
        Split,
        Tab,
    };

    enum class WorkspaceTabPlacement
    {
        TopLeft,
        TopRight,
        BottomRight,
    };

    struct WorkspaceMultiWindowPreference
    {
        WorkspaceWindowDisplayMode DisplayMode{ WorkspaceWindowDisplayMode::Split };
        WorkspaceTabPlacement TabPlacement{ WorkspaceTabPlacement::TopLeft };
        // One entry per command. Core stores these in 5% increments.
        std::vector<double> SplitWeights;
    };

    struct WorkspaceNode
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring ConnectionRef;
        std::wstring ProfileGuid;
        std::wstring ProfileName;
        // Empty means inherit the selected profile's icon.
        std::wstring Icon;
        std::wstring TabColor;
        bool ShowTab{ true };
        std::wstring StartupDirectory;
        // Legacy single-command representation. It is read for migration;
        // the next save writes Commands instead.
        std::wstring StartupAction;
        // New ordered representation. Empty means use StartupAction as a
        // compatible single command at read time.
        std::vector<WorkspaceNodeCommand> Commands;
        WorkspaceMultiWindowPreference MultiWindowPreference;
        std::wstring OperatingSystem;
        std::wstring ShellType;
        bool ShowInputPanel{ false };
        bool UseNodeNameAsTabTitle{ false };
    };

    struct Workspace
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring Description;
        std::wstring BackgroundColor;
        std::wstring Icon;
        bool Locked{ true };
        // Values copied into a newly created blank node for this workspace.
        WorkspaceNode NewNodeDefaults;
        std::vector<std::wstring> TabOrder;
        std::vector<WorkspaceNode> Nodes;
    };

    struct WorkspaceStateWindow
    {
        uint64_t WindowId{};
        uint32_t ProcessId{};
        std::wstring ProcessName;
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

    struct WorkspaceNodeLaunchResolutionPlanInput
    {
        std::optional<WorkspaceNode> PersistedNode;
        std::wstring ObservedStartupAction;
        std::wstring ObservedWorkingDirectory;
        std::wstring TrackedWorkingDirectory;
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
        std::wstring ProfileName;
        WorkspaceNodeLaunchResolution LaunchResolution;
        bool ShowInputPanel{ false };
        std::wstring TabColor;
    };

    struct WorkspaceCapturedNodeInput
    {
        WorkspaceNodeLaunchResolutionInput LaunchInput;
        WorkspaceLiveTabCaptureState CaptureState;
    };

    struct WorkspaceCapturedNodePlanInput
    {
        std::optional<WorkspaceNode> PersistedNode;
        std::wstring ProfileSource;
        std::wstring ProfileCommandline;
        std::wstring TerminalCommandline;
        std::wstring TerminalStartingDirectory;
        std::wstring ObservedStartupAction;
        std::wstring ObservedWorkingDirectory;
        std::wstring TrackedWorkingDirectory;
        std::wstring ObservedOperatingSystem;
        std::wstring ObservedShellType;
        std::wstring RuntimeStartupAction;
        std::wstring RuntimeExplicitCommandline;
        std::wstring RuntimeStartingDirectory;
        std::wstring RuntimeOperatingSystem;
        std::wstring RuntimeShellType;
        std::wstring LiveTabTitle;
        std::wstring StartupTabTitle;
        std::wstring GeneratedNodeName;
        std::wstring ProfileGuid;
        std::wstring ProfileName;
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
        std::wstring Icon;
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

    struct LoadedWorkspaceFlyoutState
    {
        WorkspaceManager Manager;
        WorkspaceFlyoutState FlyoutState;
    };

    struct LoadedWorkspaceOpenState
    {
        WorkspaceManager Manager;
        WorkspaceOpenPlan OpenPlan;
    };

    struct LoadedWorkspaceOpenExecutionState
    {
        WorkspaceManager Manager;
        WorkspaceOpenPlan OpenPlan;
        WorkspaceStartupState StartupState;
        WorkspaceOpenExecutionPlan ExecutionPlan;
    };

    struct PersistedWorkspaceRename
    {
        WorkspaceManager Manager;
        std::wstring ResolvedWorkspaceName;
    };

    struct PersistedWorkspaceEditorSave
    {
        WorkspaceManager Manager;
        WorkspaceEditorSavePlan SavePlan;
    };

    struct WorkspaceEditorDefinitionAddResult
    {
        size_t AddedWorkspaceIndex{};
    };

    struct WorkspaceEditorNodeAddResult
    {
        bool Added{ false };
    };

    struct WorkspaceCurrentIdChangePlan
    {
        std::wstring LastWorkspaceId;
        bool ResetSaveBaseline{ false };
        bool StartHeartbeat{ false };
    };

    struct WorkspaceWindowRefreshPlan
    {
        bool SkipRefresh{ false };
        bool ClearPendingWorkspaceLaunch{ false };
        bool Refreshed{ false };
        uint32_t ProcessId{};
        std::wstring WorkspaceId;
    };

    struct WorkspaceNodeRuntimeStatePlan
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
        bool StartupInputPending{ false };
        bool StartupInputDispatched{ false };
        bool SkipPendingStartupSendInput{ false };
        bool HasRuntimeState{ false };
    };

    struct WorkspaceNodeRuntimeRegistrationInput
    {
        std::wstring WorkspaceNodeId;
        std::wstring PendingStartupAction;
        std::wstring StartingDirectory;
        std::wstring ProfileSource;
        std::wstring ProfileCommandline;
        std::wstring TerminalCommandline;
        std::wstring CurrentWorkspaceId;
        std::optional<Workspace> SelectedWorkspace;
    };

    class WorkspaceStateManager;

    constexpr size_t WorkspaceNodeMinCommandCount{ 1 };
    constexpr size_t WorkspaceNodeMaxCommandCount{ 3 };
    constexpr double WorkspaceSplitWeightStep{ 0.05 };

    struct WorkspaceMultiWindowValidationResult
    {
        bool IsValid{ false };
        std::wstring Message;
    };

    struct WorkspaceSplitLayoutResult
    {
        std::vector<double> WindowWidths;
        bool RequiresHorizontalScroll{ false };
    };

    // Runtime-only state. This deliberately never belongs to WorkspaceNode's
    // persisted command configuration: terminal title refreshes must not edit
    // the user's configured command name or command line.
    struct WorkspaceCommandRuntimeState
    {
        std::wstring CommandId;
        std::wstring Title;
        bool IsRunning{ false };
    };

    struct WorkspaceNodeSessionState
    {
        std::wstring ActiveCommandId;
        std::vector<WorkspaceCommandRuntimeState> Commands;
    };

    // Returns the configured list, or a synthesized legacy command when the
    // node still only has startupAction. Callers never need to branch on file
    // schema while reading a node.
    std::vector<WorkspaceNodeCommand> EffectiveWorkspaceNodeCommands(const WorkspaceNode& node);
    WorkspaceMultiWindowValidationResult ValidateWorkspaceNodeMultiWindowConfig(const WorkspaceNode& node);
    bool SetWorkspaceNodeCommands(WorkspaceNode& node, std::vector<WorkspaceNodeCommand> commands);
    bool ReorderWorkspaceNodeCommands(WorkspaceNode& node, const std::vector<std::wstring>& orderedCommandIds);
    bool SetWorkspaceNodeSplitWeights(WorkspaceNode& node, const std::vector<double>& weights);
    bool ResizeWorkspaceNodeSplit(WorkspaceNode& node, size_t dividerIndex, double leftRatio);
    WorkspaceSplitLayoutResult CalculateWorkspaceNodeSplitLayout(const WorkspaceNode& node,
                                                                  double availableWidth,
                                                                  double minimumWindowWidth);
    bool SetWorkspaceCommandRuntimeTitle(const WorkspaceNode& node,
                                         WorkspaceNodeSessionState& session,
                                         std::wstring_view commandId,
                                         std::wstring title);
    bool SetWorkspaceNodeActiveCommand(const WorkspaceNode& node,
                                       WorkspaceNodeSessionState& session,
                                       std::wstring_view commandId);
    std::wstring ResolveWorkspaceCommandDisplayName(const WorkspaceNodeCommand& command,
                                                    const WorkspaceNodeSessionState& session);
    void NormalizeWorkspaceNodeMultiWindowConfig(WorkspaceNode& node);

    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes);
    // Moves a visible editor node by one slot and keeps the persisted tab order
    // synchronized with the definition order. UI hosts use this rather than
    // duplicating the visibility and TabOrder rules.
    bool MoveWorkspaceManagerVisibleNode(Workspace& workspace, size_t nodeIndex, int offset);
    // Replaces a just-added editor node with a node template while retaining the
    // generated identity. Profile/color presentation is intentionally outside
    // this pure editor operation.
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
                                                                      bool hasTabsToReplace);
    std::optional<Workspace> LoadWorkspaceDefinition(std::wstring_view workspaceId);
    std::optional<Workspace> LoadResolvedWorkspaceDefinition(std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace);
    std::optional<WorkspaceNode> LoadResolvedWorkspaceNode(std::wstring_view currentWorkspaceId,
                                                           const std::optional<Workspace>& selectedWorkspace,
                                                           std::wstring_view nodeId);
    WorkspaceCurrentState LoadCurrentWorkspaceState(std::wstring_view currentWorkspaceId,
                                                    std::wstring_view defaultDisplayName,
                                                    std::wstring_view unsavedTabRowName);
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

    protected:
        std::vector<WorkspaceStateWindow> _windows;
    };
}
