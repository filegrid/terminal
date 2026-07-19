// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalEventStore.h"
#include "WorkspaceChatStore.h"

#include <string>
#include <string_view>

namespace terminal::workspacechat
{
    using WorkspaceChatSubmitDiagnosticHook = void(*)(const ChatMessageEntry&);

    class WorkspaceChatController
    {
    public:
        WorkspaceChatSnapshot LoadWorkspace(std::wstring_view workspaceKey, std::wstring_view tabKey, size_t maxMessages = 200) const;
        std::wstring LoadDraft(std::wstring_view workspaceKey, std::wstring_view tabKey) const;
        bool SaveDraft(std::wstring_view workspaceKey, std::wstring_view tabKey, std::wstring_view draft) const;

        void SetSubmitDiagnosticHook(WorkspaceChatSubmitDiagnosticHook hook) noexcept;

        ChatMessageEntry SubmitUserMessage(std::wstring_view workspaceKey,
                                           std::wstring_view tabKey,
                                           uint64_t windowId,
                                           std::wstring_view tabId,
                                           std::wstring_view paneId,
                                           std::wstring_view text);

        bool LogTerminalInput(std::wstring_view workspaceKey,
                              std::wstring_view tabKey,
                              std::wstring_view tabId,
                              std::wstring_view paneId,
                              std::wstring_view text,
                              std::wstring_view workingDirectory = {},
                              std::wstring_view command = {},
                              std::wstring correlationId = {});

        bool LogTerminalOutput(std::wstring_view workspaceKey,
                               std::wstring_view tabKey,
                               std::wstring_view tabId,
                               std::wstring_view paneId,
                               std::wstring_view text,
                               std::wstring_view workingDirectory = {},
                               std::wstring_view command = {},
                               std::wstring correlationId = {});

        std::wstring ConsumePendingCorrelationId();

    private:
        WorkspaceChatStore _chatStore;
        TerminalEventStore _terminalStore;
        WorkspaceChatSubmitDiagnosticHook _submitDiagnosticHook{ nullptr };
        std::wstring _pendingCorrelationId;
    };
}
