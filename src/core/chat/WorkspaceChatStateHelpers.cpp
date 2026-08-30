// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "WorkspaceChatStateHelpers.h"

#include "WorkspaceChatTextHelpers.h"

namespace terminal::workspacechat
{
    namespace
    {
        constexpr uint32_t _vkBack{ 0x08 };
        constexpr uint32_t _vkReturn{ 0x0D };
    }

    std::wstring WorkspaceStorageKey(std::wstring_view currentWorkspaceId, const uint64_t windowId)
    {
        if (!currentWorkspaceId.empty())
        {
            return std::wstring{ currentWorkspaceId };
        }

        if (windowId != 0)
        {
            return std::wstring{ L"__unsaved-window-" } + std::to_wstring(windowId);
        }

        return L"__unsaved-window";
    }

    std::wstring WorkspaceDraftKey(std::wstring_view workspaceKey, const std::optional<uint32_t> focusedTabIndex)
    {
        if (focusedTabIndex.has_value())
        {
            return std::wstring{ workspaceKey } + L"__tab-" + std::to_wstring(*focusedTabIndex + 1);
        }

        return std::wstring{ workspaceKey } + L"__tab-0";
    }

    std::wstring WorkspaceStateKey(std::wstring_view routingKey, const uint64_t contentId)
    {
        if (!routingKey.empty())
        {
            return std::wstring{ routingKey };
        }

        return std::wstring{ L"content-" } + std::to_wstring(contentId);
    }

    bool HandleTerminalKeyDown(std::wstring& pendingInput, const uint32_t vkey) noexcept
    {
        switch (vkey)
        {
        case _vkBack:
            if (!pendingInput.empty())
            {
                pendingInput.pop_back();
            }
            return false;
        case _vkReturn:
            return true;
        default:
            return false;
        }
    }

    bool HandleTerminalCharInput(std::wstring& pendingInput, const wchar_t character) noexcept
    {
        if (character == L'\r' || character == L'\n')
        {
            return true;
        }
        if (character >= L' ' || character == L'\t')
        {
            pendingInput.push_back(character);
        }
        return false;
    }

    TerminalPendingInputSnapshot ConsumeTerminalPendingInput(std::wstring& pendingInput,
                                                             const std::wstring_view inputOverride)
    {
        TerminalPendingInputSnapshot snapshot;
        snapshot.PendingInputLength = pendingInput.size();
        snapshot.InputText = inputOverride.empty() ? pendingInput : std::wstring{ inputOverride };
        pendingInput.clear();
        return snapshot;
    }

    void SetCapturedLastSubmittedInput(TerminalCaptureState& state,
                                       const std::wstring_view lastSubmittedInput)
    {
        state.LastSubmittedInput = lastSubmittedInput;
    }

    void SeedCapturedInputState(TerminalCaptureState& state,
                                const std::wstring_view workingDirectory,
                                const std::wstring_view command,
                                const std::wstring_view operatingSystem,
                                const std::wstring_view shellType)
    {
        state.InputState.LastWorkingDirectory = workingDirectory;
        state.InputState.LastCommand = command;
        state.InputState.OperatingSystem = operatingSystem;
        state.InputState.ShellType = shellType;
    }

