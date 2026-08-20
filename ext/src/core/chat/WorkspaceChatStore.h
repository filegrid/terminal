// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace terminal::workspacechat
{
    struct ChatMessageEntry
    {
        std::wstring Timestamp;
        std::wstring WorkspaceId;
        uint64_t WindowId{};
        std::wstring MessageId;
        std::wstring Role;
        std::wstring Text;
        std::wstring ReplyTo;
        std::wstring CorrelationId;
        std::wstring TabId;
        std::wstring PaneId;
    };

    struct WorkspaceChatSnapshot
    {
        std::vector<ChatMessageEntry> Messages;
        std::wstring Draft;
    };

    class WorkspaceChatStore
    {
    public:
        WorkspaceChatSnapshot LoadSnapshot(std::wstring_view workspaceKey, std::wstring_view tabKey, size_t maxMessages = 200) const;
        std::wstring LoadDraft(std::wstring_view workspaceKey, std::wstring_view tabKey) const;
        bool SaveDraft(std::wstring_view workspaceKey, std::wstring_view tabKey, std::wstring_view draft) const;
        bool AppendMessage(const ChatMessageEntry& entry, std::wstring_view tabKey) const;

    private:
        static std::filesystem::path _workspaceDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey);
        static std::filesystem::path _chatDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey);
        static std::filesystem::path _draftPath(std::wstring_view workspaceKey, std::wstring_view tabKey);
    };
}
