    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromProfile(const std::wstring_view source)
    {
        WorkspaceRuntimeMetadata metadata;
        const auto loweredSource = _toLower(source);
        if (loweredSource == L"windows.terminal.ssh")
        {
            metadata.ShellType = L"ssh";
            return metadata;
        }

        if (loweredSource == L"windows.terminal.powershellcore")
        {
            metadata.ShellType = L"powershell";
            metadata.OperatingSystem = L"windows";
            return metadata;
        }

        if (_isWslProfileSource(loweredSource))
        {
            metadata.OperatingSystem = L"linux";
            metadata.ShellType = L"wsl";
            return metadata;
        }

        return metadata;
    }

    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromCommandline(const std::wstring_view value)
    {
        WorkspaceRuntimeMetadata metadata;
        if (value.empty())
        {
            return metadata;
        }

        const auto lowered = _toLower(value);
        if (lowered.find(L"ssh.exe") != std::wstring::npos ||
            lowered == L"ssh" ||
            lowered.starts_with(L"ssh "))
        {
            metadata.ShellType = L"ssh";
            return metadata;
        }

        if (lowered.find(L"powershell.exe") != std::wstring::npos ||
            lowered.find(L"pwsh.exe") != std::wstring::npos ||
            lowered == L"powershell" ||
            lowered == L"pwsh" ||
            lowered.starts_with(L"powershell ") ||
            lowered.starts_with(L"pwsh "))
        {
            metadata.ShellType = L"powershell";
            metadata.OperatingSystem = L"windows";
            return metadata;
        }

        if (lowered.find(L"cmd.exe") != std::wstring::npos ||
            lowered == L"cmd" ||
            lowered.starts_with(L"cmd "))
        {
            metadata.ShellType = L"cmd";
            metadata.OperatingSystem = L"windows";
            return metadata;
        }

        if (_isWslCommandline(lowered))
        {
            metadata.OperatingSystem = L"linux";
            metadata.ShellType = L"wsl";
            return metadata;
        }

        if (lowered.find(L"/bin/bash") != std::wstring::npos ||
            lowered.find(L"/bin/sh") != std::wstring::npos ||
            lowered.find(L"/bin/zsh") != std::wstring::npos ||
            lowered.find(L"/bin/fish") != std::wstring::npos)
        {
            metadata.OperatingSystem = L"linux";
        }

        return metadata;
    }

    bool IsWorkspaceSshCommandline(const std::wstring_view value)
    {
        const auto lowered = _toLower(value);
        return lowered.find(L"ssh.exe") != std::wstring::npos ||
               lowered == L"ssh" ||
               lowered.starts_with(L"ssh ");
    }

    bool HasWorkspaceSshTtyOption(const std::wstring_view commandline)
    {
        if (commandline.empty())
        {
            return false;
        }

        const auto argv = _splitCommandlineArguments(commandline);
        for (size_t index = 1; index < argv.size(); ++index)
        {
            const auto arg = _toLower(_trim(argv[index]));
            if (arg == L"-t" || arg == L"-tt")
            {
                return true;
            }
        }

        return false;
    }

    bool IsWorkspaceSshTransport(const std::wstring_view profileSource,
                                 const std::wstring_view profileCommandline,
                                 const std::wstring_view commandline)
    {
        if (IsWorkspaceSshCommandline(commandline))
        {
            return true;
        }

        const auto loweredSource = _toLower(profileSource);
        return loweredSource == L"windows.terminal.ssh" || IsWorkspaceSshCommandline(profileCommandline);
    }

    WorkspaceRuntimeLaunchState PrepareWorkspaceRuntimeLaunchState(const std::wstring_view /*startingDirectory*/,
                                                                   const std::wstring_view profileSource,
                                                                   const std::wstring_view profileCommandline,
                                                                   const std::wstring_view commandline)
    {
        WorkspaceRuntimeLaunchState state;
        state.IsSshTransport = IsWorkspaceSshTransport(profileSource, profileCommandline, commandline);
        state.HasSshTtyOption = HasWorkspaceSshTtyOption(commandline);
        state.StartingDirectory.clear();
        if (!commandline.empty() && commandline != profileCommandline)
        {
            state.ExplicitCommandline = std::wstring{ commandline };
        }

        auto metadata = InferWorkspaceRuntimeMetadataFromCommandline(commandline.empty() ? profileCommandline : commandline);
        if (metadata.OperatingSystem.empty() || metadata.ShellType.empty())
        {
            const auto profileMetadata = InferWorkspaceRuntimeMetadataFromProfile(profileSource);
            if (metadata.OperatingSystem.empty())
            {
                metadata.OperatingSystem = profileMetadata.OperatingSystem;
            }
            if (metadata.ShellType.empty())
            {
                metadata.ShellType = profileMetadata.ShellType;
            }
        }

        state.OperatingSystem = metadata.OperatingSystem.empty() ? L"linux" : metadata.OperatingSystem;
        state.ShellType = std::move(metadata.ShellType);
        return state;
    }

    WorkspaceNodeLaunchResolution ResolveWorkspaceNodeLaunchResolution(const WorkspaceNodeLaunchResolutionInput& input)
    {
        WorkspaceNodeLaunchResolution resolution;

        if (!input.ObservedStartupAction.empty())
        {
            resolution.StartupAction = input.ObservedStartupAction;
        }
        else if (!input.RuntimeStartupAction.empty())
        {
            resolution.StartupAction = input.RuntimeStartupAction;
        }
        else if (!input.RuntimeExplicitCommandline.empty())
        {
            resolution.StartupAction = input.RuntimeExplicitCommandline;
        }
        else if (input.PersistedNode.has_value())
        {
            resolution.StartupAction = input.PersistedNode->StartupAction;
        }
        else if (!input.TerminalCommandline.empty() && input.TerminalCommandline != input.ProfileCommandline)
        {
            resolution.StartupAction = input.TerminalCommandline;
        }

        if (!input.ObservedWorkingDirectory.empty())
        {
            resolution.StartingDirectory = input.ObservedWorkingDirectory;
        }
        else if (!input.RuntimeStartingDirectory.empty())
        {
            resolution.StartingDirectory = input.RuntimeStartingDirectory;
        }
        else if (input.PersistedNode.has_value())
        {
            resolution.StartingDirectory = input.PersistedNode->StartupDirectory;
        }
        const auto profileMetadata = InferWorkspaceRuntimeMetadataFromProfile(input.ProfileSource);
        if (profileMetadata.ShellType == L"wsl")
        {
            resolution.OperatingSystem = L"linux";
            resolution.ShellType = L"wsl";
            return resolution;
        }

        if (!input.ObservedOperatingSystem.empty())
        {
            resolution.OperatingSystem = input.ObservedOperatingSystem;
        }
        else if (!input.RuntimeOperatingSystem.empty())
        {
            resolution.OperatingSystem = input.RuntimeOperatingSystem;
        }
        else
        {
            auto metadata = InferWorkspaceRuntimeMetadataFromCommandline(input.TerminalCommandline.empty() ? input.ProfileCommandline : input.TerminalCommandline);
            if (metadata.OperatingSystem.empty())
            {
                metadata = profileMetadata;
            }
            if (!metadata.OperatingSystem.empty())
            {
                resolution.OperatingSystem = metadata.OperatingSystem;
            }
            else if (input.PersistedNode.has_value())
            {
                resolution.OperatingSystem = input.PersistedNode->OperatingSystem;
            }
            else
            {
                resolution.OperatingSystem = L"linux";
            }
        }

        if (!input.ObservedShellType.empty())
        {
            resolution.ShellType = input.ObservedShellType;
        }
        else if (!input.RuntimeShellType.empty())
        {
            resolution.ShellType = input.RuntimeShellType;
        }
        else
        {
            auto metadata = InferWorkspaceRuntimeMetadataFromCommandline(input.TerminalCommandline.empty() ? input.ProfileCommandline : input.TerminalCommandline);
            if (metadata.ShellType.empty())
            {
                metadata = profileMetadata;
            }
            if (!metadata.ShellType.empty())
            {
                resolution.ShellType = metadata.ShellType;
            }
            else if (input.PersistedNode.has_value())
            {
                resolution.ShellType = input.PersistedNode->ShellType;
            }
        }

        return resolution;
    }

    WorkspaceNodeLaunchResolution ResolveWorkspaceNodeLaunchResolution(const WorkspaceNodeLaunchResolutionPlanInput& input)
    {
        return ResolveWorkspaceNodeLaunchResolution(WorkspaceNodeLaunchResolutionInput{
            .PersistedNode = input.PersistedNode,
            .ObservedStartupAction = input.ObservedStartupAction,
            .ObservedWorkingDirectory = input.ObservedWorkingDirectory.empty() ? input.TrackedWorkingDirectory :
                                                                                 input.ObservedWorkingDirectory,
            .ObservedOperatingSystem = input.ObservedOperatingSystem,
            .ObservedShellType = input.ObservedShellType,
            .RuntimeStartupAction = input.RuntimeStartupAction,
            .RuntimeExplicitCommandline = input.RuntimeExplicitCommandline,
            .RuntimeStartingDirectory = input.RuntimeStartingDirectory,
            .RuntimeOperatingSystem = input.RuntimeOperatingSystem,
            .RuntimeShellType = input.RuntimeShellType,
            .ProfileSource = input.ProfileSource,
            .ProfileCommandline = input.ProfileCommandline,
            .TerminalCommandline = input.TerminalCommandline,
            .TerminalStartingDirectory = input.TerminalStartingDirectory,
        });
    }

    std::wstring ResolveTrackedWorkspaceDirectory(const WorkspaceTrackedDirectoryInput& input)
    {
        const auto reportedWorkingDirectory = std::wstring{ _trim(input.ReportedWorkingDirectory) };
        if (input.IsSshTransport ||
            _toLower(input.RuntimeShellType) == L"ssh" ||
            _toLower(input.RuntimeOperatingSystem) == L"linux")
        {
            return reportedWorkingDirectory;
        }

        const auto processWorkingDirectory = std::wstring{ _trim(input.ProcessWorkingDirectory) };
        if (!processWorkingDirectory.empty())
        {
            return processWorkingDirectory;
        }

        if (!reportedWorkingDirectory.empty())
        {
            return reportedWorkingDirectory;
        }

        return std::wstring{ _trim(input.RuntimeStartingDirectory) };
    }
