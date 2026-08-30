// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <Windows.h>
#include <cstdint>

namespace terminal::workspace
{
    inline constexpr uint32_t ExtCoreApiVersion{ 7 };
    inline constexpr char GetExtCoreApiVersionSymbol[] = "GetExtCoreApiVersion";
    inline constexpr char GetExtWorkspaceCurrentIdChangePlanSymbol[] = "GetExtWorkspaceCurrentIdChangePlan";
    inline constexpr char GetExtWorkspaceDefinitionsPathSymbol[] = "GetExtWorkspaceDefinitionsPath";
    inline constexpr char RemoveExtPersistedWorkspaceWindowStateSymbol[] = "RemoveExtPersistedWorkspaceWindowState";
    inline constexpr char GetExtWorkspaceWindowRefreshPlanSymbol[] = "GetExtWorkspaceWindowRefreshPlan";
    inline constexpr char GetExtWorkspaceStartupPlanSymbol[] = "GetExtWorkspaceStartupPlan";
    inline constexpr char GetExtWorkspaceManagerNavigationPlanSymbol[] = "GetExtWorkspaceManagerNavigationPlan";

    // C ABI only: callers own the output character buffer. This keeps Glue from
    // depending on Ext's C++/WinRT value types or SettingsModel implementation.
    struct ExtWorkspaceWindowRefreshPlan
    {
        uint32_t Size{};
        uint32_t WorkspaceIdLength{};
        uint64_t ProcessId{};
        uint8_t SkipRefresh{};
        uint8_t ClearPendingWorkspaceLaunch{};
        uint8_t Reserved[6]{};
    };

    struct ExtWorkspaceCurrentIdChangePlan
    {
        uint32_t Size{};
        uint32_t LastWorkspaceIdLength{};
        uint8_t ResetSaveBaseline{};
        uint8_t StartHeartbeat{};
        uint8_t Reserved[6]{};
    };

    // Node IDs are encoded as consecutive NUL-terminated UTF-16 strings. The
    // caller owns both buffers, so no C++ container crosses the DLL boundary.
    struct ExtWorkspaceStartupPlan
    {
        uint32_t Size{};
        uint32_t PendingInputVisibilityCount{};
        uint32_t PendingNodeIdCharacterCount{};
        uint32_t Reserved{};
    };

    // Navigation values are core policy. Keep their encoding out of Glue and
    // return all values needed by a page event in one fixed-layout plan.
    struct ExtWorkspaceManagerNavigationPlan
    {
        uint32_t Size{};
        int32_t WorkspaceSelection{};
        int32_t WorkspaceNodeSelection{};
        int32_t EditorSelection{};
        int32_t ResolvedWorkspaceIndex{};
        int32_t ResolvedNodeIndex{};
        uint8_t HasResolvedWorkspaceIndex{};
        uint8_t HasResolvedNodeIndex{};
        uint8_t Reserved[2]{};
    };

    using GetExtCoreApiVersionFn = uint32_t(WINAPI*)();
    using GetExtWorkspaceWindowRefreshPlanFn = HRESULT(WINAPI*)(uint64_t windowId,
                                                                 const wchar_t* currentWorkspaceId,
                                                                 ExtWorkspaceWindowRefreshPlan* plan,
                                                                 wchar_t* workspaceId,
                                                                 uint32_t workspaceIdCapacity);
    using GetExtWorkspaceCurrentIdChangePlanFn = HRESULT(WINAPI*)(const wchar_t* previousWorkspaceId,
                                                                   const wchar_t* nextWorkspaceId,
                                                                   const wchar_t* lastWorkspaceId,
                                                                   const wchar_t* currentBaselineWorkspaceId,
                                                                   ExtWorkspaceCurrentIdChangePlan* plan,
                                                                   wchar_t* resolvedLastWorkspaceId,
                                                                   uint32_t resolvedLastWorkspaceIdCapacity);
    using GetExtWorkspaceStartupPlanFn = HRESULT(WINAPI*)(const wchar_t* workspaceId,
                                                           ExtWorkspaceStartupPlan* plan,
                                                           uint8_t* pendingInputVisibility,
                                                           uint32_t pendingInputVisibilityCapacity,
                                                           wchar_t* pendingNodeIds,
                                                           uint32_t pendingNodeIdsCapacity);
    using GetExtWorkspaceManagerNavigationPlanFn = HRESULT(WINAPI*)(uint64_t workspaceIndex,
                                                                     uint64_t nodeIndex,
                                                                     uint64_t workspaceCount,
                                                                     uint64_t selectedWorkspaceIndex,
                                                                     int32_t navSelection,
                                                                     ExtWorkspaceManagerNavigationPlan* plan);
    using GetExtWorkspaceDefinitionsPathFn = HRESULT(WINAPI*)(wchar_t* path, uint32_t pathCapacity, uint32_t* pathLength);
    using RemoveExtPersistedWorkspaceWindowStateFn = HRESULT(WINAPI*)(uint64_t windowId);
}

#if defined(WT_EXT_CORE_BUILD)
#define WT_EXT_CORE_API __declspec(dllexport)
#else
#define WT_EXT_CORE_API
#endif

extern "C" WT_EXT_CORE_API uint32_t WINAPI GetExtCoreApiVersion();
extern "C" WT_EXT_CORE_API HRESULT WINAPI GetExtWorkspaceWindowRefreshPlan(uint64_t windowId,
                                                                             const wchar_t* currentWorkspaceId,
                                                                             terminal::workspace::ExtWorkspaceWindowRefreshPlan* plan,
                                                                             wchar_t* workspaceId,
                                                                             uint32_t workspaceIdCapacity);
extern "C" WT_EXT_CORE_API HRESULT WINAPI GetExtWorkspaceCurrentIdChangePlan(const wchar_t* previousWorkspaceId,
                                                                                const wchar_t* nextWorkspaceId,
                                                                                const wchar_t* lastWorkspaceId,
                                                                                const wchar_t* currentBaselineWorkspaceId,
                                                                                terminal::workspace::ExtWorkspaceCurrentIdChangePlan* plan,
                                                                                wchar_t* resolvedLastWorkspaceId,
                                                                                uint32_t resolvedLastWorkspaceIdCapacity);
extern "C" WT_EXT_CORE_API HRESULT WINAPI GetExtWorkspaceStartupPlan(const wchar_t* workspaceId,
                                                                        terminal::workspace::ExtWorkspaceStartupPlan* plan,
                                                                        uint8_t* pendingInputVisibility,
                                                                        uint32_t pendingInputVisibilityCapacity,
                                                                        wchar_t* pendingNodeIds,
                                                                        uint32_t pendingNodeIdsCapacity);
extern "C" WT_EXT_CORE_API HRESULT WINAPI GetExtWorkspaceManagerNavigationPlan(uint64_t workspaceIndex,
                                                                                  uint64_t nodeIndex,
                                                                                  uint64_t workspaceCount,
                                                                                  uint64_t selectedWorkspaceIndex,
                                                                                  int32_t navSelection,
                                                                                  terminal::workspace::ExtWorkspaceManagerNavigationPlan* plan);
extern "C" WT_EXT_CORE_API HRESULT WINAPI GetExtWorkspaceDefinitionsPath(wchar_t* path,
                                                                            uint32_t pathCapacity,
                                                                            uint32_t* pathLength);
extern "C" WT_EXT_CORE_API HRESULT WINAPI RemoveExtPersistedWorkspaceWindowState(uint64_t windowId);
