// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <winrt/Microsoft.Terminal.Settings.Model.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct WorkspaceNode
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring ConnectionRef;
        std::wstring ProfileGuid;
        std::wstring TabColor;
        bool ShowTab{ true };
        std::wstring StartupDirectory;
        std::wstring StartupAction;
        std::wstring OperatingSystem;
        std::wstring ShellType;
        bool ShowInputPanel{ false };
        bool UseNodeNameAsTabTitle{ true };
    };

    struct Workspace
    {
        std::wstring Id;
        std::wstring Name;
        std::wstring Description;
        std::wstring BackgroundColor;
        bool Locked{ true };
        std::vector<WorkspaceNode> Nodes;
    };

    struct WorkspaceStateWindow
    {
        uint64_t WindowId{};
        std::wstring WindowName;
        std::wstring WorkspaceId;
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
        std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> BuildStartupActions(const Workspace& workspace, const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings) const;

        std::vector<Workspace>& Workspaces() noexcept;
        const std::vector<Workspace>& Workspaces() const noexcept;
        void SetWorkspaces(std::vector<Workspace> workspaces);

    private:
        std::vector<Workspace> _workspaces;
    };

    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes);
    std::wstring SanitizeWorkspaceDirectoryName(std::wstring_view value, std::wstring_view fallback = {}) noexcept;
    std::optional<winrt::Windows::UI::Color> ResolveWorkspaceNodeTabColor(const Workspace& workspace,
                                                                           size_t nodeIndex,
                                                                           const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);
    void EnsureWorkspaceNodeTabColors(Workspace& workspace, const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

    class WorkspaceStateManager
    {
    public:
        static std::filesystem::path DefaultPath();
        static WorkspaceStateManager Load();
        static WorkspaceStateManager LoadFromPath(const std::filesystem::path& path);

        bool Save() const;
        bool SaveToPath(const std::filesystem::path& path) const;

        const std::vector<WorkspaceStateWindow>& Windows() const noexcept;
        void UpsertWindow(WorkspaceStateWindow window);
        void RemoveWindow(uint64_t windowId) noexcept;

    private:
        std::vector<WorkspaceStateWindow> _windows;
    };
}
