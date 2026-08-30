// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <Windows.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../../core/workspace/ExtCoreApi.h"

namespace terminal::workspace
{
    struct LoadedWorkspaceWindowRefreshPlan
    {
        std::wstring WorkspaceId;
        uint64_t ProcessId{};
        bool SkipRefresh{};
        bool ClearPendingWorkspaceLaunch{};
    };

    struct LoadedWorkspaceCurrentIdChangePlan
    {
        std::wstring LastWorkspaceId;
        bool ResetSaveBaseline{};
        bool StartHeartbeat{};
    };

    struct LoadedWorkspaceStartupPlan
    {
        std::vector<bool> PendingNodeInputVisibility;
        std::vector<std::wstring> PendingNodeIds;
    };

    struct LoadedWorkspaceManagerNavigationPlan
    {
        int32_t WorkspaceSelection{};
        int32_t WorkspaceNodeSelection{};
        int32_t EditorSelection{};
        std::optional<size_t> ResolvedWorkspaceIndex;
        std::optional<size_t> ResolvedNodeIndex;
    };

    class WorkspaceExtLoader final
    {
    public:
        WorkspaceExtLoader();
        ~WorkspaceExtLoader();

        WorkspaceExtLoader(const WorkspaceExtLoader&) = delete;
        WorkspaceExtLoader& operator=(const WorkspaceExtLoader&) = delete;

        LoadedWorkspaceWindowRefreshPlan RefreshWorkspaceWindowState(uint64_t windowId,
                                                                      const std::wstring& currentWorkspaceId) const;
        LoadedWorkspaceCurrentIdChangePlan PrepareWorkspaceCurrentIdChange(const std::wstring& previousWorkspaceId,
                                                                            const std::wstring& nextWorkspaceId,
                                                                            const std::wstring& lastWorkspaceId,
                                                                            const std::wstring& currentBaselineWorkspaceId) const;
        LoadedWorkspaceStartupPlan PrepareWorkspaceStartup(const std::wstring& workspaceId) const;
        LoadedWorkspaceManagerNavigationPlan PrepareWorkspaceManagerNavigation(uint64_t workspaceIndex,
                                                                                uint64_t nodeIndex,
                                                                                uint64_t workspaceCount,
                                                                                uint64_t selectedWorkspaceIndex,
                                                                                int32_t navSelection) const;

    private:
        HMODULE _module{ nullptr };
        GetExtWorkspaceCurrentIdChangePlanFn _getWorkspaceCurrentIdChangePlan{ nullptr };
        GetExtWorkspaceStartupPlanFn _getWorkspaceStartupPlan{ nullptr };
        GetExtWorkspaceManagerNavigationPlanFn _getWorkspaceManagerNavigationPlan{ nullptr };
        GetExtWorkspaceWindowRefreshPlanFn _getWorkspaceWindowRefreshPlan{ nullptr };
    };
}
