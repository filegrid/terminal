// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "ExtCoreApi.h"

#include "WorkspaceCore.h"
#include "WorkspacePersistencePaths.h"

#include <algorithm>
#include <string_view>
#include <tuple>

extern "C" uint32_t WINAPI GetExtCoreApiVersion()
{
    return terminal::workspace::ExtCoreApiVersion;
}

extern "C" HRESULT WINAPI GetExtWorkspaceWindowRefreshPlan(const uint64_t windowId,
                                                             const wchar_t* const currentWorkspaceId,
                                                             terminal::workspace::ExtWorkspaceWindowRefreshPlan* const plan,
                                                             wchar_t* const workspaceId,
                                                             const uint32_t workspaceIdCapacity)
{
    if (!plan || plan->Size != sizeof(*plan))
    {
        return E_INVALIDARG;
    }

    const auto corePlan = terminal::workspace::RefreshWorkspaceWindowState(
        windowId,
        currentWorkspaceId ? std::wstring_view{ currentWorkspaceId } : std::wstring_view{});
    const auto requiredLength = static_cast<uint32_t>(corePlan.WorkspaceId.size());
    plan->WorkspaceIdLength = requiredLength;
    plan->ProcessId = corePlan.ProcessId;
    plan->SkipRefresh = corePlan.SkipRefresh;
    plan->ClearPendingWorkspaceLaunch = corePlan.ClearPendingWorkspaceLaunch;

    if (!workspaceId || workspaceIdCapacity <= requiredLength)
    {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    std::copy(corePlan.WorkspaceId.begin(), corePlan.WorkspaceId.end(), workspaceId);
    workspaceId[requiredLength] = L'\0';
    return S_OK;
}

extern "C" HRESULT WINAPI GetExtWorkspaceCurrentIdChangePlan(const wchar_t* const previousWorkspaceId,
                                                               const wchar_t* const nextWorkspaceId,
                                                               const wchar_t* const lastWorkspaceId,
                                                               const wchar_t* const currentBaselineWorkspaceId,
                                                               terminal::workspace::ExtWorkspaceCurrentIdChangePlan* const plan,
                                                               wchar_t* const resolvedLastWorkspaceId,
                                                               const uint32_t resolvedLastWorkspaceIdCapacity)
{
    if (!plan || plan->Size != sizeof(*plan))
    {
        return E_INVALIDARG;
    }

    const auto corePlan = terminal::workspace::PrepareWorkspaceCurrentIdChange(
        previousWorkspaceId ? std::wstring_view{ previousWorkspaceId } : std::wstring_view{},
        nextWorkspaceId ? std::wstring_view{ nextWorkspaceId } : std::wstring_view{},
        lastWorkspaceId ? std::wstring_view{ lastWorkspaceId } : std::wstring_view{},
        currentBaselineWorkspaceId ? std::wstring_view{ currentBaselineWorkspaceId } : std::wstring_view{});
    const auto requiredLength = static_cast<uint32_t>(corePlan.LastWorkspaceId.size());
    plan->LastWorkspaceIdLength = requiredLength;
    plan->ResetSaveBaseline = corePlan.ResetSaveBaseline ? 1 : 0;
    plan->StartHeartbeat = corePlan.StartHeartbeat ? 1 : 0;

    if (!resolvedLastWorkspaceId || resolvedLastWorkspaceIdCapacity <= requiredLength)
    {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    std::copy(corePlan.LastWorkspaceId.begin(), corePlan.LastWorkspaceId.end(), resolvedLastWorkspaceId);
    resolvedLastWorkspaceId[requiredLength] = L'\0';
    return S_OK;
}

extern "C" HRESULT WINAPI GetExtWorkspaceStartupPlan(const wchar_t* const workspaceId,
                                                       terminal::workspace::ExtWorkspaceStartupPlan* const plan,
                                                       uint8_t* const pendingInputVisibility,
                                                       const uint32_t pendingInputVisibilityCapacity,
                                                       wchar_t* const pendingNodeIds,
                                                       const uint32_t pendingNodeIdsCapacity)
{
    if (!plan || plan->Size != sizeof(*plan))
    {
        return E_INVALIDARG;
    }

    const auto state = terminal::workspace::LoadWorkspaceStartupState(
        workspaceId ? std::wstring_view{ workspaceId } : std::wstring_view{});
    const auto visibilityCount = static_cast<uint32_t>(state.PendingNodeInputVisibility.size());
    uint32_t nodeIdCharacterCount{};
    for (const auto& nodeId : state.PendingNodeIds)
    {
        nodeIdCharacterCount += static_cast<uint32_t>(nodeId.size() + 1);
    }

    plan->PendingInputVisibilityCount = visibilityCount;
    plan->PendingNodeIdCharacterCount = nodeIdCharacterCount;
    if ((visibilityCount && (!pendingInputVisibility || pendingInputVisibilityCapacity < visibilityCount)) ||
        (nodeIdCharacterCount && (!pendingNodeIds || pendingNodeIdsCapacity < nodeIdCharacterCount)))
    {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    for (uint32_t index = 0; index < visibilityCount; ++index)
    {
        pendingInputVisibility[index] = state.PendingNodeInputVisibility[index] ? 1 : 0;
    }

    uint32_t nodeIdOffset{};
    for (const auto& nodeId : state.PendingNodeIds)
    {
        std::copy(nodeId.begin(), nodeId.end(), pendingNodeIds + nodeIdOffset);
        nodeIdOffset += static_cast<uint32_t>(nodeId.size());
        pendingNodeIds[nodeIdOffset++] = L'\0';
    }
    return S_OK;
}

extern "C" HRESULT WINAPI GetExtWorkspaceManagerNavigationPlan(const uint64_t workspaceIndex,
                                                                  const uint64_t nodeIndex,
                                                                  const uint64_t workspaceCount,
                                                                  const uint64_t selectedWorkspaceIndex,
                                                                  const int32_t navSelection,
                                                                  terminal::workspace::ExtWorkspaceManagerNavigationPlan* const plan)
{
    if (!plan || plan->Size != sizeof(*plan))
    {
        return E_INVALIDARG;
    }

    plan->WorkspaceSelection = terminal::workspace::WorkspaceManagerNavSelectionForWorkspace(static_cast<size_t>(workspaceIndex));
    plan->WorkspaceNodeSelection = terminal::workspace::WorkspaceManagerNavSelectionForWorkspaceNode(static_cast<size_t>(workspaceIndex), static_cast<size_t>(nodeIndex));
    plan->EditorSelection = terminal::workspace::ResolveWorkspaceManagerNavSelectionForEditor(static_cast<size_t>(workspaceCount), static_cast<size_t>(selectedWorkspaceIndex));
    if (const auto resolved = terminal::workspace::ResolveWorkspaceIndexFromManagerNavSelection(navSelection))
    {
        plan->ResolvedWorkspaceIndex = gsl::narrow_cast<int32_t>(*resolved);
        plan->HasResolvedWorkspaceIndex = 1;
    }
    if (const auto resolved = terminal::workspace::ResolveWorkspaceNodeIndexFromManagerNavSelection(navSelection))
    {
        plan->ResolvedNodeIndex = gsl::narrow_cast<int32_t>(*resolved);
        plan->HasResolvedNodeIndex = 1;
    }
    return S_OK;
}

extern "C" HRESULT WINAPI GetExtWorkspaceDefinitionsPath(wchar_t* const path,
                                                           const uint32_t pathCapacity,
                                                           uint32_t* const pathLength)
{
    if (!pathLength)
    {
        return E_INVALIDARG;
    }

    const auto value = terminal::workspacepaths::ResolveWorkspaceDefinitionsPath().wstring();
    const auto requiredLength = static_cast<uint32_t>(value.size());
    *pathLength = requiredLength;
    if (!path || pathCapacity <= requiredLength)
    {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    std::copy(value.begin(), value.end(), path);
    path[requiredLength] = L'\0';
    return S_OK;
}

extern "C" HRESULT WINAPI RemoveExtPersistedWorkspaceWindowState(const uint64_t windowId)
{
    std::ignore = terminal::workspace::WorkspaceStateManager::RemoveRuntimeWindowState(windowId);
    return S_OK;
}
