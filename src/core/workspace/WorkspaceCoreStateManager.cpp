    std::filesystem::path WorkspaceStateManager::DefaultPath()
    {
        return terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
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

    WorkspaceStateManager WorkspaceStateManager::LoadRuntime()
    {
        auto manager = _withRuntimeWorkspaceStateBlock([](RuntimeWorkspaceStateBlock& block, uint64_t) {
            return _loadRuntimeWorkspaceStateManagerFromBlock(block);
        });

        return manager;
    }

    uint64_t WorkspaceStateManager::RuntimeHeartbeatIntervalMs() noexcept
    {
        return WorkspaceWindowStateHeartbeatIntervalMs;
    }

    bool WorkspaceStateManager::RemoveRuntimeWindowState(const uint64_t windowId)
    {
        if (windowId == 0)
        {
            return false;
        }

        const auto removed = _withRuntimeWorkspaceStateBlock([&](RuntimeWorkspaceStateBlock& block, const uint64_t now) {
            auto manager = _loadRuntimeWorkspaceStateManagerFromBlock(block);
            const auto recordCountBefore = manager.Windows().size();
            manager.RemoveWindow(windowId);
            const auto changed = manager.Windows().size() != recordCountBefore;
            _saveRuntimeWorkspaceStateManagerToBlock(manager, block, now);
            return changed;
        });
        return removed;
    }

    bool WorkspaceStateManager::RefreshRuntimeWindowState(const uint64_t windowId,
                                                          const std::wstring_view windowName,
                                                          const std::wstring_view workspaceId)
    {
        if (windowId == 0)
        {
            return false;
        }

        const auto refreshed = _withRuntimeWorkspaceStateBlock([&](RuntimeWorkspaceStateBlock& block, const uint64_t now) {
            auto manager = _loadRuntimeWorkspaceStateManagerFromBlock(block);
            manager.UpdateWindowState(windowId, windowName, workspaceId);
            _saveRuntimeWorkspaceStateManagerToBlock(manager, block, now, windowId);
            return true;
        });
        return refreshed;
    }

    bool WorkspaceStateManager::Save() const
    {
        return SaveToPath(DefaultPath());
    }

    bool WorkspaceStateManager::SaveToPath(const std::filesystem::path& path) const
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

        const auto persistedWorkspaces = _enumeratePersistedWorkspaceDirectoriesIfPresent(path);
        if (!persistedWorkspaces.has_value())
        {
            return false;
        }

        std::unordered_set<std::wstring> touchedWorkspaces;
        for (const auto& window : _windows)
        {
            if (window.WorkspaceId.empty())
            {
                continue;
            }

            touchedWorkspaces.emplace(window.WorkspaceId);
        }

        for (const auto& workspace : *persistedWorkspaces)
        {
            std::vector<WorkspaceStateWindow> windows;
            for (const auto& window : _windows)
            {
                if (window.WorkspaceId == workspace.Definition.Id)
                {
                    windows.emplace_back(window);
                }
            }

            const auto statePath = workspace.Directory / std::filesystem::path{ terminal::workspacepaths::WorkspaceStateFileName };
            if (windows.empty())
            {
                std::filesystem::remove(statePath, ec);
                if (ec)
                {
                    return false;
                }
                continue;
            }

            if (!_writeUtf8TextFile(statePath, _serializeWorkspaceStateFile(windows)))
            {
                return false;
            }
        }

        for (const auto& workspaceId : touchedWorkspaces)
        {
            const auto existingIt = std::find_if(persistedWorkspaces->begin(), persistedWorkspaces->end(), [&](const auto& workspace) {
                return workspace.Definition.Id == workspaceId;
            });
            if (existingIt != persistedWorkspaces->end())
            {
                continue;
            }

            const auto workspaceDir = path / SanitizeWorkspaceDirectoryName(workspaceId, L"workspace");
            const auto statePath = workspaceDir / std::filesystem::path{ terminal::workspacepaths::WorkspaceStateFileName };

            std::vector<WorkspaceStateWindow> windows;
            for (const auto& window : _windows)
            {
                if (window.WorkspaceId == workspaceId)
                {
                    windows.emplace_back(window);
                }
            }

            if (windows.empty())
            {
                continue;
            }

            std::filesystem::create_directories(workspaceDir, ec);
            if (ec)
            {
                return false;
            }

            if (!_writeUtf8TextFile(statePath, _serializeWorkspaceStateFile(windows)))
            {
                return false;
            }
        }

        if (path == terminal::workspacepaths::ResolveWorkspaceDefinitionsPath())
        {
            return _removeLegacyWorkspaceStateFilesIfPresent();
        }

        return true;
    }

    const std::vector<WorkspaceStateWindow>& WorkspaceStateManager::Windows() const noexcept
    {
        return _windows;
    }

    void WorkspaceStateManager::SetWindows(std::vector<WorkspaceStateWindow> windows)
    {
        _windows = std::move(windows);
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

    void WorkspaceStateManager::RemoveWindow(uint64_t windowId) noexcept
    {
        _windows.erase(std::remove_if(_windows.begin(), _windows.end(), [&](const auto& window) {
            return window.WindowId == windowId;
        }), _windows.end());
    }

    void WorkspaceStateManager::UpdateWindowState(const uint64_t windowId, const std::wstring_view windowName, const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            RemoveWindow(windowId);
            return;
        }

        WorkspaceStateWindow window;
        window.WindowId = windowId;
        window.ProcessId = GetCurrentProcessId();
        window.ProcessName = _queryProcessImageName(window.ProcessId);
        window.WindowName = std::wstring{ windowName };
        window.WorkspaceId = std::wstring{ workspaceId };
        UpsertWindow(std::move(window));
    }

    bool WorkspaceStateManager::HasOpenWorkspace(const std::wstring_view workspaceId) const noexcept
    {
        if (workspaceId.empty())
        {
            return false;
        }

        return std::any_of(_windows.begin(), _windows.end(), [&](const auto& window) {
            return window.WorkspaceId == workspaceId && _isWorkspaceWindowProcessAlive(window);
        });
    }

    std::optional<uint64_t> WorkspaceStateManager::FindOpenWorkspaceWindowId(const std::wstring_view workspaceId) const noexcept
    {
        if (workspaceId.empty())
        {
            return std::nullopt;
        }

        const auto found = std::find_if(_windows.begin(), _windows.end(), [&](const auto& window) {
            return window.WorkspaceId == workspaceId &&
                   _isWorkspaceWindowProcessAlive(window);
        });
        if (found == _windows.end())
        {
            return std::nullopt;
        }

        return found->WindowId;
    }
