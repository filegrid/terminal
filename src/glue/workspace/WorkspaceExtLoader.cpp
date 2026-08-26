// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "WorkspaceExtLoader.h"

namespace terminal::workspace
{
    WorkspaceExtLoader::WorkspaceExtLoader()
    {
        std::filesystem::path modulePath = wil::GetModuleFileNameW<std::wstring>(nullptr);
        modulePath.replace_filename(L"Ext.dll");

        _module = LoadLibraryExW(modulePath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        THROW_LAST_ERROR_IF_NULL(_module);

        const auto getApiVersion = reinterpret_cast<GetExtCoreApiVersionFn>(
            GetProcAddress(_module, GetExtCoreApiVersionSymbol));
        if (!getApiVersion || getApiVersion() != ExtCoreApiVersion)
        {
            FreeLibrary(_module);
            _module = nullptr;
            THROW_HR(E_NOINTERFACE);
        }

        _getWorkspaceWindowRefreshPlan = reinterpret_cast<GetExtWorkspaceWindowRefreshPlanFn>(
            GetProcAddress(_module, GetExtWorkspaceWindowRefreshPlanSymbol));
        _getWorkspaceCurrentIdChangePlan = reinterpret_cast<GetExtWorkspaceCurrentIdChangePlanFn>(
            GetProcAddress(_module, GetExtWorkspaceCurrentIdChangePlanSymbol));
        _getWorkspaceStartupPlan = reinterpret_cast<GetExtWorkspaceStartupPlanFn>(
            GetProcAddress(_module, GetExtWorkspaceStartupPlanSymbol));
        _getWorkspaceManagerNavigationPlan = reinterpret_cast<GetExtWorkspaceManagerNavigationPlanFn>(
            GetProcAddress(_module, GetExtWorkspaceManagerNavigationPlanSymbol));
        if (!_getWorkspaceWindowRefreshPlan || !_getWorkspaceCurrentIdChangePlan || !_getWorkspaceStartupPlan || !_getWorkspaceManagerNavigationPlan)
        {
            FreeLibrary(_module);
            _module = nullptr;
            THROW_HR(E_NOINTERFACE);
        }
    }

    WorkspaceExtLoader::~WorkspaceExtLoader()
    {
        if (_module)
        {
            FreeLibrary(_module);
        }
    }

    LoadedWorkspaceWindowRefreshPlan WorkspaceExtLoader::RefreshWorkspaceWindowState(const uint64_t windowId,
                                                                                       const std::wstring& currentWorkspaceId) const
    {
        ExtWorkspaceWindowRefreshPlan rawPlan{};
        rawPlan.Size = sizeof(rawPlan);
        const auto initialResult = _getWorkspaceWindowRefreshPlan(windowId, currentWorkspaceId.c_str(), &rawPlan, nullptr, 0);
        if (initialResult != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
        {
            THROW_IF_FAILED(initialResult);
        }

        std::wstring workspaceId(rawPlan.WorkspaceIdLength, L'\0');
        workspaceId.push_back(L'\0');
        THROW_IF_FAILED(_getWorkspaceWindowRefreshPlan(windowId,
                                                        currentWorkspaceId.c_str(),
                                                        &rawPlan,
                                                        workspaceId.data(),
                                                        static_cast<uint32_t>(workspaceId.size())));
        workspaceId.resize(rawPlan.WorkspaceIdLength);
        return { std::move(workspaceId),
                 rawPlan.ProcessId,
                 rawPlan.SkipRefresh != 0,
                 rawPlan.ClearPendingWorkspaceLaunch != 0 };
    }

    LoadedWorkspaceCurrentIdChangePlan WorkspaceExtLoader::PrepareWorkspaceCurrentIdChange(
        const std::wstring& previousWorkspaceId,
        const std::wstring& nextWorkspaceId,
        const std::wstring& lastWorkspaceId,
        const std::wstring& currentBaselineWorkspaceId) const
    {
        ExtWorkspaceCurrentIdChangePlan rawPlan{};
        rawPlan.Size = sizeof(rawPlan);
        const auto initialResult = _getWorkspaceCurrentIdChangePlan(previousWorkspaceId.c_str(),
                                                                      nextWorkspaceId.c_str(),
                                                                      lastWorkspaceId.c_str(),
                                                                      currentBaselineWorkspaceId.c_str(),
                                                                      &rawPlan,
                                                                      nullptr,
                                                                      0);
        if (initialResult != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
        {
            THROW_IF_FAILED(initialResult);
        }

        std::wstring resolvedLastWorkspaceId(rawPlan.LastWorkspaceIdLength, L'\0');
        resolvedLastWorkspaceId.push_back(L'\0');
        THROW_IF_FAILED(_getWorkspaceCurrentIdChangePlan(previousWorkspaceId.c_str(),
                                                          nextWorkspaceId.c_str(),
                                                          lastWorkspaceId.c_str(),
                                                          currentBaselineWorkspaceId.c_str(),
                                                          &rawPlan,
                                                          resolvedLastWorkspaceId.data(),
                                                          static_cast<uint32_t>(resolvedLastWorkspaceId.size())));
        resolvedLastWorkspaceId.resize(rawPlan.LastWorkspaceIdLength);
        return { std::move(resolvedLastWorkspaceId), rawPlan.ResetSaveBaseline != 0, rawPlan.StartHeartbeat != 0 };
    }

    LoadedWorkspaceStartupPlan WorkspaceExtLoader::PrepareWorkspaceStartup(const std::wstring& workspaceId) const
    {
        ExtWorkspaceStartupPlan rawPlan{};
        rawPlan.Size = sizeof(rawPlan);
        const auto initialResult = _getWorkspaceStartupPlan(workspaceId.c_str(), &rawPlan, nullptr, 0, nullptr, 0);
        if (initialResult != S_OK && initialResult != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
        {
            THROW_IF_FAILED(initialResult);
        }

        std::vector<uint8_t> visibility(rawPlan.PendingInputVisibilityCount);
        std::wstring nodeIds(rawPlan.PendingNodeIdCharacterCount, L'\0');
        THROW_IF_FAILED(_getWorkspaceStartupPlan(workspaceId.c_str(),
                                                  &rawPlan,
                                                  visibility.empty() ? nullptr : visibility.data(),
                                                  static_cast<uint32_t>(visibility.size()),
                                                  nodeIds.empty() ? nullptr : nodeIds.data(),
                                                  static_cast<uint32_t>(nodeIds.size())));

        LoadedWorkspaceStartupPlan result;
        result.PendingNodeInputVisibility.reserve(visibility.size());
        for (const auto value : visibility)
        {
            result.PendingNodeInputVisibility.emplace_back(value != 0);
        }
        size_t offset{};
        while (offset < nodeIds.size())
        {
            const auto length = std::char_traits<wchar_t>::length(nodeIds.data() + offset);
            result.PendingNodeIds.emplace_back(nodeIds.data() + offset, length);
            offset += length + 1;
        }
        return result;
    }

    LoadedWorkspaceManagerNavigationPlan WorkspaceExtLoader::PrepareWorkspaceManagerNavigation(
        const uint64_t workspaceIndex,
        const uint64_t nodeIndex,
        const uint64_t workspaceCount,
        const uint64_t selectedWorkspaceIndex,
        const int32_t navSelection) const
    {
        ExtWorkspaceManagerNavigationPlan rawPlan{};
        rawPlan.Size = sizeof(rawPlan);
        THROW_IF_FAILED(_getWorkspaceManagerNavigationPlan(workspaceIndex,
                                                            nodeIndex,
                                                            workspaceCount,
                                                            selectedWorkspaceIndex,
                                                            navSelection,
                                                            &rawPlan));
        LoadedWorkspaceManagerNavigationPlan result;
        result.WorkspaceSelection = rawPlan.WorkspaceSelection;
        result.WorkspaceNodeSelection = rawPlan.WorkspaceNodeSelection;
        result.EditorSelection = rawPlan.EditorSelection;
        if (rawPlan.HasResolvedWorkspaceIndex)
        {
            result.ResolvedWorkspaceIndex = gsl::narrow_cast<size_t>(rawPlan.ResolvedWorkspaceIndex);
        }
        if (rawPlan.HasResolvedNodeIndex)
        {
            result.ResolvedNodeIndex = gsl::narrow_cast<size_t>(rawPlan.ResolvedNodeIndex);
        }
        return result;
    }
}
