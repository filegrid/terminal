// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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
