// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct WorkspaceNodeCommand
    {
        std::wstring Id;
        std::wstring Icon;
        std::wstring Name;
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
        std::wstring StartupAction;
        // New command-list schema. Empty represents an unmigrated legacy node
        // and is interpreted by Core as its single StartupAction command.
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

    struct WorkspaceSaveTargetState
    {
        std::wstring Id;
        std::wstring Name;
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
}
