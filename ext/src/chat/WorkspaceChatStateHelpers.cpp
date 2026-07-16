// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceChatStateHelpers.h"

#include "WorkspaceChatTextHelpers.h"

#include <fmt/format.h>

namespace terminal::workspacechat
{
    std::wstring WorkspaceStorageKey(std::wstring_view currentWorkspaceId, const uint64_t windowId)
    {
        if (!currentWorkspaceId.empty())
        {
            return std::wstring{ currentWorkspaceId };
        }

        if (windowId != 0)
        {
            return fmt::format(L"__unsaved-window-{}", windowId);
        }

        return L"__unsaved-window";
    }

    std::wstring WorkspaceDraftKey(std::wstring_view workspaceKey, const std::optional<uint32_t> focusedTabIndex)
    {
        if (focusedTabIndex.has_value())
        {
            return fmt::format(L"{}__tab-{}", workspaceKey, *focusedTabIndex + 1);
        }

        return std::wstring{ workspaceKey } + L"__tab-0";
    }

    std::wstring WorkspaceStateKey(std::wstring_view routingKey, const uint64_t contentId)
    {
        if (!routingKey.empty())
        {
            return std::wstring{ routingKey };
        }

        return fmt::format(L"content-{}", contentId);
    }

    bool SyncCapturedWorkingDirectory(TerminalInputState& inputState,
                                      std::wstring& lastReportedWorkingDirectory,
                                      std::wstring_view workingDirectory)
    {
        const auto trimmedWorkingDirectory = TrimWorkspaceChatText(workingDirectory);
        if (trimmedWorkingDirectory.empty())
        {
            return false;
        }

        const auto inferredWorkingDirectory = TrimWorkspaceChatText(inputState.LastWorkingDirectory);
        if (inferredWorkingDirectory.empty())
        {
            TrackTerminalInput(inputState, {}, trimmedWorkingDirectory);
            lastReportedWorkingDirectory = trimmedWorkingDirectory;
            return true;
        }

        if (inferredWorkingDirectory == trimmedWorkingDirectory)
        {
            lastReportedWorkingDirectory = trimmedWorkingDirectory;
            return false;
        }

        if (lastReportedWorkingDirectory.empty() || inferredWorkingDirectory == lastReportedWorkingDirectory)
        {
            TrackTerminalInput(inputState, {}, trimmedWorkingDirectory);
            lastReportedWorkingDirectory = trimmedWorkingDirectory;
            return true;
        }

        return false;
    }

    bool ShouldPreferTwoPhaseSubmit(std::wstring_view currentCommandline,
                                    const bool preferTwoPhaseSubmit,
                                    std::wstring_view lastCommand,
                                    std::wstring_view lastSubmittedInput,
                                    std::wstring_view startupAction,
                                    std::wstring_view explicitCommandline)
    {
        return IsInteractiveCliCommand(currentCommandline) ||
               preferTwoPhaseSubmit ||
               IsInteractiveCliCommand(lastCommand) ||
               IsInteractiveCliCommand(lastSubmittedInput) ||
               IsInteractiveCliCommand(startupAction) ||
               IsInteractiveCliCommand(explicitCommandline);
    }
}
