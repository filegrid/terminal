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

        void _applyNodeField(WorkspaceNode& node, const std::wstring& key, const std::wstring& value);

        void _applyWorkspaceField(Workspace& workspace, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"description")
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
            else if (key.rfind(L"default.", 0) == 0)
            {
                _applyNodeField(workspace.NewNodeDefaults, key.substr(8), value);
            }
        }

        void _applyNodeField(WorkspaceNode& node, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"connectionRef")
            {
                node.ConnectionRef = value;
            }
            else if (key == L"profileGuid")
            {
                node.ProfileGuid = value;
            }
            else if (key == L"tabColor")
            {
                node.TabColor = value;
            }
            else if (key == L"showTab")
            {
                node.ShowTab = _parseBool(value, node.ShowTab);
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

        struct PendingMultilineValue
        {
            uint32_t ContentIndent{};
            std::wstring Key;
            std::wstring Value;
        };

        std::wstring _makeUniqueDirectoryName(const std::wstring& baseName, std::unordered_set<std::wstring>& usedNames)
        {
            std::wstring candidate = baseName;
            auto lowered = _toLower(candidate);
            for (size_t index = 2; !usedNames.emplace(lowered).second; ++index)
            {
                candidate = fmt::format(FMT_COMPILE(L"{} ({})"), baseName, index);
                lowered = _toLower(candidate);
            }
            return candidate;
        }

        bool _writeUtf8TextFile(const std::filesystem::path& path, const std::wstring& content)
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

            const auto utf8 = til::u16u8(content);
            std::ofstream output{ path, std::ios::binary | std::ios::trunc };
            if (!output)
            {
                return false;
            }

            output.write(utf8.data(), gsl::narrow_cast<std::streamsize>(utf8.size()));
            return output.good();
        }

        std::optional<std::wstring> _readUtf8TextFile(const std::filesystem::path& path)
        {
            std::ifstream input{ path, std::ios::binary };
            if (!input)
            {
                return std::nullopt;
            }

            std::string utf8{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
            return til::u8u16(utf8);
        }

        template<typename T, typename ApplyFn>
        std::optional<std::pair<T, size_t>> _loadFlatYamlObject(const std::filesystem::path& path, ApplyFn&& applyField)
        {
            const auto content = _readUtf8TextFile(path);
            if (!content.has_value())
            {
                return std::nullopt;
            }

            T result{};
            std::optional<PendingMultilineValue> pendingMultilineValue;
            size_t order = 0;

            auto applyPendingValue = [&]() {
                if (pendingMultilineValue)
                {
                    applyField(result, pendingMultilineValue->Key, pendingMultilineValue->Value, order);
                    pendingMultilineValue.reset();
                }
            };

            std::wistringstream stream{ *content };
            for (std::wstring line; std::getline(stream, line);)
            {
                const std::wstring_view rawView{ line };
                const auto contentView = _trimRight(rawView);
                const auto trimmed = _trim(contentView);
                const auto indent = rawView.find_first_not_of(L' ');
                const auto safeIndent = indent == std::wstring_view::npos ? gsl::narrow_cast<uint32_t>(rawView.size()) : gsl::narrow_cast<uint32_t>(indent);

                if (pendingMultilineValue)
                {
                    if (safeIndent >= pendingMultilineValue->ContentIndent)
                    {
                        if (!pendingMultilineValue->Value.empty())
                        {
                            pendingMultilineValue->Value.push_back(L'\n');
                        }

                        const auto start = std::min(static_cast<size_t>(pendingMultilineValue->ContentIndent), rawView.size());
                        pendingMultilineValue->Value.append(rawView.substr(start));
                        continue;
                    }

                    applyPendingValue();
                }

                if (trimmed.empty() || trimmed.starts_with(L"#") || safeIndent != 0)
                {
                    continue;
                }

                const auto [key, value] = _parseKeyValue(trimmed);
                if (key.empty())
                {
                    continue;
                }

                if (value == L"|")
                {
                    pendingMultilineValue = PendingMultilineValue{ 2, key, {} };
                    continue;
                }

                applyField(result, key, value, order);
            }

            applyPendingValue();
            return std::pair<T, size_t>{ std::move(result), order };
        }

        std::optional<std::pair<Workspace, size_t>> _loadWorkspaceMetadataFile(const std::filesystem::path& path)
        {
            return _loadFlatYamlObject<Workspace>(path, [](Workspace& workspace, const std::wstring& key, const std::wstring& value, size_t&) {
                if (key != L"version")
                {
                    _applyWorkspaceField(workspace, key, value);
                }
            });
        }

        std::optional<WorkspaceNode> _loadWorkspaceNodeMetadataFile(const std::filesystem::path& path)
        {
            if (const auto metadata = _loadFlatYamlObject<WorkspaceNode>(path, [](WorkspaceNode& node, const std::wstring& key, const std::wstring& value, size_t&) {
                    if (key != L"version")
                    {
                        _applyNodeField(node, key, value);
                    }
                }))
                {
                    return std::move(metadata->first);
                }

            return std::nullopt;
        }

        struct PersistedWorkspaceDirectory
        {
            std::filesystem::path Directory;
            Workspace Definition;
        };

        std::optional<std::vector<PersistedWorkspaceDirectory>> _enumeratePersistedWorkspaceDirectories(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::vector<PersistedWorkspaceDirectory> workspaces;
            for (const auto& workspaceEntry : std::filesystem::directory_iterator(path, ec))
            {
                if (ec)
                {
                    return std::nullopt;
                }
                if (!workspaceEntry.is_directory())
                {
                    continue;
                }

                const auto workspaceFile = workspaceEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceMetadataFileName };
                if (!std::filesystem::exists(workspaceFile, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                const auto metadata = _loadWorkspaceMetadataFile(workspaceFile);
                if (!metadata.has_value())
                {
                    continue;
                }

                auto workspace = std::move(metadata->first);
                workspace.Name = workspaceEntry.path().filename().wstring();
                workspace.Id = workspace.Name;

                workspaces.emplace_back(PersistedWorkspaceDirectory{
                    workspaceEntry.path(),
                    std::move(workspace),
                });
            }

            std::stable_sort(workspaces.begin(), workspaces.end(), [](const auto& lhs, const auto& rhs) {
                return _toLower(lhs.Definition.Name) < _toLower(rhs.Definition.Name);
            });
            return workspaces;
        }

        std::wstring _serializeWorkspaceStateFile(const std::vector<WorkspaceStateWindow>& windows)
        {
            std::wostringstream stream;
            stream << L"version: 1\n";
            stream << L"windows:\n";
            for (const auto& window : windows)
            {
                stream << L"  - windowId: " << _quote(std::to_wstring(window.WindowId)) << L"\n";
                if (!window.WindowName.empty())
                {
                    stream << L"    windowName: " << _quote(window.WindowName) << L"\n";
                }
            }
            return stream.str();
        }

        void _loadWorkspaceStateFile(WorkspaceStateManager& manager, const std::filesystem::path& path, const std::wstring_view workspaceId)
        {
            const auto content = _readUtf8TextFile(path);
            if (!content.has_value())
            {
                return;
            }

            std::wistringstream stream{ *content };

            enum class ParseSection
            {
                None,
                Windows,
            };

            ParseSection section = ParseSection::None;
            std::optional<WorkspaceStateWindow> currentWindow;

            auto appendWindow = [&]() {
                if (!currentWindow)
                {
                    return;
                }

                if (currentWindow->WorkspaceId.empty())
                {
                    currentWindow->WorkspaceId.assign(workspaceId);
                }
                manager.UpsertWindow(std::move(*currentWindow));
                currentWindow.reset();
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
        }

        std::wstring _serializeWorkspaceMetadata(const Workspace& workspace)
        {
            std::wostringstream stream;
            stream << L"version: 1\n";
            if (!workspace.Description.empty())
            {
                stream << L"description: " << _quote(workspace.Description) << L"\n";
            }
            if (!workspace.BackgroundColor.empty())
            {
                stream << L"backgroundColor: " << _quote(workspace.BackgroundColor) << L"\n";
            }
            stream << L"locked: " << (workspace.Locked ? L"true" : L"false") << L"\n";
            const auto& defaults = workspace.NewNodeDefaults;
            if (!defaults.ProfileGuid.empty())
            {
                stream << L"default.profileGuid: " << _quote(defaults.ProfileGuid) << L"\n";
            }
            if (!defaults.StartupDirectory.empty())
            {
                stream << L"default.startupDirectory: " << _quote(defaults.StartupDirectory) << L"\n";
            }
            if (!defaults.StartupAction.empty())
            {
                stream << L"default.startupAction: " << _quote(defaults.StartupAction) << L"\n";
            }
            stream << L"default.showTab: " << (defaults.ShowTab ? L"true" : L"false") << L"\n";
            stream << L"default.showInputPanel: " << (defaults.ShowInputPanel ? L"true" : L"false") << L"\n";
            stream << L"default.useNodeNameAsTabTitle: " << (defaults.UseNodeNameAsTabTitle ? L"true" : L"false") << L"\n";
            return stream.str();
        }

        std::wstring _serializeWorkspaceOrder(const std::vector<Workspace>& workspaces)
        {
            std::wostringstream stream;
            stream << L"version: 1\n";
            if (!workspaces.empty())
            {
                std::wstring serializedOrder;
                for (const auto& workspace : workspaces)
                {
                    const auto& workspaceName = workspace.Name.empty() ? workspace.Id : workspace.Name;
                    if (workspaceName.empty())
                    {
                        continue;
                    }

                    if (!serializedOrder.empty())
                    {
                        serializedOrder.push_back(L'\n');
                    }
                    serializedOrder.append(workspaceName);
                }

                if (!serializedOrder.empty())
                {
                    _writeMultilineValue(stream, L"", L"workspaces", serializedOrder);
                }
            }
            return stream.str();
        }

        std::wstring _serializeWorkspaceNodeMetadata(const WorkspaceNode& node)
        {
            std::wostringstream stream;
            stream << L"version: 1\n";
            if (!node.ConnectionRef.empty())
            {
                stream << L"connectionRef: " << _quote(node.ConnectionRef) << L"\n";
            }
            if (!node.ProfileGuid.empty())
            {
                stream << L"profileGuid: " << _quote(node.ProfileGuid) << L"\n";
            }
            if (!node.TabColor.empty())
            {
                const auto normalizedColor = _normalizeColor(node.TabColor);
                stream << L"tabColor: " << _quote(normalizedColor.empty() ? node.TabColor : normalizedColor) << L"\n";
            }
            stream << L"showTab: " << (node.ShowTab ? L"true" : L"false") << L"\n";
            stream << L"startupDirectory: " << _quote(node.StartupDirectory) << L"\n";
            if (_containsLineBreak(node.StartupAction))
            {
                _writeMultilineValue(stream, L"", L"startupAction", node.StartupAction);
            }
            else
            {
                stream << L"startupAction: " << _quote(node.StartupAction) << L"\n";
            }
            if (!node.OperatingSystem.empty())
            {
                stream << L"operatingSystem: " << _quote(node.OperatingSystem) << L"\n";
            }
            if (!node.ShellType.empty())
            {
                stream << L"shellType: " << _quote(node.ShellType) << L"\n";
            }
            stream << L"showInputPanel: " << (node.ShowInputPanel ? L"true" : L"false") << L"\n";
            stream << L"useNodeNameAsTabTitle: " << (node.UseNodeNameAsTabTitle ? L"true" : L"false") << L"\n";
            return stream.str();
        }
