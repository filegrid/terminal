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
        WorkspaceChatSnapshot LoadSnapshot(std::wstring_view workspaceKey, size_t maxMessages = 200) const;
        std::wstring LoadDraft(std::wstring_view workspaceKey) const;
        bool SaveDraft(std::wstring_view workspaceKey, std::wstring_view draft) const;
        bool AppendMessage(const ChatMessageEntry& entry) const;

    private:
        static std::filesystem::path _workspaceRoot();
        static std::filesystem::path _workspaceDirectory(std::wstring_view workspaceKey);
        static std::filesystem::path _chatDirectory(std::wstring_view workspaceKey);
        static std::filesystem::path _draftPath(std::wstring_view workspaceKey);
    };
}