    void SeedCapturedShellMetadata(TerminalCaptureState& state,
                                   const std::wstring_view operatingSystem,
                                   const std::wstring_view shellType)
    {
        if (state.InputState.OperatingSystem.empty())
        {
            state.InputState.OperatingSystem = operatingSystem;
        }
        if (state.InputState.ShellType.empty())
        {
            state.InputState.ShellType = shellType;
        }
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

    bool UpdateTwoPhaseSubmitPreference(bool& preferTwoPhaseSubmit,
                                        const std::wstring_view currentCommandline,
                                        const std::wstring_view lastCommand,
                                        const std::wstring_view lastSubmittedInput,
                                        const std::wstring_view startupAction,
                                        const std::wstring_view explicitCommandline)
    {
        const auto shouldPrefer = ShouldPreferTwoPhaseSubmit(currentCommandline,
                                                             preferTwoPhaseSubmit,
                                                             lastCommand,
                                                             lastSubmittedInput,
                                                             startupAction,
                                                             explicitCommandline);
        preferTwoPhaseSubmit = preferTwoPhaseSubmit || shouldPrefer;
        return shouldPrefer;
    }

    bool EnsureCapturedBufferSnapshot(TerminalCaptureState& state,
                                      const std::wstring_view currentBuffer)
    {
        if (state.HasBufferSnapshot)
        {
            return false;
        }

        state.LastBufferSnapshot = currentBuffer;
        state.HasBufferSnapshot = true;
        return true;
    }

    TerminalInputCaptureResult BuildTerminalInputCaptureResult(TerminalCaptureState& state,
                                                               const std::wstring_view inputOverride,
                                                               const std::wstring_view currentCommandline,
                                                               const std::wstring_view workingDirectory,
                                                               const std::wstring_view currentBuffer)
    {
        TerminalInputCaptureResult result;
        result.PendingInput = ConsumeTerminalPendingInput(state.PendingInput, inputOverride);
        result.FlushPlan = BuildTerminalInputFlushPlan(result.PendingInput.InputText, currentCommandline);
        SetCapturedLastSubmittedInput(state, result.FlushPlan.NormalizedInput);
        std::ignore = EnsureCapturedBufferSnapshot(state, currentBuffer);

        if (result.FlushPlan.Lines.empty())
        {
            return result;
        }

        result.StateChanged = SyncCapturedWorkingDirectory(state.InputState,
                                                           state.LastReportedWorkingDirectory,
                                                           workingDirectory);
        result.Entries.reserve(result.FlushPlan.Lines.size());
        for (const auto& line : result.FlushPlan.Lines)
        {
            result.Entries.emplace_back(TerminalInputCaptureEntry{
                .Text = line,
                .Snapshot = CaptureTrackedTerminalInput(state, line),
            });
            result.StateChanged = true;
        }

        return result;
    }

    TerminalInputSnapshot CaptureTrackedTerminalInput(TerminalCaptureState& state,
                                                      const std::wstring_view line,
                                                      const std::wstring_view knownWorkingDirectory)
    {
        return TrackTerminalInput(state.InputState, line, knownWorkingDirectory);
    }

    std::wstring ResolveCapturedWorkingDirectory(const TerminalCaptureState& state)
    {
        return state.InputState.LastWorkingDirectory;
    }

    std::wstring ResolveCapturedCommand(const TerminalCaptureState& state)
    {
        return state.InputState.LastCommand;
    }

    std::wstring ResolveCapturedOperatingSystem(const TerminalCaptureState& state)
    {
        return state.InputState.OperatingSystem;
    }

    std::wstring ResolveCapturedShellType(const TerminalCaptureState& state)
    {
        return state.InputState.ShellType;
    }

    std::wstring ResolveCapturedStartupAction(const TerminalCaptureState& state)
    {
        return ResolveCapturedStartupAction(state.LastSubmittedInput, state.InputState.LastCommand);
    }

    std::wstring ResolveCapturedStartupAction(const std::wstring_view lastSubmittedInput,
                                              const std::wstring_view lastCommand)
    {
        if (ContainsLineBreak(lastSubmittedInput))
        {
            return std::wstring{ lastSubmittedInput };
        }

        if (!lastCommand.empty())
        {
            return std::wstring{ lastCommand };
        }

        if (!lastSubmittedInput.empty())
        {
            return std::wstring{ lastSubmittedInput };
        }

        return {};
    }

    TerminalInputFlushPlan BuildTerminalInputFlushPlan(const std::wstring_view inputText,
                                                       const std::wstring_view currentCommandline)
    {
        TerminalInputFlushPlan plan;
        plan.NormalizedInput = NormalizeTerminalInput(inputText);
        const auto normalizedCommandline = NormalizeTerminalInput(currentCommandline);
        if (plan.NormalizedInput.empty())
        {
            if (!normalizedCommandline.empty())
            {
                plan.NormalizedInput = normalizedCommandline;
            }
        }
        else if (!normalizedCommandline.empty() &&
                 ContainsLineBreak(normalizedCommandline) &&
                 normalizedCommandline.size() > plan.NormalizedInput.size() &&
                 normalizedCommandline.ends_with(plan.NormalizedInput))
        {
            plan.NormalizedInput = normalizedCommandline;
        }

        plan.Lines = SplitTerminalInputLines(plan.NormalizedInput);
        return plan;
    }

    TerminalOutputCaptureResult BuildTerminalOutputCaptureResult(TerminalInputState& inputState,
                                                                 std::wstring& lastReportedWorkingDirectory,
                                                                 std::wstring& lastBufferSnapshot,
                                                                 bool& hasBufferSnapshot,
                                                                 const std::wstring_view workingDirectory,
                                                                 const std::wstring_view currentBuffer)
    {
        TerminalOutputCaptureResult result;
        result.WorkingDirectoryChanged = SyncCapturedWorkingDirectory(inputState,
                                                                     lastReportedWorkingDirectory,
                                                                     workingDirectory);
        result.OutputSummary = SummarizeTerminalOutput(currentBuffer, lastBufferSnapshot);
        lastBufferSnapshot = currentBuffer;
        hasBufferSnapshot = true;
        return result;
    }

    WorkspaceChatDispatchPlan BuildWorkspaceChatDispatchPlan(const std::wstring_view text,
                                                             const bool useWindowKeyboardTransport,
                                                             const bool bracketedPasteEnabled,
                                                             const bool preferTwoPhaseSubmit)
    {
        static constexpr std::wstring_view submitPayload{ L"\r" };
        static constexpr std::wstring_view bracketedPasteBodyPrefix{ L"\n" };
        static constexpr std::wstring_view pasteBodySuffix{ L"\n" };

        WorkspaceChatDispatchPlan plan;
        const auto useSpecialSubmitPath = bracketedPasteEnabled || preferTwoPhaseSubmit;

        if (useWindowKeyboardTransport)
        {
            plan.Mode = WorkspaceChatDispatchMode::WindowKeyboardInput;
            plan.SubmitPayload = std::wstring{ L"<window-keyboard-enter>" };
            if (useSpecialSubmitPath)
            {
                plan.TextPayload = NormalizeWorkspaceChatSubmitPreviewText(text);
                plan.PastePayload = plan.TextPayload;
                plan.CombinedPayload = plan.TextPayload;
                plan.TwoPhaseSend = true;
            }
            else
            {
                plan.TextPayload = std::wstring{ text };
                plan.CombinedPayload = plan.TextPayload;
                plan.TwoPhaseSend = true;
            }
            return plan;
        }

        if (useSpecialSubmitPath)
        {
            plan.Mode = WorkspaceChatDispatchMode::PasteThenSendInput;
            plan.PastePayload = std::wstring{ text };
            if (bracketedPasteEnabled)
            {
                plan.PastePayload.insert(0, bracketedPasteBodyPrefix);
            }
            plan.PastePayload.append(pasteBodySuffix);
            plan.SubmitPayload = std::wstring{ submitPayload };
            plan.CombinedPayload = plan.PastePayload;
            plan.CombinedPayload.append(plan.SubmitPayload);
            plan.TwoPhaseSend = true;
            return plan;
        }

        plan.Mode = WorkspaceChatDispatchMode::InlineSendInput;
        plan.SubmitPayload = std::wstring{ text };
        plan.SubmitPayload.append(submitPayload);
        plan.CombinedPayload = plan.SubmitPayload;
        return plan;
    }
}
