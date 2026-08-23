        std::wstring _resolveNodeProfileKey(const WorkspaceNode& node,
                                            const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
                                            const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
        {
            if (profile)
            {
                return Utils::GuidToString(profile.Guid());
            }

            if (!node.ProfileGuid.empty())
            {
                return node.ProfileGuid;
            }

            return Utils::GuidToString(settings.GlobalSettings().DefaultProfile());
        }

        winrt::Windows::UI::Color _fallbackProfileColor(std::wstring_view profileKey)
        {
            const auto& palette = WorkspaceColorPalette();
            const auto index = gsl::narrow_cast<size_t>(_stableHash(profileKey) % palette.size());
            return _parseColor(palette[index]).value();
        }

        winrt::Windows::UI::Color _resolveBaseNodeColor(const WorkspaceNode& node,
                                                        const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
                                                        const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
        {
            if (profile && profile.TabColor())
            {
                return static_cast<winrt::Windows::UI::Color>(til::color{ profile.TabColor().Value() });
            }

            return _fallbackProfileColor(_resolveNodeProfileKey(node, profile, settings));
        }

        winrt::Windows::UI::Color _resolveDuplicateNodeColor(const winrt::Windows::UI::Color& baseColor, const size_t occurrenceIndex) noexcept
        {
            if (occurrenceIndex == 0)
            {
                return baseColor;
            }

            const auto baseRef = _toColorRef(baseColor);
            const auto baseLightness = ColorFix::GetLightness(baseRef);
            auto direction = baseLightness >= 0.5f ? -1.0f : 1.0f;
            if ((occurrenceIndex % 2) == 0)
            {
                direction *= -1.0f;
            }

            const auto magnitude = std::min(0.42f, 0.12f * static_cast<float>((occurrenceIndex + 1) / 2));
            const auto adjusted = ColorFix::AdjustLightness(baseRef, direction * magnitude);
            return _fromColorRef(adjusted, baseColor.A);
        }

        winrt::Microsoft::Terminal::Settings::Model::Profile _resolveNodeProfile(const WorkspaceNode& node,
                                                                                  const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
        {
            if (!node.ProfileGuid.empty() && node.ProfileGuid.size() == 38 && node.ProfileGuid.front() == L'{')
            {
                if (const auto profile = settings.FindProfile(Utils::GuidFromString(node.ProfileGuid.c_str())))
                {
                    return profile;
                }
            }

            if (!node.ProfileName.empty())
            {
                const auto expectedName = _toLower(node.ProfileName);
                const auto profiles = settings.AllProfiles();
                for (uint32_t i = 0; i < profiles.Size(); ++i)
                {
                    const auto profile = profiles.GetAt(i);
                    if (_toLower(profile.Name().c_str()) == expectedName)
                    {
                        return profile;
                    }
                }
            }

            return nullptr;
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

        bool _isPowerShellShellType(std::wstring_view value)
        {
            const auto lowered = _toLower(value);
            return lowered == L"powershell" || lowered == L"pwsh";
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

        std::wstring _quotePosixShellPath(std::wstring_view value)
        {
            std::wstring quoted{ L"\"" };
            size_t offset = 0;
            if (value == L"~" || value.starts_with(L"~/"))
            {
                quoted.append(L"$HOME");
                offset = 1;
            }
            for (const auto ch : value.substr(offset))
            {
                if (ch == L'\\' || ch == L'\"' || ch == L'$' || ch == L'`')
                {
                    quoted.push_back(L'\\');
                }
                quoted.push_back(ch);
            }
            quoted.push_back(L'\"');
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
                if (_isPowerShellShellType(node.ShellType))
                {
                    std::wstring input{ L"Set-Location -LiteralPath " };
                    input.append(_quotePowerShellSingle(startupDirectory));
                    input.push_back(L'\r');
                    return input;
                }
                return _buildWindowsSshStartupDirectoryInput(startupDirectory);
            }

            std::wstring input{ L"cd -- " };
            input.append(_quotePosixShellPath(startupDirectory));
            input.push_back(L'\r');
            return input;
        }

        std::wstring _buildWslStartupInput(const WorkspaceNode& node)
        {
            const auto startupDirectory = _trim(node.StartupDirectory);
            if (startupDirectory.empty())
            {
                return {};
            }

            std::wstring input{ L"cd -- " };
            input.append(_quotePosixShellPath(startupDirectory));
            input.push_back(L'\r');
            return input;
        }
