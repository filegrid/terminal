// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "WorkspaceDataTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
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
}
