#pragma once

#include "TerminalInputHarness.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace terminal::workspacechat
{
    enum class WorkspaceChatDispatchMode : uint8_t
    {
        InlineSendInput,
        PasteThenSendInput,
        WindowKeyboardInput,
    };

    struct WorkspaceChatDispatchPlan
    {
        WorkspaceChatDispatchMode Mode{ WorkspaceChatDispatchMode::InlineSendInput };
        std::wstring TextPayload;
        std::wstring PastePayload;
        std::wstring SubmitPayload;
        std::wstring CombinedPayload;
        bool TwoPhaseSend{ false };
    };

    struct TerminalInputFlushPlan
    {
        std::wstring NormalizedInput;
        std::vector<std::wstring> Lines;
    };

    struct TerminalOutputCaptureResult
    {
        bool WorkingDirectoryChanged{ false };
        std::wstring OutputSummary;
    };

    struct TerminalPendingInputSnapshot
    {
        size_t PendingInputLength{};
        std::wstring InputText;
    };

    struct TerminalInputCaptureEntry
    {
        std::wstring Text;
        TerminalInputSnapshot Snapshot;
    };

    struct TerminalInputCaptureResult
    {
        TerminalPendingInputSnapshot PendingInput;
        TerminalInputFlushPlan FlushPlan;
        std::vector<TerminalInputCaptureEntry> Entries;
        bool StateChanged{ false };
    };

    struct TerminalCaptureState
    {
        std::wstring PendingInput;
        std::wstring LastSubmittedInput;
        std::wstring LastBufferSnapshot;
        std::wstring LastReportedWorkingDirectory;
        bool HasBufferSnapshot{ false };
        bool PreferTwoPhaseSubmit{ false };
        TerminalInputState InputState;
    };

    std::wstring WorkspaceStorageKey(std::wstring_view currentWorkspaceId, uint64_t windowId);
    std::wstring WorkspaceDraftKey(std::wstring_view workspaceKey, std::optional<uint32_t> focusedTabIndex);
    std::wstring WorkspaceStateKey(std::wstring_view routingKey, uint64_t contentId);

    template<typename TMap>
    TerminalCaptureState* FindTerminalCaptureState(TMap& states, std::wstring_view stateKey)
    {
        const auto it = states.find(std::wstring{ stateKey });
        return it == states.end() ? nullptr : &it->second;
    }

    template<typename TMap>
    const TerminalCaptureState* FindTerminalCaptureState(const TMap& states, std::wstring_view stateKey)
    {
        const auto it = states.find(std::wstring{ stateKey });
        return it == states.end() ? nullptr : &it->second;
    }

    template<typename TMap>
    TerminalCaptureState& GetOrCreateTerminalCaptureState(TMap& states, std::wstring_view stateKey)
    {
        return states[std::wstring{ stateKey }];
    }

    bool HandleTerminalKeyDown(std::wstring& pendingInput, uint32_t vkey) noexcept;
    bool HandleTerminalCharInput(std::wstring& pendingInput, wchar_t character) noexcept;
    TerminalPendingInputSnapshot ConsumeTerminalPendingInput(std::wstring& pendingInput,
                                                             std::wstring_view inputOverride);
    void SetCapturedLastSubmittedInput(TerminalCaptureState& state,
                                       std::wstring_view lastSubmittedInput);
    void SeedCapturedInputState(TerminalCaptureState& state,
                                std::wstring_view workingDirectory,
                                std::wstring_view command,
                                std::wstring_view operatingSystem,
                                std::wstring_view shellType);
    void SeedCapturedShellMetadata(TerminalCaptureState& state,
                                   std::wstring_view operatingSystem,
                                   std::wstring_view shellType);
    bool SyncCapturedWorkingDirectory(TerminalInputState& inputState,
                                      std::wstring& lastReportedWorkingDirectory,
                                      std::wstring_view workingDirectory);

    bool ShouldPreferTwoPhaseSubmit(std::wstring_view currentCommandline,
                                    bool preferTwoPhaseSubmit,
                                    std::wstring_view lastCommand,
                                    std::wstring_view lastSubmittedInput,
                                    std::wstring_view startupAction,
                                    std::wstring_view explicitCommandline);
    bool UpdateTwoPhaseSubmitPreference(bool& preferTwoPhaseSubmit,
                                        std::wstring_view currentCommandline,
                                        std::wstring_view lastCommand,
                                        std::wstring_view lastSubmittedInput,
                                        std::wstring_view startupAction,
                                        std::wstring_view explicitCommandline);
    bool EnsureCapturedBufferSnapshot(TerminalCaptureState& state,
                                      std::wstring_view currentBuffer);
    TerminalInputCaptureResult BuildTerminalInputCaptureResult(TerminalCaptureState& state,
                                                               std::wstring_view inputOverride,
                                                               std::wstring_view currentCommandline,
                                                               std::wstring_view workingDirectory,
                                                               std::wstring_view currentBuffer);
    TerminalInputSnapshot CaptureTrackedTerminalInput(TerminalCaptureState& state,
                                                      std::wstring_view line,
                                                      std::wstring_view knownWorkingDirectory = {});
    std::wstring ResolveCapturedWorkingDirectory(const TerminalCaptureState& state);
    std::wstring ResolveCapturedCommand(const TerminalCaptureState& state);
    std::wstring ResolveCapturedOperatingSystem(const TerminalCaptureState& state);
    std::wstring ResolveCapturedShellType(const TerminalCaptureState& state);
    std::wstring ResolveCapturedStartupAction(const TerminalCaptureState& state);
    std::wstring ResolveCapturedStartupAction(std::wstring_view lastSubmittedInput,
                                              std::wstring_view lastCommand);
    TerminalInputFlushPlan BuildTerminalInputFlushPlan(std::wstring_view inputText,
                                                       std::wstring_view currentCommandline);
    TerminalOutputCaptureResult BuildTerminalOutputCaptureResult(TerminalInputState& inputState,
                                                                 std::wstring& lastReportedWorkingDirectory,
                                                                 std::wstring& lastBufferSnapshot,
                                                                 bool& hasBufferSnapshot,
                                                                 std::wstring_view workingDirectory,
                                                                 std::wstring_view currentBuffer);
    WorkspaceChatDispatchPlan BuildWorkspaceChatDispatchPlan(std::wstring_view text,
                                                             bool useWindowKeyboardTransport,
                                                             bool bracketedPasteEnabled,
                                                             bool preferTwoPhaseSubmit);

    template<typename TPendingCapture>
    void UpsertPendingTerminalOutputCapture(std::vector<TPendingCapture>& pendingCaptures, TPendingCapture pending)
    {
        pendingCaptures.erase(std::remove_if(pendingCaptures.begin(),
                                             pendingCaptures.end(),
                                             [&](const auto& existing) {
                                                 return existing.ContentId == pending.ContentId;
                                             }),
                              pendingCaptures.end());
        pendingCaptures.emplace_back(std::move(pending));
    }

    template<typename TPendingCapture>
    std::vector<TPendingCapture> TakeReadyPendingTerminalOutputCaptures(std::vector<TPendingCapture>& pendingCaptures, uint64_t now)
    {
        std::vector<TPendingCapture> ready;
        auto keep = pendingCaptures.begin();
        for (auto it = pendingCaptures.begin(); it != pendingCaptures.end(); ++it)
        {
            if (it->DueTick > now)
            {
                *keep++ = std::move(*it);
                continue;
            }

            ready.emplace_back(std::move(*it));
        }

        pendingCaptures.erase(keep, pendingCaptures.end());
        return ready;
    }
}
