// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "WorkspaceApi.h"
#include "../../core/workspace/WorkspacePersistencePaths.h"

namespace terminal::workspace
{
    namespace impl = winrt::Microsoft::Terminal::Settings::Model::implementation;

    using Workspace = impl::Workspace;
    using WorkspaceNode = impl::WorkspaceNode;
    using WorkspaceManager = impl::WorkspaceManager;
    using WorkspaceStateManager = impl::WorkspaceStateManager;
    using WorkspaceCurrentState = impl::WorkspaceCurrentState;
    using WorkspaceSaveTargetState = impl::WorkspaceSaveTargetState;
    using WorkspaceOpenExecutionDisposition = impl::WorkspaceOpenExecutionDisposition;
    using WorkspaceNodeRuntimeRegistrationInput = impl::WorkspaceNodeRuntimeRegistrationInput;
    using WorkspaceNodeRuntimeStatePlan = impl::WorkspaceNodeRuntimeStatePlan;
    using WorkspaceEditorDefinitionAddResult = impl::WorkspaceEditorDefinitionAddResult;
    using WorkspaceEditorNodeAddResult = impl::WorkspaceEditorNodeAddResult;
    using PersistedWorkspaceRename = impl::PersistedWorkspaceRename;
    using PersistedWorkspaceEditorSave = impl::PersistedWorkspaceEditorSave;

    inline std::filesystem::path WorkspaceDefinitionsPath()
    {
        return terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
    }

    inline std::optional<uint64_t> FindOpenWorkspaceWindowId(const std::wstring_view workspaceId)
    {
        return impl::WorkspaceStateManager::LoadRuntime().FindOpenWorkspaceWindowId(workspaceId);
    }

    inline void RemovePersistedWorkspaceWindowState(const uint64_t windowId)
    {
        impl::RemovePersistedWorkspaceWindowState(windowId);
    }

    inline void RefreshPersistedWorkspaceWindowState(const uint64_t windowId,
                                                     const std::wstring_view processName,
                                                     const std::wstring_view workspaceId)
    {
        impl::RefreshPersistedWorkspaceWindowState(windowId, processName, workspaceId);
    }

    inline auto LoadWorkspaceEditorState(const std::wstring_view selectedWorkspaceId,
                                         const std::wstring_view currentWorkspaceId,
                                         const size_t fallbackIndex)
    {
        return impl::LoadWorkspaceEditorState(selectedWorkspaceId, currentWorkspaceId, fallbackIndex);
    }

    inline auto LoadWorkspaceFlyoutState(const std::wstring_view currentWorkspaceId)
    {
        return impl::LoadWorkspaceFlyoutState(currentWorkspaceId);
    }

