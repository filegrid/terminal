// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Workspace.h"

#include "../../types/inc/utils.hpp"

#include <shlobj.h>

using namespace Microsoft::Console;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    namespace
    {
        std::wstring_view _trimLeft(const std::wstring_view value)
        {
            const auto pos = value.find_first_not_of(L" \t\r\n");
            return pos == std::wstring_view::npos ? std::wstring_view{} : value.substr(pos);
        }

        std::wstring_view _trimRight(const std::wstring_view value)
        {
            const auto pos = value.find_last_not_of(L" \t\r\n");
            return pos == std::wstring_view::npos ? std::wstring_view{} : value.substr(0, pos + 1);
        }

        std::wstring_view _trim(const std::wstring_view value)
        {
            return _trimRight(_trimLeft(value));
        }

        std::wstring _unquote(std::wstring_view value)
        {
            value = _trim(value);
            if (value.size() >= 2)
            {
                const auto first = value.front();
                const auto last = value.back();
                if ((first == L'\'' && last == L'\'') || (first == L'"' && last == L'"'))
                {
                    value.remove_prefix(1);
                    value.remove_suffix(1);
                }
            }

            std::wstring result;
            result.reserve(value.size());
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (value[i] == L'\'' && i + 1 < value.size() && value[i + 1] == L'\'')
                {
                    result.push_back(L'\'');
                    ++i;
                }
                else
                {
                    result.push_back(value[i]);
                }
            }
            return result;
        }

        std::wstring _quote(std::wstring_view value)
        {
            std::wstring result;
            result.reserve(value.size() + 2);
            result.push_back(L'\'');
            for (const auto ch : value)
            {
                result.push_back(ch);
                if (ch == L'\'')
                {
                    result.push_back(L'\'');
                }
            }
            result.push_back(L'\'');
            return result;
        }

        std::wstring _toLower(std::wstring_view value)
        {
            std::wstring lowered;
            lowered.reserve(value.size());
            for (const auto ch : value)
            {
                lowered.push_back(static_cast<wchar_t>(std::towlower(ch)));
            }
            return lowered;
        }

        bool _isWindowsDriveAbsolutePath(std::wstring_view value) noexcept
        {
            return value.size() >= 2 && std::iswalpha(value[0]) != 0 && value[1] == L':';
        }

        bool _looksLikeWindowsPath(std::wstring_view value) noexcept
        {
            return _isWindowsDriveAbsolutePath(value) || value.starts_with(L"\\\\") || value.find(L'\\') != std::wstring_view::npos;
        }

        bool _isSshCommandline(std::wstring_view value)
        {
            const auto lowered = _toLower(value);
            return lowered.find(L"ssh.exe") != std::wstring::npos || lowered == L"ssh" || lowered.starts_with(L"ssh ");
        }

        bool _isWslProfileSource(std::wstring_view source)
        {
            const auto lowered = _toLower(source);
            return lowered == L"microsoft.wsl" || lowered == L"windows.terminal.wsl";
        }

        bool _isWslCommandline(std::wstring_view value)
        {
            const auto lowered = _toLower(value);
            return lowered.find(L"wsl.exe") != std::wstring::npos ||
                   lowered == L"wsl" ||
                   lowered.starts_with(L"wsl ") ||
                   lowered.find(L"bash.exe") != std::wstring::npos;
        }

        std::wstring _quotePowerShellSingle(std::wstring_view value)
        {
            std::wstring quoted;
            quoted.reserve(value.size() + 2);
            quoted.push_back(L'\'');
            for (const auto ch : value)
            {
                quoted.push_back(ch);
                if (ch == L'\'')
                {
                    quoted.push_back(L'\'');
                }
            }
            quoted.push_back(L'\'');
            return quoted;
        }

        std::wstring _appendStartupCommand(std::wstring input, std::wstring_view command)
        {
            const auto trimmed = _trim(command);
            if (trimmed.empty())
            {
                return input;
            }

            input.append(trimmed);
            input.push_back(L'\r');
            return input;
        }

        std::wstring _buildWindowsSshStartupDirectoryInput(std::wstring_view directory)
        {
            auto normalized = std::wstring{ _trim(directory) };
            std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
            if (normalized.empty())
            {
                return {};
            }

            if (_isWindowsDriveAbsolutePath(normalized))
            {
                std::wstring input;
                input.push_back(static_cast<wchar_t>(std::towupper(normalized[0])));
                input.push_back(L':');
                input.push_back(L'\r');

                auto remainder = normalized.substr(2);
                if (remainder.empty())
                {
                    return input;
                }

                if (remainder.front() != L'\\')
                {
                    remainder.insert(remainder.begin(), L'\\');
                }

                if (remainder == L"\\")
                {
                    input.append(L"cd \\");
                }
                else
                {
                    input.append(L"cd \"");
                    input.append(remainder);
                    input.append(L"\"");
                }
                input.push_back(L'\r');
                return input;
            }

            std::wstring input{ L"cd \"" };
            input.append(normalized);
            input.append(L"\"\r");
            return input;
        }

        bool _isSshTransportNode(const WorkspaceNode& node, const Model::Profile& profile)
        {
            if (profile)
            {
                const auto source = _toLower(profile.Source().c_str());
                const auto commandline = _toLower(profile.Commandline().c_str());

                if (_isWslProfileSource(source) || _isWslCommandline(commandline))
                {
                    return false;
                }
                if (source == L"windows.terminal.ssh")
                {
                    return true;
                }

                if (_isSshCommandline(commandline))
                {
                    return true;
                }

                if (source == L"windows.terminal.powershellcore" ||
                    commandline.find(L"powershell.exe") != std::wstring::npos ||
                    commandline.find(L"pwsh.exe") != std::wstring::npos ||
                    commandline == L"powershell" ||
                    commandline == L"pwsh" ||
                    commandline.starts_with(L"powershell ") ||
                    commandline.starts_with(L"pwsh ") ||
                    commandline.find(L"cmd.exe") != std::wstring::npos ||
                    commandline == L"cmd" ||
                    commandline.starts_with(L"cmd "))
                {
                    return false;
                }
            }

            if (_toLower(node.ShellType) == L"ssh")
            {
                return true;
            }

            return false;
        }

        std::wstring _buildSshStartupInput(const WorkspaceNode& node)
        {
            const auto startupDirectory = _trim(node.StartupDirectory);
            if (startupDirectory.empty())
            {
                return {};
            }

            const auto operatingSystem = _toLower(node.OperatingSystem);
            if (operatingSystem == L"windows" || _looksLikeWindowsPath(startupDirectory))
            {
                if (_toLower(node.ShellType) == L"powershell")
                {
                    std::wstring input{ L"Set-Location -LiteralPath " };
                    input.append(_quotePowerShellSingle(startupDirectory));
                    input.push_back(L'\r');
                    return input;
                }
                return _buildWindowsSshStartupDirectoryInput(startupDirectory);
            }

            std::wstring input{ L"cd \"" };
            input.append(startupDirectory);
            input.append(L"\"\r");
            return input;
        }

        std::pair<std::wstring, std::wstring> _parseKeyValue(std::wstring_view content)
        {
            if (content.starts_with(L"- "))
            {
                content.remove_prefix(2);
            }

            const auto colon = content.find(L':');
            if (colon == std::wstring_view::npos)
            {
                return {};
            }

            std::wstring key{ _trim(content.substr(0, colon)) };
            std::wstring value{ _unquote(content.substr(colon + 1)) };
            return { std::move(key), std::move(value) };
        }

        bool _parseBool(const std::wstring& value, const bool fallback) noexcept
        {
            if (value == L"true")
            {
                return true;
            }
            if (value == L"false")
            {
                return false;
            }
            return fallback;
        }

        void _applyWindowStateField(WorkspaceStateWindow& window, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"windowId")
            {
                window.WindowId = value.empty() ? 0ull : static_cast<uint64_t>(_wtoi64(value.c_str()));
            }
            else if (key == L"windowName")
            {
                window.WindowName = value;
            }
            else if (key == L"workspaceId")
            {
                window.WorkspaceId = value;
            }
        }

        void _applyWorkspaceField(Workspace& workspace, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"id")
            {
                workspace.Id = value;
            }
            else if (key == L"name")
            {
                workspace.Name = value;
            }
            else if (key == L"description")
            {
                workspace.Description = value;
            }
            else if (key == L"backgroundColor")
            {
                workspace.BackgroundColor = value;
            }
            else if (key == L"locked")
            {
                workspace.Locked = _parseBool(value, workspace.Locked);
            }
        }

        void _applyNodeField(WorkspaceNode& node, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"id")
            {
                node.Id = value;
            }
            else if (key == L"name")
            {
                node.Name = value;
            }
            else if (key == L"connectionRef")
            {
                node.ConnectionRef = value;
            }
            else if (key == L"profileGuid")
            {
                node.ProfileGuid = value;
            }
            else if (key == L"startupDirectory")
            {
                node.StartupDirectory = value;
            }
            else if (key == L"startupAction")
            {
                node.StartupAction = value;
            }
            else if (key == L"operatingSystem")
            {
                node.OperatingSystem = value;
            }
            else if (key == L"shellType")
            {
                node.ShellType = value;
            }
            else if (key == L"showInputPanel")
            {
                node.ShowInputPanel = _parseBool(value, node.ShowInputPanel);
            }
            else if (key == L"useNodeNameAsTabTitle")
            {
                node.UseNodeNameAsTabTitle = _parseBool(value, node.UseNodeNameAsTabTitle);
            }
        }

        void _appendNodeIfPresent(std::optional<WorkspaceNode>& currentNode, std::optional<Workspace>& currentWorkspace)
        {
            if (currentNode && currentWorkspace)
            {
                currentWorkspace->Nodes.emplace_back(std::move(*currentNode));
                currentNode.reset();
            }
        }

        void _appendWorkspaceIfPresent(std::optional<Workspace>& currentWorkspace, std::vector<Workspace>& workspaces)
        {
            if (currentWorkspace)
            {
                workspaces.emplace_back(std::move(*currentWorkspace));
                currentWorkspace.reset();
            }
        }

        std::filesystem::path _workspaceRoot()
        {
            const auto userProfile = wil::TryGetEnvironmentVariableW<std::wstring>(L"USERPROFILE");
            if (!userProfile.empty())
            {
                return std::filesystem::path{ userProfile } / L".wt";
            }

            wil::unique_cotaskmem_string profileFolder;
            THROW_IF_FAILED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &profileFolder));
            return std::filesystem::path{ profileFolder.get() } / L".wt";
        }

        std::filesystem::path _workspaceStatePath()
        {
            const auto modulePath = wil::GetModuleFileNameW<std::wstring>(nullptr);
            if (modulePath.empty())
            {
                return _workspaceRoot() / L"workspace-window-state.yaml";
            }

            std::wstringstream stream;
            const auto module = std::filesystem::path{ modulePath };
            stream << L"workspace-window-state-" << module.stem().wstring() << L"-" << std::hex << static_cast<unsigned long long>(std::hash<std::wstring>{}(modulePath)) << L".yaml";
            return _workspaceRoot() / stream.str();
        }
    }

    std::filesystem::path WorkspaceManager::DefaultPath()
    {
        return _workspaceRoot() / L"workspaces.yaml";
    }

    std::filesystem::path WorkspaceStateManager::DefaultPath()
    {
        return _workspaceStatePath();
    }

    WorkspaceManager WorkspaceManager::Load()
    {
        return LoadFromPath(DefaultPath());
    }

    WorkspaceManager WorkspaceManager::LoadFromPath(const std::filesystem::path& path)
    {
        WorkspaceManager manager;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
        {
            return manager;
        }

        std::ifstream input{ path, std::ios::binary };
        if (!input)
        {
            return manager;
        }

        std::string utf8{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
        std::wstring content{ til::u8u16(utf8) };
        std::wistringstream stream{ content };

        std::vector<Workspace> workspaces;
        std::optional<Workspace> currentWorkspace;
        std::optional<WorkspaceNode> currentNode;
        bool inNodes = false;

        for (std::wstring line; std::getline(stream, line);)
        {
            const auto contentView = _trimRight(std::wstring_view{ line });
            const auto trimmed = _trim(contentView);
            if (trimmed.empty() || trimmed.starts_with(L"#"))
            {
                continue;
            }

            const auto indent = contentView.find_first_not_of(L' ');
            const auto safeIndent = indent == std::wstring_view::npos ? 0u : gsl::narrow_cast<uint32_t>(indent);

            if (safeIndent == 0)
            {
                if (trimmed == L"workspaces:")
                {
                    continue;
                }

                if (trimmed.starts_with(L"version:"))
                {
                    continue;
                }

            }

            if (safeIndent == 2 && trimmed.starts_with(L"- "))
            {
                _appendNodeIfPresent(currentNode, currentWorkspace);
                _appendWorkspaceIfPresent(currentWorkspace, workspaces);

                currentWorkspace.emplace();
                inNodes = false;

                const auto [key, value] = _parseKeyValue(trimmed);
                _applyWorkspaceField(*currentWorkspace, key, value);
                continue;
            }

            if (safeIndent == 4 && trimmed == L"nodes:")
            {
                inNodes = true;
                continue;
            }

            if (safeIndent == 4 && currentWorkspace && !inNodes)
            {
                const auto [key, value] = _parseKeyValue(trimmed);
                _applyWorkspaceField(*currentWorkspace, key, value);
                continue;
            }

            if (safeIndent == 6 && trimmed.starts_with(L"- "))
            {
                _appendNodeIfPresent(currentNode, currentWorkspace);
                currentNode.emplace();
                const auto [key, value] = _parseKeyValue(trimmed);
                _applyNodeField(*currentNode, key, value);
                continue;
            }

            if (safeIndent == 8 && currentNode)
            {
                const auto [key, value] = _parseKeyValue(trimmed);
                _applyNodeField(*currentNode, key, value);
                continue;
            }
        }

        _appendNodeIfPresent(currentNode, currentWorkspace);
        _appendWorkspaceIfPresent(currentWorkspace, workspaces);

        manager.SetWorkspaces(std::move(workspaces));
        return manager;
    }

    bool WorkspaceManager::Save() const
    {
        return SaveToPath(DefaultPath());
    }

    bool WorkspaceManager::SaveToPath(const std::filesystem::path& path) const
    {
        std::error_code ec;
        const auto parent = path.parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent, ec);
            if (ec)
            {
                return false;
            }
        }

        std::wostringstream stream;
        stream << L"version: 1\n";
        stream << L"workspaces:\n";

        for (const auto& workspace : _workspaces)
        {
            stream << L"  - id: " << _quote(workspace.Id) << L"\n";
            stream << L"    name: " << _quote(workspace.Name) << L"\n";
            if (!workspace.Description.empty())
            {
                stream << L"    description: " << _quote(workspace.Description) << L"\n";
            }
            if (!workspace.BackgroundColor.empty())
            {
                stream << L"    backgroundColor: " << _quote(workspace.BackgroundColor) << L"\n";
            }
            stream << L"    locked: " << (workspace.Locked ? L"true" : L"false") << L"\n";
            stream << L"    nodes:\n";
            for (const auto& node : workspace.Nodes)
            {
                stream << L"      - id: " << _quote(node.Id) << L"\n";
                stream << L"        name: " << _quote(node.Name) << L"\n";
                if (!node.ConnectionRef.empty())
                {
                    stream << L"        connectionRef: " << _quote(node.ConnectionRef) << L"\n";
                }
                if (!node.ProfileGuid.empty())
                {
                    stream << L"        profileGuid: " << _quote(node.ProfileGuid) << L"\n";
                }
                stream << L"        startupDirectory: " << _quote(node.StartupDirectory) << L"\n";
                stream << L"        startupAction: " << _quote(node.StartupAction) << L"\n";
                if (!node.OperatingSystem.empty())
                {
                    stream << L"        operatingSystem: " << _quote(node.OperatingSystem) << L"\n";
                }
                if (!node.ShellType.empty())
                {
                    stream << L"        shellType: " << _quote(node.ShellType) << L"\n";
                }
                stream << L"        showInputPanel: " << (node.ShowInputPanel ? L"true" : L"false") << L"\n";
                stream << L"        useNodeNameAsTabTitle: " << (node.UseNodeNameAsTabTitle ? L"true" : L"false") << L"\n";
            }
        }

        const auto utf8 = til::u16u8(stream.str());
        std::ofstream output{ path, std::ios::binary | std::ios::trunc };
        if (!output)
        {
            return false;
        }

        output.write(utf8.data(), gsl::narrow_cast<std::streamsize>(utf8.size()));
        return output.good();
    }

    WorkspaceStateManager WorkspaceStateManager::Load()
    {
        return LoadFromPath(DefaultPath());
    }

    WorkspaceStateManager WorkspaceStateManager::LoadFromPath(const std::filesystem::path& path)
    {
        WorkspaceStateManager manager;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
        {
            return manager;
        }

        std::ifstream input{ path, std::ios::binary };
        if (!input)
        {
            return manager;
        }

        std::string utf8{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
        std::wstring content{ til::u8u16(utf8) };
        std::wistringstream stream{ content };

        enum class ParseSection
        {
            None,
            Windows,
        };

        ParseSection section = ParseSection::None;
        std::optional<WorkspaceStateWindow> currentWindow;

        auto appendWindow = [&]() {
            if (currentWindow)
            {
                manager.UpsertWindow(std::move(*currentWindow));
                currentWindow.reset();
            }
        };

        for (std::wstring line; std::getline(stream, line);)
        {
            const auto contentView = _trimRight(std::wstring_view{ line });
            const auto trimmed = _trim(contentView);
            if (trimmed.empty() || trimmed.starts_with(L"#"))
            {
                continue;
            }

            const auto indent = contentView.find_first_not_of(L' ');
            const auto safeIndent = indent == std::wstring_view::npos ? 0u : gsl::narrow_cast<uint32_t>(indent);

            if (safeIndent == 0)
            {
                appendWindow();

                if (trimmed.starts_with(L"version:"))
                {
                    section = ParseSection::None;
                    continue;
                }
                if (trimmed == L"windows:")
                {
                    section = ParseSection::Windows;
                    continue;
                }
                section = ParseSection::None;
                continue;
            }

            if (section == ParseSection::Windows && safeIndent == 2 && trimmed.starts_with(L"- "))
            {
                appendWindow();
                currentWindow.emplace();
                const auto [key, value] = _parseKeyValue(trimmed);
                _applyWindowStateField(*currentWindow, key, value);
                continue;
            }

            if (section == ParseSection::Windows && safeIndent == 4 && currentWindow)
            {
                const auto [key, value] = _parseKeyValue(trimmed);
                _applyWindowStateField(*currentWindow, key, value);
                continue;
            }
        }

        appendWindow();
        return manager;
    }

    bool WorkspaceStateManager::Save() const
    {
        return SaveToPath(DefaultPath());
    }

    bool WorkspaceStateManager::SaveToPath(const std::filesystem::path& path) const
    {
        std::error_code ec;
        const auto parent = path.parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent, ec);
            if (ec)
            {
                return false;
            }
        }

        std::wostringstream stream;
        stream << L"version: 1\n";
        stream << L"windows:\n";
        for (const auto& window : _windows)
        {
            stream << L"  - windowId: " << _quote(std::to_wstring(window.WindowId)) << L"\n";
            if (!window.WindowName.empty())
            {
                stream << L"    windowName: " << _quote(window.WindowName) << L"\n";
            }
            if (!window.WorkspaceId.empty())
            {
                stream << L"    workspaceId: " << _quote(window.WorkspaceId) << L"\n";
            }
        }

        const auto utf8 = til::u16u8(stream.str());
        std::ofstream output{ path, std::ios::binary | std::ios::trunc };
        if (!output)
        {
            return false;
        }

        output.write(utf8.data(), gsl::narrow_cast<std::streamsize>(utf8.size()));
        return output.good();
    }

    const Workspace* WorkspaceManager::FindById(const std::wstring_view id) const noexcept
    {
        const auto it = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == id;
        });
        return it == _workspaces.end() ? nullptr : &*it;
    }

    bool WorkspaceManager::ReorderWorkspaceNodes(const std::wstring_view workspaceId, const std::vector<std::wstring>& orderedNodeIds)
    {
        const auto workspaceIt = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == _workspaces.end())
        {
            return false;
        }

        auto& nodes = workspaceIt->Nodes;
        if (orderedNodeIds.size() != nodes.size())
        {
            return false;
        }

        std::vector<WorkspaceNode> reorderedNodes;
        reorderedNodes.reserve(nodes.size());
        std::vector<bool> consumed(nodes.size(), false);

        for (const auto& nodeId : orderedNodeIds)
        {
            const auto nodeIt = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
                return node.Id == nodeId;
            });
            if (nodeIt == nodes.end())
            {
                return false;
            }

            const auto index = gsl::narrow_cast<size_t>(std::distance(nodes.begin(), nodeIt));
            if (consumed[index])
            {
                return false;
            }

            reorderedNodes.emplace_back(*nodeIt);
            consumed[index] = true;
        }

        nodes = std::move(reorderedNodes);
        return true;
    }

    std::vector<Model::ActionAndArgs> WorkspaceManager::BuildStartupActions(const Workspace& workspace, const Model::CascadiaSettings& settings) const
    {
        std::vector<Model::ActionAndArgs> actions;

        for (const auto& node : workspace.Nodes)
        {
            Model::NewTerminalArgs terminalArgs;
            Model::Profile profile{ nullptr };

            if (!node.Name.empty())
            {
                terminalArgs.TabTitle(node.Name);
            }
            terminalArgs.SuppressApplicationTitle(node.UseNodeNameAsTabTitle);

            if (!node.ProfileGuid.empty())
            {
                if (node.ProfileGuid.size() == 38 && node.ProfileGuid.front() == L'{')
                {
                    profile = settings.FindProfile(Utils::GuidFromString(node.ProfileGuid.c_str()));
                }
            }

            if (!profile)
            {
                profile = settings.FindProfile(settings.GlobalSettings().DefaultProfile());
            }

            if (!profile)
            {
                continue;
            }

            const auto isSshTransport = _isSshTransportNode(node, profile);
            if (!node.StartupDirectory.empty() && !isSshTransport)
            {
                terminalArgs.StartingDirectory(node.StartupDirectory);
            }

            terminalArgs.Profile(Utils::GuidToString(profile.Guid()));

            Model::ActionAndArgs newTabAction;
            newTabAction.Action(ShortcutAction::NewTab);
            newTabAction.Args(Model::NewTabArgs{ terminalArgs });
            actions.emplace_back(std::move(newTabAction));

            auto startupInput = isSshTransport ? _buildSshStartupInput(node) : std::wstring{};
            startupInput = _appendStartupCommand(std::move(startupInput), node.StartupAction);
            if (!startupInput.empty())
            {
                Model::ActionAndArgs startupAction;
                startupAction.Action(ShortcutAction::SendInput);
                startupAction.Args(Model::SendInputArgs{ winrt::hstring{ startupInput } });
                actions.emplace_back(std::move(startupAction));
            }
        }

        return actions;
    }

    std::vector<Workspace>& WorkspaceManager::Workspaces() noexcept
    {
        return _workspaces;
    }

    const std::vector<Workspace>& WorkspaceManager::Workspaces() const noexcept
    {
        return _workspaces;
    }

    void WorkspaceManager::SetWorkspaces(std::vector<Workspace> workspaces)
    {
        _workspaces = std::move(workspaces);
    }

    const std::vector<WorkspaceStateWindow>& WorkspaceStateManager::Windows() const noexcept
    {
        return _windows;
    }

    void WorkspaceStateManager::UpsertWindow(WorkspaceStateWindow window)
    {
        const auto it = std::find_if(_windows.begin(), _windows.end(), [&](const auto& candidate) {
            return candidate.WindowId == window.WindowId;
        });

        if (it != _windows.end())
        {
            *it = std::move(window);
        }
        else
        {
            _windows.emplace_back(std::move(window));
        }
    }

    void WorkspaceStateManager::RemoveWindow(const uint64_t windowId) noexcept
    {
        _windows.erase(std::remove_if(_windows.begin(), _windows.end(), [&](const auto& window) {
            return window.WindowId == windowId;
        }), _windows.end());
    }
}
