// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <winrt/Microsoft.Terminal.Settings.Model.h>

#include "WorkspaceApi.h"

namespace terminal::workspace
{
    namespace details
    {
        inline winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager LoadPersistedWorkspaceManager()
        {
            return winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::Load();
        }

        inline winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceStateManager LoadPersistedWorkspaceStateManager()
        {
            return winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceStateManager::Load();
        }

        template<typename TManager, typename TApply>
        std::optional<TManager> PersistManagerChange(TManager manager, TApply&& apply)
        {
            if (!apply(manager) || !manager.Save())
            {
                return std::nullopt;
            }

            return manager;
        }

        template<typename TApply>
        bool PersistStateManagerChange(TApply&& apply)
        {
            auto state = LoadPersistedWorkspaceStateManager();
            apply(state);
            return state.Save();
        }
    }

    struct LoadedWorkspaceFlyoutState
    {
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager Manager;
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceFlyoutState FlyoutState;
    };

    struct LoadedWorkspaceOpenState
    {
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager Manager;
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceOpenPlan OpenPlan;
    };

    struct PersistedWorkspaceRename
    {
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager Manager;
        std::wstring ResolvedWorkspaceName;
    };

    struct PersistedWorkspaceEditorSave
    {
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager Manager;
        winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceEditorSavePlan SavePlan;
    };

    inline std::filesystem::path WorkspaceDefinitionsPath()
    {
        return winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::DefaultPath();
    }

    inline std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> ConsumeInitialWorkspaceStartupActions(const std::wstring_view workspaceId,
                                                                                                                            const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
    {
        if (workspaceId.empty())
        {
            return {};
        }

        const auto manager = details::LoadPersistedWorkspaceManager();
        if (const auto workspace = manager.FindById(workspaceId))
        {
            return manager.BuildStartupActions(*workspace, settings);
        }

        return {};
    }

    inline winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceEditorLoadState LoadWorkspaceEditorState(const std::wstring_view selectedWorkspaceId,
                                                                                                                           const std::wstring_view currentWorkspaceId,
                                                                                                                           const size_t fallbackIndex)
    {
        return winrt::Microsoft::Terminal::Settings::Model::implementation::LoadWorkspaceEditorState(selectedWorkspaceId,
                                                                                                      currentWorkspaceId,
                                                                                                      fallbackIndex);
    }

    inline winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceCurrentState LoadCurrentWorkspaceState(const std::wstring_view currentWorkspaceId,
                                                                                                                         const std::wstring_view defaultDisplayName,
                                                                                                                         const std::wstring_view unsavedTabRowName)
    {
        const auto manager = details::LoadPersistedWorkspaceManager();
        return winrt::Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceCurrentState(currentWorkspaceId,
                                                                                                          manager,
                                                                                                          defaultDisplayName,
                                                                                                          unsavedTabRowName);
    }

    inline winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceStartupState LoadWorkspaceStartupState(const std::wstring_view workspaceId)
    {
        const auto manager = details::LoadPersistedWorkspaceManager();
        return winrt::Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceStartupState(workspaceId, manager);
    }

    inline LoadedWorkspaceFlyoutState LoadWorkspaceFlyoutState(const std::wstring_view currentWorkspaceId)
    {
        auto manager = details::LoadPersistedWorkspaceManager();
        const auto windowState = details::LoadPersistedWorkspaceStateManager();
        return LoadedWorkspaceFlyoutState{
            .Manager = manager,
            .FlyoutState = winrt::Microsoft::Terminal::Settings::Model::implementation::BuildWorkspaceFlyoutState(currentWorkspaceId, manager, windowState),
        };
    }

    inline LoadedWorkspaceOpenState LoadWorkspaceOpenState(const std::wstring_view workspaceId,
                                                           const bool openInNewWindow,
                                                           const std::wstring_view currentWorkspaceId,
                                                           const bool currentWorkspaceNeedsSave)
    {
        auto manager = details::LoadPersistedWorkspaceManager();
        const auto windowState = details::LoadPersistedWorkspaceStateManager();
        return LoadedWorkspaceOpenState{
            .Manager = manager,
            .OpenPlan = winrt::Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceForOpen(workspaceId,
                                                                                                             openInNewWindow,
                                                                                                             currentWorkspaceId,
                                                                                                             currentWorkspaceNeedsSave,
                                                                                                             manager,
                                                                                                             windowState),
        };
    }

    inline std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace> LoadWorkspaceDefinition(const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            return std::nullopt;
        }

        const auto manager = details::LoadPersistedWorkspaceManager();
        if (const auto workspace = manager.FindById(workspaceId))
        {
            return *workspace;
        }

        return std::nullopt;
    }

    inline std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace> LoadResolvedWorkspaceDefinition(const std::wstring_view currentWorkspaceId,
                                                                                                                                 const std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace>& selectedWorkspace)
    {
        const auto manager = details::LoadPersistedWorkspaceManager();
        return winrt::Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceDefinition(currentWorkspaceId,
                                                                                                        selectedWorkspace,
                                                                                                        manager);
    }