    inline auto LoadWorkspaceOpenExecutionState(const std::wstring_view workspaceId,
                                                const bool openInNewWindow,
                                                const std::wstring_view currentWorkspaceId,
                                                const bool currentWorkspaceNeedsSave,
                                                const bool hasTabsToReplace,
                                                const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
    {
        return impl::LoadWorkspaceOpenExecutionState(workspaceId,
                                                     openInNewWindow,
                                                     currentWorkspaceId,
                                                     currentWorkspaceNeedsSave,
                                                     hasTabsToReplace,
                                                     settings);
    }

    inline auto LoadWorkspaceDefinition(const std::wstring_view workspaceId)
    {
        return impl::LoadWorkspaceDefinition(workspaceId);
    }

    inline auto LoadResolvedWorkspaceDefinition(const std::wstring_view currentWorkspaceId,
                                                const std::optional<Workspace>& selectedWorkspace)
    {
        return impl::LoadResolvedWorkspaceDefinition(currentWorkspaceId, selectedWorkspace);
    }

    inline auto LoadResolvedWorkspaceNode(const std::wstring_view currentWorkspaceId,
                                          const std::optional<Workspace>& selectedWorkspace,
                                          const std::wstring_view nodeId)
    {
        return impl::LoadResolvedWorkspaceNode(currentWorkspaceId, selectedWorkspace, nodeId);
    }

    inline auto LoadCurrentWorkspaceState(const std::wstring_view currentWorkspaceId,
                                          const std::wstring_view defaultDisplayName,
                                          const std::wstring_view unsavedTabRowName)
    {
        return impl::LoadCurrentWorkspaceState(currentWorkspaceId, defaultDisplayName, unsavedTabRowName);
    }

    inline auto LoadWorkspaceSaveTargetState(const std::wstring_view currentWorkspaceId,
                                             const std::wstring_view lastWorkspaceId)
    {
        return impl::LoadWorkspaceSaveTargetState(currentWorkspaceId, lastWorkspaceId);
    }

    inline auto LoadWorkspaceStartupState(const std::wstring_view workspaceId)
    {
        return impl::LoadWorkspaceStartupState(workspaceId);
    }

    inline auto ResolveWorkspaceStartupState(const Workspace& workspace,
                                             const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
    {
        return impl::ResolveWorkspaceStartupState(workspace, settings);
    }

    inline auto LoadResolvedWorkspaceBackedTabIndex(const std::wstring_view currentWorkspaceId,
                                                    const std::optional<Workspace>& selectedWorkspace,
                                                    const std::vector<impl::WorkspaceLiveTabSnapshot>& tabs,
                                                    const size_t targetTabIndex)
    {
        return impl::LoadResolvedWorkspaceBackedTabIndex(currentWorkspaceId, selectedWorkspace, tabs, targetTabIndex);
    }

    inline auto LoadResolvedWorkspaceBackedTabNode(const std::wstring_view currentWorkspaceId,
                                                   const std::optional<Workspace>& selectedWorkspace,
                                                   const std::vector<impl::WorkspaceLiveTabSnapshot>& tabs,
                                                   const size_t targetTabIndex)
    {
        return impl::LoadResolvedWorkspaceBackedTabNode(currentWorkspaceId, selectedWorkspace, tabs, targetTabIndex);
    }

    inline auto FindResolvedWorkspaceBackedTabSnapshotIndex(const std::wstring_view currentWorkspaceId,
                                                            const std::optional<Workspace>& selectedWorkspace,
                                                            const std::vector<impl::WorkspaceLiveTabSnapshot>& tabs,
                                                            const size_t nodeIndex)
    {
        return impl::FindResolvedWorkspaceBackedTabSnapshotIndex(currentWorkspaceId, selectedWorkspace, tabs, nodeIndex);
    }

    inline auto PersistWorkspaceNodeInputVisibility(const WorkspaceManager& preferredManager,
                                                    const std::wstring_view workspaceId,
                                                    const size_t nodeIndex,
                                                    const bool showInputPanel)
    {
        return impl::PersistWorkspaceNodeInputVisibility(preferredManager, workspaceId, nodeIndex, showInputPanel);
    }

    inline auto PersistWorkspaceNodeOrder(const std::wstring_view workspaceId,
                                          const std::vector<std::wstring>& orderedNodeIds)
    {
        return impl::PersistWorkspaceNodeOrder(workspaceId, orderedNodeIds);
    }

    inline auto PersistWorkspaceEditorState(const WorkspaceManager& editorManager,
                                            const std::wstring_view currentWorkspaceId,
                                            const std::wstring_view lastOpenedWorkspaceId,
                                            const size_t fallbackSelectedWorkspaceIndex)
    {
        return impl::PersistWorkspaceEditorState(editorManager, currentWorkspaceId, lastOpenedWorkspaceId, fallbackSelectedWorkspaceIndex);
    }

    inline auto PersistWorkspaceRename(const std::wstring_view workspaceId, const std::wstring_view newName)
    {
        return impl::PersistWorkspaceRename(workspaceId, newName);
    }

    bool PersistWorkspaceLockedState(std::wstring_view workspaceId, bool locked);

    inline auto AddWorkspaceDefinition(WorkspaceManager& manager,
                                       const std::wstring_view generatedName,
                                       const std::optional<size_t> templateIndex)
    {
        return impl::AddWorkspaceDefinition(manager, generatedName, templateIndex);
    }

    inline auto AddWorkspaceNode(WorkspaceManager& manager,
                                 const size_t workspaceIndex,
                                 const std::wstring_view generatedName,
                                 const std::wstring_view defaultProfileGuid,
                                 const std::wstring_view defaultProfileName)
    {
        return impl::AddWorkspaceNode(manager, workspaceIndex, generatedName, defaultProfileGuid, defaultProfileName);
    }

    inline auto PrepareWorkspaceCurrentIdChange(const std::wstring_view previousWorkspaceId,
                                                const std::wstring_view nextWorkspaceId,
                                                const std::wstring_view lastWorkspaceId,
                                                const std::wstring_view currentBaselineWorkspaceId)
    {
        return impl::PrepareWorkspaceCurrentIdChange(previousWorkspaceId, nextWorkspaceId, lastWorkspaceId, currentBaselineWorkspaceId);
    }

    inline auto RefreshWorkspaceWindowState(const std::uint64_t windowId, const std::wstring_view currentWorkspaceId)
    {
        return impl::RefreshWorkspaceWindowState(windowId, currentWorkspaceId);
    }

    inline auto PickWorkspacePaletteColor(const std::unordered_set<std::wstring>& usedColors,
                                          const size_t fallbackIndex,
                                          const std::wstring_view excludedColor = {})
    {
        return impl::PickWorkspacePaletteColor(usedColors, fallbackIndex, excludedColor);
    }

    inline void EnsureWorkspaceNodeTabColors(Workspace& workspace,
                                             const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
    {
        impl::EnsureWorkspaceNodeTabColors(workspace, settings);
    }

    inline auto ResolveWorkspaceNodeTabColor(const Workspace& workspace,
                                             const size_t nodeIndex,
                                             const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
    {
        return impl::ResolveWorkspaceNodeTabColor(workspace, nodeIndex, settings);
    }

    inline auto LoadPersistedWorkspaceForComparison(const std::wstring_view workspaceId)
    {
        return impl::LoadWorkspaceDefinition(workspaceId);
    }
}
