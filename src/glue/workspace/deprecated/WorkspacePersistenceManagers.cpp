    namespace
    {
        std::optional<std::vector<PersistedWorkspaceDirectory>> _enumeratePersistedWorkspaceDirectoriesIfPresent(const std::filesystem::path& path)
        {
            std::error_code ec;
            if (path.empty() || !std::filesystem::exists(path, ec) || ec)
            {
                return std::vector<PersistedWorkspaceDirectory>{};
            }

            if (!std::filesystem::is_directory(path, ec) || ec)
            {
                return std::vector<PersistedWorkspaceDirectory>{};
            }

            return _enumeratePersistedWorkspaceDirectories(path);
        }

        bool _removeLegacyWorkspaceDirectoriesIfPresent()
        {
            const auto legacyPath = terminal::workspacepaths::ResolveLegacyWorkspaceDefinitionsPath();
            const auto currentPath = terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
            if (legacyPath.empty() || legacyPath == currentPath)
            {
                return true;
            }

            std::error_code ec;
            if (!std::filesystem::exists(legacyPath, ec) || ec || !std::filesystem::is_directory(legacyPath, ec) || ec)
            {
                return !ec;
            }

            for (const auto& existingWorkspaceEntry : std::filesystem::directory_iterator(legacyPath, ec))
            {
                if (ec)
                {
                    return false;
                }
                if (!existingWorkspaceEntry.is_directory())
                {
                    continue;
                }
                if (!std::filesystem::exists(existingWorkspaceEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceMetadataFileName }, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                std::filesystem::remove_all(existingWorkspaceEntry.path(), ec);
                if (ec)
                {
                    return false;
                }
            }

            std::filesystem::remove(legacyPath / std::filesystem::path{ terminal::workspacepaths::LegacyWorkspaceFileName }, ec);
            return !ec;
        }

        bool _removeLegacyWorkspaceStateFilesIfPresent()
        {
            const auto legacyPath = terminal::workspacepaths::ResolveLegacyWorkspaceDefinitionsPath();
            const auto currentPath = terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
            if (legacyPath.empty() || legacyPath == currentPath)
            {
                return true;
            }

            std::error_code ec;
            if (!std::filesystem::exists(legacyPath, ec) || ec || !std::filesystem::is_directory(legacyPath, ec) || ec)
            {
                return !ec;
            }

            for (const auto& existingEntry : std::filesystem::directory_iterator(legacyPath, ec))
            {
                if (ec)
                {
                    return false;
                }
                if (!existingEntry.is_regular_file())
                {
                    continue;
                }

                const auto fileName = existingEntry.path().filename().wstring();
                if (fileName == terminal::workspacepaths::WorkspaceStateFileName ||
                    fileName.rfind(std::wstring{ terminal::workspacepaths::LegacyWorkspaceStateFilePrefix }, 0) == 0)
                {
                    std::filesystem::remove(existingEntry.path(), ec);
                    if (ec)
                    {
                        return false;
                    }
                }
            }

            return true;
        }
    }

    WorkspaceManager WorkspaceManager::Load()
    {
        return LoadFromPath(DefaultPath());
    }

    WorkspaceManager WorkspaceManager::LoadFromPath(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
        {
            return {};
        }

        if (std::filesystem::is_regular_file(path, ec) && !ec)
        {
            return {};
        }

        WorkspaceManager manager;
        if (!std::filesystem::is_directory(path, ec) || ec)
        {
            return manager;
        }

        const auto persistedWorkspaces = _enumeratePersistedWorkspaceDirectories(path);
        if (!persistedWorkspaces.has_value())
        {
            return manager;
        }

        std::vector<Workspace> workspaces;
        workspaces.reserve(persistedWorkspaces->size());
        for (auto persistedWorkspace : *persistedWorkspaces)
        {
            auto workspace = std::move(persistedWorkspace.Definition);
            std::vector<WorkspaceNode> loadedNodes;
            for (const auto& nodeEntry : std::filesystem::directory_iterator(persistedWorkspace.Directory, ec))
            {
                if (ec)
                {
                    return {};
                }
                if (!nodeEntry.is_directory())
                {
                    continue;
                }

                const auto nodeFile = nodeEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceNodeMetadataFileName };
                if (!std::filesystem::exists(nodeFile, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                auto nodeMetadata = _loadWorkspaceNodeMetadataFile(nodeFile);
                if (!nodeMetadata.has_value())
                {
                    continue;
                }

                auto node = std::move(*nodeMetadata);
                node.Name = nodeEntry.path().filename().wstring();
                node.Id = node.Name;
                loadedNodes.emplace_back(std::move(node));
            }

            std::stable_sort(loadedNodes.begin(), loadedNodes.end(), [](const auto& lhs, const auto& rhs) {
                return _toLower(lhs.Name) < _toLower(rhs.Name);
            });
            workspace.Nodes.reserve(loadedNodes.size());
            for (auto& node : loadedNodes)
            {
                workspace.Nodes.emplace_back(std::move(node));
            }
            workspaces.emplace_back(std::move(workspace));
        }

        manager.SetWorkspaces(std::move(workspaces));
        return manager;
    }

    bool WorkspaceManager::Save() const
    {
        return SaveToPath(DefaultPath());
    }

    bool WorkspaceManager::SaveToPath(const std::filesystem::path& path) const
    {
        if (path.has_extension())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec)
        {
            return false;
        }

        std::unordered_set<std::wstring> desiredWorkspaceDirs;
        for (size_t workspaceIndex = 0; workspaceIndex < _workspaces.size(); ++workspaceIndex)
        {
            const auto& workspace = _workspaces.at(workspaceIndex);
            const auto workspaceDirName = _makeUniqueDirectoryName(
                SanitizeWorkspaceDirectoryName(workspace.Name, L"workspace"),
                desiredWorkspaceDirs);
            const auto workspaceDir = path / workspaceDirName;
            std::filesystem::create_directories(workspaceDir, ec);
            if (ec)
            {
                return false;
            }

            if (!_writeUtf8TextFile(workspaceDir / std::filesystem::path{ terminal::workspacepaths::WorkspaceMetadataFileName },
                                    _serializeWorkspaceMetadata(workspace)))
            {
                return false;
            }

            std::unordered_set<std::wstring> desiredNodeDirs;
            for (size_t nodeIndex = 0; nodeIndex < workspace.Nodes.size(); ++nodeIndex)
            {
                const auto& node = workspace.Nodes.at(nodeIndex);
                const auto nodeDirName = _makeUniqueDirectoryName(
                    SanitizeWorkspaceDirectoryName(node.Name, L"tab"),
                    desiredNodeDirs);
                const auto nodeDir = workspaceDir / nodeDirName;
                std::filesystem::create_directories(nodeDir, ec);
                if (ec)
                {
                    return false;
                }

                if (!_writeUtf8TextFile(nodeDir / std::filesystem::path{ terminal::workspacepaths::WorkspaceNodeMetadataFileName },
                                        _serializeWorkspaceNodeMetadata(node)))
                {
                    return false;
                }
            }

            for (const auto& existingNodeEntry : std::filesystem::directory_iterator(workspaceDir, ec))
            {
                if (ec)
                {
                    return false;
                }
                if (!existingNodeEntry.is_directory())
                {
                    continue;
                }
                if (!std::filesystem::exists(existingNodeEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceNodeMetadataFileName }, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                if (desiredNodeDirs.find(_toLower(existingNodeEntry.path().filename().wstring())) == desiredNodeDirs.end())
                {
                    std::filesystem::remove_all(existingNodeEntry.path(), ec);
                    if (ec)
                    {
                        return false;
                    }
                }
            }
        }

        for (const auto& existingWorkspaceEntry : std::filesystem::directory_iterator(path, ec))
        {
            if (ec)
            {
                return false;
            }
            if (!existingWorkspaceEntry.is_directory())
            {
                continue;
            }
            if (!std::filesystem::exists(existingWorkspaceEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceMetadataFileName }, ec) || ec)
            {
                ec.clear();
                continue;
            }

            if (desiredWorkspaceDirs.find(_toLower(existingWorkspaceEntry.path().filename().wstring())) == desiredWorkspaceDirs.end())
            {
                std::filesystem::remove_all(existingWorkspaceEntry.path(), ec);
                if (ec)
                {
                    return false;
                }
            }
        }

        std::filesystem::remove(path / std::filesystem::path{ terminal::workspacepaths::LegacyWorkspaceFileName }, ec);
        if (ec)
        {
            return false;
        }

        if (!_writeUtf8TextFile(path / std::filesystem::path{ terminal::workspacepaths::WorkspaceOrderFileName },
                                _serializeWorkspaceOrder(_workspaces)))
        {
            return false;
        }

        if (path == terminal::workspacepaths::ResolveWorkspaceDefinitionsPath())
        {
            return _removeLegacyWorkspaceDirectoriesIfPresent();
        }

        return true;
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

        if (std::filesystem::is_regular_file(path, ec) && !ec)
        {
            return manager;
        }

        if (!std::filesystem::is_directory(path, ec) || ec)
        {
            return manager;
        }

        const auto persistedWorkspaces = _enumeratePersistedWorkspaceDirectories(path);
        if (!persistedWorkspaces.has_value())
        {
            return manager;
        }

        for (const auto& workspace : *persistedWorkspaces)
        {
            const auto statePath = workspace.Directory / std::filesystem::path{ terminal::workspacepaths::WorkspaceStateFileName };
            if (!std::filesystem::exists(statePath, ec) || ec)
            {
                ec.clear();
                continue;
            }

            _loadWorkspaceStateFile(manager, statePath, workspace.Definition.Id);
        }

        return manager;
    }

    bool WorkspaceStateManager::Save() const
    {
        return SaveToPath(DefaultPath());
    }

    bool WorkspaceStateManager::SaveToPath(const std::filesystem::path& path) const
    {
        std::error_code ec;
        if (path.has_extension())
        {
            return false;
        }

        std::filesystem::create_directories(path, ec);
        if (ec)
        {
            return false;
        }

        const auto persistedWorkspaces = _enumeratePersistedWorkspaceDirectories(path);
        if (!persistedWorkspaces.has_value())
        {
            return false;
        }

        std::unordered_map<std::wstring, std::vector<WorkspaceStateWindow>> windowsByWorkspaceId;
        for (const auto& window : _windows)
        {
            if (window.WindowId == 0 || window.WorkspaceId.empty())
            {
                continue;
            }

            windowsByWorkspaceId[window.WorkspaceId].emplace_back(window);
        }

        for (const auto& workspace : *persistedWorkspaces)
        {
            const auto statePath = workspace.Directory / std::filesystem::path{ terminal::workspacepaths::WorkspaceStateFileName };
            if (const auto windows = windowsByWorkspaceId.find(workspace.Definition.Id);
                windows != windowsByWorkspaceId.end() && !windows->second.empty())
            {
                if (!_writeUtf8TextFile(statePath, _serializeWorkspaceStateFile(windows->second)))
                {
                    return false;
                }
            }
            else
            {
                std::filesystem::remove(statePath, ec);
                if (ec)
                {
                    return false;
                }
            }
        }

        for (const auto& existingEntry : std::filesystem::directory_iterator(path, ec))
        {
            if (ec)
            {
                return false;
            }
            if (!existingEntry.is_regular_file())
            {
                continue;
            }

            const auto fileName = existingEntry.path().filename().wstring();
            if (fileName == terminal::workspacepaths::WorkspaceStateFileName ||
                fileName.rfind(std::wstring{ terminal::workspacepaths::LegacyWorkspaceStateFilePrefix }, 0) == 0)
            {
                std::filesystem::remove(existingEntry.path(), ec);
                if (ec)
                {
                    return false;
                }
            }
        }
        if (path == terminal::workspacepaths::ResolveWorkspaceDefinitionsPath())
        {
            return _removeLegacyWorkspaceStateFilesIfPresent();
        }

        return true;
    }