    inline std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode> LoadResolvedWorkspaceNode(const std::wstring_view currentWorkspaceId,
                                                                                                                                const std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace>& selectedWorkspace,
                                                                                                                                const std::wstring_view nodeId)
    {
        const auto manager = details::LoadPersistedWorkspaceManager();
        return winrt::Microsoft::Terminal::Settings::Model::implementation::ResolveCurrentWorkspaceNode(currentWorkspaceId,
                                                                                                        selectedWorkspace,
                                                                                                        manager,
                                                                                                        nodeId);
    }

    inline std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace> LoadPersistedWorkspaceForComparison(const std::wstring_view workspaceId)
    {
        return LoadWorkspaceDefinition(workspaceId);
    }

    inline bool IsWorkspaceLocked(const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            return false;
        }

        return LoadCurrentWorkspaceState(workspaceId, L"", L"").Locked;
    }

    inline bool PersistWorkspaceLockedState(const std::wstring_view workspaceId, const bool locked)
    {
        if (workspaceId.empty())
        {
            return false;
        }

        return details::PersistManagerChange(details::LoadPersistedWorkspaceManager(),
                                             [&](auto& manager) {
                                                 return winrt::Microsoft::Terminal::Settings::Model::implementation::SetWorkspaceLocked(manager, workspaceId, locked);
                                             }).has_value();
    }

    inline std::optional<uint64_t> FindOpenWorkspaceWindowId(const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            return std::nullopt;
        }

        const auto state = details::LoadPersistedWorkspaceStateManager();
        return state.FindOpenWorkspaceWindowId(workspaceId);
    }

    inline std::optional<PersistedWorkspaceRename> PersistWorkspaceRename(const std::wstring_view workspaceId, const std::wstring_view newName)
    {
        if (workspaceId.empty() || newName.empty())
        {
            return std::nullopt;
        }

        auto manager = details::LoadPersistedWorkspaceManager();
        const auto resolvedWorkspaceName = winrt::Microsoft::Terminal::Settings::Model::implementation::RenameWorkspace(manager, workspaceId, newName);
        if (!resolvedWorkspaceName.has_value() || !manager.Save())
        {
            return std::nullopt;
        }

        return PersistedWorkspaceRename{
            .Manager = std::move(manager),
            .ResolvedWorkspaceName = std::move(*resolvedWorkspaceName),
        };
    }

    inline std::optional<PersistedWorkspaceEditorSave> PersistWorkspaceEditorState(const winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& editorManager,
                                                                                   const std::wstring_view currentWorkspaceId,
                                                                                   const std::wstring_view lastOpenedWorkspaceId,
                                                                                   const size_t fallbackSelectedWorkspaceIndex)
    {
        const auto persistedManager = details::LoadPersistedWorkspaceManager();
        auto manager = editorManager;
        const auto savePlan = winrt::Microsoft::Terminal::Settings::Model::implementation::PrepareWorkspaceEditorForSave(manager,
                                                                                                                          persistedManager,
                                                                                                                          currentWorkspaceId,
                                                                                                                          lastOpenedWorkspaceId,
                                                                                                                          fallbackSelectedWorkspaceIndex);
        if (!manager.Save())
        {
            return std::nullopt;
        }

        return PersistedWorkspaceEditorSave{
            .Manager = std::move(manager),
            .SavePlan = savePlan,
        };
    }

    inline std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager> PersistWorkspaceNodeInputVisibility(const winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager& preferredManager,
                                                                                                                                             const std::wstring_view workspaceId,
                                                                                                                                             const size_t nodeIndex,
                                                                                                                                             const bool showInputPanel)
    {
        auto manager = preferredManager;
        if (!winrt::Microsoft::Terminal::Settings::Model::implementation::SetWorkspaceNodeInputVisibility(manager, workspaceId, nodeIndex, showInputPanel))
        {
            manager = details::LoadPersistedWorkspaceManager();
            if (!winrt::Microsoft::Terminal::Settings::Model::implementation::SetWorkspaceNodeInputVisibility(manager, workspaceId, nodeIndex, showInputPanel))
            {
                return std::nullopt;
            }
        }

        if (!manager.Save())
        {
            return std::nullopt;
        }

        return manager;
    }

    inline std::optional<winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager> PersistWorkspaceNodeOrder(const std::wstring_view workspaceId,
                                                                                                                                   const std::vector<std::wstring>& orderedNodeIds)
    {
        if (workspaceId.empty() || orderedNodeIds.empty())
        {
            return std::nullopt;
        }

        return details::PersistManagerChange(details::LoadPersistedWorkspaceManager(),
                                             [&](auto& manager) {
                                                 return manager.ReorderWorkspaceNodes(workspaceId, orderedNodeIds);
                                             });
    }

    inline void RemovePersistedWorkspaceWindowState(const uint64_t windowId)
    {
        if (windowId == 0)
        {
            return;
        }

        std::ignore = details::PersistStateManagerChange([&](auto& state) {
            state.RemoveWindow(windowId);
        });
    }

    inline void RefreshPersistedWorkspaceWindowState(const uint64_t windowId,
                                                     const std::wstring_view windowName,
                                                     const std::wstring_view workspaceId)
    {
        if (windowId == 0)
        {
            return;
        }

        std::ignore = details::PersistStateManagerChange([&](auto& state) {
            state.UpdateWindowState(windowId, windowName, workspaceId);
        });
    }
}
