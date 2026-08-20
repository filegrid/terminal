    std::wstring _trimWorkspaceNodeValue(std::wstring_view value)
    {
        const auto first = value.find_first_not_of(L" \t\r\n");
        if (first == std::wstring_view::npos)
        {
            return {};
        }

        const auto last = value.find_last_not_of(L" \t\r\n");
        return std::wstring{ value.substr(first, last - first + 1) };
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

    bool _containsInsensitive(std::wstring_view haystack, std::wstring_view needle)
    {
        if (needle.empty())
        {
            return true;
        }

        const auto haystackLower = _toLower(haystack);
        const auto needleLower = _toLower(needle);
        return haystackLower.find(needleLower) != std::wstring::npos;
    }

    bool _isLineBreakCharacter(const wchar_t ch) noexcept
    {
        return ch == L'\r' || ch == L'\n';
    }

    std::optional<size_t> _lastNonLineBreakCharacterIndex(std::wstring_view value) noexcept
    {
        for (auto index = value.size(); index > 0; --index)
        {
            if (!_isLineBreakCharacter(value[index - 1]))
            {
                return index - 1;
            }
        }

        return std::nullopt;
    }

    std::vector<std::wstring> _splitDeferredStartupInput(std::wstring_view value)
    {
        std::vector<std::wstring> commands;
        size_t start = 0;

        while (start < value.size())
        {
            auto end = start;
            while (end < value.size() && !_isLineBreakCharacter(value[end]))
            {
                ++end;
            }

            const auto line = value.substr(start, end - start);
            if (!_trimWorkspaceNodeValue(line).empty())
            {
                commands.emplace_back(line);
            }

            start = end;
            while (start < value.size() && _isLineBreakCharacter(value[start]))
            {
                ++start;
            }
        }

        return commands;
    }

    std::wstring _inferOperatingSystemFromPath(std::wstring_view value)
    {
        if (value.empty())
        {
            return {};
        }

        if ((value.size() >= 2 && std::iswalpha(value[0]) != 0 && value[1] == L':') ||
            value.starts_with(L"\\\\") ||
            value.find(L'\\') != std::wstring_view::npos)
        {
            return L"windows";
        }

        if (value.starts_with(L"/") || value.starts_with(L"~"))
        {
            return L"linux";
        }

        return {};
    }

    bool _shouldWaitForSshTtyStartupReplay(std::wstring_view operatingSystem, const bool hasSshTtyOption)
    {
        return hasSshTtyOption && _toLower(operatingSystem) == L"windows";
    }

    bool _isSshTtyStartupReplayReady(const TermControl& control, std::wstring_view operatingSystem)
    {
        const auto workingDirectory = _trimWorkspaceNodeValue(control.WorkingDirectory().c_str());
        if (workingDirectory.empty())
        {
            return false;
        }

        const auto expectedOperatingSystem = _toLower(operatingSystem);
        if (expectedOperatingSystem.empty())
        {
            return true;
        }

        return _inferOperatingSystemFromPath(workingDirectory) == expectedOperatingSystem;
    }

    bool _isPowerShellShellType(std::wstring_view shellType)
    {
        const auto lowered = _toLower(shellType);
        return lowered == L"powershell" || lowered == L"pwsh";
    }

    std::wstring _quotePowerShellSingle(std::wstring_view value)
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

    std::wstring _buildWindowsSshStartupDirectoryInput(std::wstring_view directory)
    {
        auto normalized = _trimWorkspaceNodeValue(directory);
        std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
        if (normalized.empty())
        {
            return {};
        }

        if (normalized.size() >= 2 && std::iswalpha(normalized[0]) != 0 && normalized[1] == L':')
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

    std::vector<std::wstring> _buildSshDeferredStartupInputs(const WorkspaceNode& node)
    {
        std::vector<std::wstring> inputs;

        const auto startupDirectory = _trimWorkspaceNodeValue(node.StartupDirectory);
        const auto startupAction = _trimWorkspaceNodeValue(node.StartupAction);
        const auto operatingSystem = _toLower(node.OperatingSystem);
        if (!startupDirectory.empty())
        {
            if (operatingSystem == L"windows" || _inferOperatingSystemFromPath(startupDirectory) == L"windows")
            {
                if (_isPowerShellShellType(node.ShellType))
                {
                    std::wstring input{ L"Set-Location -LiteralPath " };
                    input.append(_quotePowerShellSingle(startupDirectory));
                    input.push_back(L'\r');
                    inputs.emplace_back(std::move(input));
                }
                else
                {
                    inputs.emplace_back(_buildWindowsSshStartupDirectoryInput(startupDirectory));
                }
            }
            else
            {
                std::wstring input{ L"cd \"" };
                input.append(startupDirectory);
                input.append(L"\"\r");
                inputs.emplace_back(std::move(input));
            }
        }

        if (!startupAction.empty())
        {
            inputs.emplace_back(startupAction);
        }

        return inputs;
    }

    std::wstring _appendDeferredStartupSubmit(std::wstring payload)
    {
        if (payload.empty())
        {
            return payload;
        }

        const auto last = payload.back();
        if (last != L'\r' && last != L'\n')
        {
            payload.push_back(L'\r');
        }
        return payload;
    }

    std::wstring _currentDirectoryFromProcessHandle(HANDLE process)
    {
        if (!process)
        {
            return {};
        }

        struct CURDIR_EX
        {
            UNICODE_STRING DosPath;
            HANDLE Handle;
        };

        struct PROCESS_BASIC_INFORMATION
        {
            NTSTATUS ExitStatus;
            PPEB PebBaseAddress;
            ULONG_PTR AffinityMask;
            KPRIORITY BasePriority;
            ULONG_PTR UniqueProcessId;
            ULONG_PTR InheritedFromUniqueProcessId;
        } info{};

        struct RTL_USER_PROCESS_PARAMETERS_EX
        {
            ULONG MaximumLength;
            ULONG Length;
            ULONG Flags;
            ULONG DebugFlags;
            HANDLE ConsoleHandle;
            ULONG ConsoleFlags;
            HANDLE StandardInput;
            HANDLE StandardOutput;
            HANDLE StandardError;
            CURDIR_EX CurrentDirectory;
            UNICODE_STRING DllPath;
            UNICODE_STRING ImagePathName;
            UNICODE_STRING CommandLine;
        };

        if (!NT_SUCCESS(NtQueryInformationProcess(process, ProcessBasicInformation, &info, sizeof(info), nullptr)))
        {
            return {};
        }

        PEB peb{};
        if (!ReadProcessMemory(process, info.PebBaseAddress, &peb, sizeof(peb), nullptr))
        {
            return {};
        }

        RTL_USER_PROCESS_PARAMETERS_EX params{};
        if (!ReadProcessMemory(process, peb.ProcessParameters, &params, sizeof(params), nullptr))
        {
            return {};
        }

        const auto& dir = params.CurrentDirectory.DosPath;
        if (!dir.Buffer || dir.Length == 0)
        {
            return {};
        }

        std::wstring result(dir.Length / 2u, L'\0');
        if (!ReadProcessMemory(process, dir.Buffer, result.data(), dir.Length, nullptr))
        {
            return {};
        }

        return _trimWorkspaceNodeValue(result);
    }

    std::optional<winrt::guid> _tryParseGuid(std::wstring_view value)
    {
        GUID guid{};
        if (SUCCEEDED(IIDFromString(std::wstring{ value }.c_str(), &guid)))
        {
            return guid;
        }

        return std::nullopt;
    }

    bool _isHexDigit(const wchar_t ch) noexcept
    {
        return (ch >= L'0' && ch <= L'9') ||
               (ch >= L'a' && ch <= L'f') ||
               (ch >= L'A' && ch <= L'F');
    }

    std::wstring _normalizeWorkspaceColor(std::wstring_view color)
    {
        if (color.size() != 7 || color[0] != L'#')
        {
            return {};
        }

        std::wstring normalized;
        normalized.reserve(color.size());
        normalized.push_back(L'#');

        for (size_t i = 1; i < color.size(); ++i)
        {
            if (!_isHexDigit(color[i]))
            {
                return {};
            }
            normalized.push_back(static_cast<wchar_t>(std::towupper(color[i])));
        }

        return normalized;
    }

    std::optional<winrt::Windows::UI::Color> _parseWorkspaceColor(std::wstring_view color)
    {
        const auto normalized = _normalizeWorkspaceColor(color);
        if (normalized.empty())
        {
            return std::nullopt;
        }

        return static_cast<winrt::Windows::UI::Color>(::Microsoft::Console::Utils::ColorFromHexString(til::u16u8(normalized)));
    }

    std::wstring _workspaceColorToString(const winrt::Windows::UI::Color& color)
    {
        return til::u8u16(::Microsoft::Console::Utils::ColorToHexString(til::color{ color }));
    }

    std::wstring _pickUnusedWorkspaceColor(const std::vector<Workspace>& workspaces)
    {
        std::unordered_set<std::wstring> usedColors;
        usedColors.reserve(workspaces.size());
        for (const auto& workspace : workspaces)
        {
            if (const auto normalized = _normalizeWorkspaceColor(workspace.BackgroundColor); !normalized.empty())
            {
                usedColors.emplace(std::move(normalized));
            }
        }

        return PickWorkspacePaletteColor(usedColors, workspaces.size());
    }

    winrt::Windows::UI::Color _workspaceForegroundColor(const winrt::Windows::UI::Color& color)
    {
        constexpr auto lightnessThreshold = 0.6f;
        return ColorFix::GetLightness(til::color{ color }) >= lightnessThreshold ? Colors::Black() : Colors::White();
    }

    std::wstring _workspaceNodeColorDisplayValue(const Workspace& workspace,
                                                 const size_t nodeIndex,
                                                 const CascadiaSettings& settings)
    {
        if (const auto color = ResolveWorkspaceNodeTabColor(workspace, nodeIndex, settings))
        {
            return _workspaceColorToString(*color);
        }

        return {};
    }

    bool _workspaceNodeLoadsTab(const WorkspaceNode& node) noexcept
    {
        return node.ShowTab;
    }

    std::optional<size_t> _findWorkspaceNodeIndexById(const Workspace& workspace, const std::wstring_view nodeId)
    {
        if (nodeId.empty())
        {
            return std::nullopt;
        }

        for (size_t i = 0; i < workspace.Nodes.size(); ++i)
        {
            if (workspace.Nodes.at(i).Id == nodeId)
            {
                return i;
            }
        }

        return std::nullopt;
    }

    std::optional<size_t> _findWorkspaceVisibleNodeIndex(const Workspace& workspace, const size_t visibleOrdinal)
    {
        size_t currentVisibleOrdinal = 0;
        for (size_t i = 0; i < workspace.Nodes.size(); ++i)
        {
            if (!_workspaceNodeLoadsTab(workspace.Nodes.at(i)))
            {
                continue;
            }

            if (currentVisibleOrdinal == visibleOrdinal)
            {
                return i;
            }

            ++currentVisibleOrdinal;
        }

        return std::nullopt;
    }

    std::wstring _makeUniquePersistedName(const std::wstring& baseName, std::unordered_set<std::wstring>& usedNames)
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

    bool _workspaceNodeEquivalent(const WorkspaceNode& lhs, const WorkspaceNode& rhs)
    {
        const auto lhsTabColor = _normalizeWorkspaceColor(lhs.TabColor).empty() ? lhs.TabColor : _normalizeWorkspaceColor(lhs.TabColor);
        const auto rhsTabColor = _normalizeWorkspaceColor(rhs.TabColor).empty() ? rhs.TabColor : _normalizeWorkspaceColor(rhs.TabColor);
        return lhs.Name == rhs.Name &&
               lhs.ProfileGuid == rhs.ProfileGuid &&
               lhsTabColor == rhsTabColor &&
               lhs.ShowTab == rhs.ShowTab &&
               lhs.StartupDirectory == rhs.StartupDirectory &&
               lhs.StartupAction == rhs.StartupAction &&
               lhs.OperatingSystem == rhs.OperatingSystem &&
               lhs.ShellType == rhs.ShellType &&
               lhs.ShowInputPanel == rhs.ShowInputPanel &&
               lhs.UseNodeNameAsTabTitle == rhs.UseNodeNameAsTabTitle &&
               (lhs.ConnectionRef.empty() || rhs.ConnectionRef.empty() || lhs.ConnectionRef == rhs.ConnectionRef);
    }

    bool _workspaceLayoutEquivalent(const Workspace& lhs, const Workspace& rhs)
    {
        if (lhs.Nodes.size() != rhs.Nodes.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs.Nodes.size(); ++i)
        {
            if (!_workspaceNodeEquivalent(lhs.Nodes[i], rhs.Nodes[i]))
            {
                return false;
            }
        }

        return true;
    }
