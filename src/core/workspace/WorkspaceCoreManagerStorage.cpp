    std::filesystem::path WorkspaceManager::DefaultPath()
    {
        return terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
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

            if (!workspace.Nodes.empty())
            {
                std::vector<WorkspaceNode> orderedVisibleNodes;
                for (const auto nodeIndex : _orderedVisibleWorkspaceNodeIndices(workspace))
                {
                    orderedVisibleNodes.emplace_back(workspace.Nodes.at(nodeIndex));
                }
                std::ignore = ApplyVisibleWorkspaceNodeOrder(workspace, orderedVisibleNodes);
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

    const Workspace* WorkspaceManager::FindById(std::wstring_view id) const noexcept
    {
        const auto it = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == id;
        });
        return it == _workspaces.end() ? nullptr : &*it;
    }
