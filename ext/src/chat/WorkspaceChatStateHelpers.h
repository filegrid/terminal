#pragma once

#include "TerminalInputHarness.h"

#include <optional>
#include <string>
#include <string_view>

namespace terminal::workspacechat
{
    std::wstring WorkspaceStorageKey(std::wstring_view currentWorkspaceId, uint64_t windowId);
    std::wstring WorkspaceDraftKey(std::wstring_view workspaceKey, std::optional<uint32_t> focusedTabIndex);
    std::wstring WorkspaceStateKey(std::wstring_view routingKey, uint64_t contentId);

    bool SyncCapturedWorkingDirectory(TerminalInputState& inputState,
                                      std::wstring& lastReportedWorkingDirectory,
                                      std::wstring_view workingDirectory);

    bool ShouldPreferTwoPhaseSubmit(std::wstring_view currentCommandline,
                                    bool preferTwoPhaseSubmit,
                                    std::wstring_view lastCommand,
                                    std::wstring_view lastSubmittedInput,
                                    std::wstring_view startupAction,
                                    std::wstring_view explicitCommandline);
}
