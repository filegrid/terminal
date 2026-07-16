#include "pch.h"
#include "WorkspaceChatStore.h"
#include "WorkspaceStoragePaths.h"

#include <json/json.h>
#include <til/unicode.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <optional>
#include <sstream>

namespace terminal::workspacechat
{
    namespace
    {
        constexpr std::wstring_view _chatDirectoryName{ L"chat" };
        constexpr std::wstring_view _draftsDirectoryName{ L"drafts" };
        constexpr std::wstring_view _draftFileName{ L"active.json" };

        std::wstring _localDateStamp()
        {
            SYSTEMTIME localTime{};
            GetLocalTime(&localTime);

            wchar_t buffer[16]{};
            swprintf_s(buffer, L"%04u-%02u-%02u", localTime.wYear, localTime.wMonth, localTime.wDay);
            return buffer;
        }

        std::string _toUtf8(std::wstring_view value)
        {
            return til::u16u8(std::wstring{ value });
        }

        std::wstring _toWide(const Json::Value& value, std::string_view key)
        {
            if (!value.isObject() || !value.isMember(std::string{ key }))
            {
                return {};
            }

            const auto& member = value[std::string{ key }];
            if (!member.isString())
            {
                return {};
            }

            return til::u8u16(member.asString());
        }

        uint64_t _toUInt64(const Json::Value& value, std::string_view key)
        {
            if (!value.isObject() || !value.isMember(std::string{ key }))
            {
                return 0;
            }

            const auto& member = value[std::string{ key }];
            if (member.isUInt64())
            {
                return member.asUInt64();
            }
            if (member.isString())
            {
                return _wcstoui64(til::u8u16(member.asString()).c_str(), nullptr, 10);
            }
            return 0;
        }

        bool _ensureParent(const std::filesystem::path& path)
        {
            std::error_code ec;
            if (const auto parent = path.parent_path(); !parent.empty())
            {
                std::filesystem::create_directories(parent, ec);
            }
            return !ec;
        }

        bool _appendJsonLine(const std::filesystem::path& path, const Json::Value& value)
        {
            if (!_ensureParent(path))
            {
                return false;
            }

            Json::StreamWriterBuilder builder;
            builder["indentation"] = "";
            const auto serialized = Json::writeString(builder, value);

            std::ofstream output{ path, std::ios::binary | std::ios::app };
            if (!output)
            {
                return false;
            }

            output.write(serialized.data(), gsl::narrow_cast<std::streamsize>(serialized.size()));
            output.put('\n');
            return output.good();
        }

        std::optional<Json::Value> _parseJsonLine(const std::string& line)
        {
            Json::CharReaderBuilder builder;
            std::string errors;
            Json::Value value;
            std::istringstream stream{ line };
            if (!Json::parseFromStream(builder, stream, &value, &errors))
            {
                return std::nullopt;
            }
            return value;
        }

        ChatMessageEntry _messageFromJson(const Json::Value& value)
        {
            ChatMessageEntry entry;
            entry.Timestamp = _toWide(value, "ts");
            entry.WorkspaceId = _toWide(value, "workspaceId");
            entry.WindowId = _toUInt64(value, "windowId");
            entry.MessageId = _toWide(value, "messageId");
            entry.Role = _toWide(value, "role");
            entry.Text = _toWide(value, "text");
            entry.ReplyTo = _toWide(value, "replyTo");
            entry.CorrelationId = _toWide(value, "correlationId");
            entry.TabId = _toWide(value, "tabId");
            entry.PaneId = _toWide(value, "paneId");
            return entry;
        }

        Json::Value _messageToJson(const ChatMessageEntry& entry)
        {
            Json::Value value{ Json::objectValue };
            value["ts"] = _toUtf8(entry.Timestamp);
            value["workspaceId"] = _toUtf8(entry.WorkspaceId);
            value["windowId"] = Json::UInt64{ entry.WindowId };
            value["messageId"] = _toUtf8(entry.MessageId);
            value["role"] = _toUtf8(entry.Role);
            value["text"] = _toUtf8(entry.Text);
            value["replyTo"] = _toUtf8(entry.ReplyTo);
            value["correlationId"] = _toUtf8(entry.CorrelationId);
            value["tabId"] = _toUtf8(entry.TabId);
            value["paneId"] = _toUtf8(entry.PaneId);
            return value;
        }
    }

    std::filesystem::path WorkspaceChatStore::_workspaceDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey)
    {
        return ResolveWorkspaceArtifactDirectory(workspaceKey, tabKey);
    }

    std::filesystem::path WorkspaceChatStore::_chatDirectory(std::wstring_view workspaceKey, std::wstring_view tabKey)
    {
        return _workspaceDirectory(workspaceKey, tabKey) / _chatDirectoryName;
    }

    std::filesystem::path WorkspaceChatStore::_draftPath(std::wstring_view workspaceKey, std::wstring_view tabKey)
    {
        return _workspaceDirectory(workspaceKey, tabKey) / _draftsDirectoryName / _draftFileName;
    }

    WorkspaceChatSnapshot WorkspaceChatStore::LoadSnapshot(std::wstring_view workspaceKey, std::wstring_view tabKey, const size_t maxMessages) const
    {
        WorkspaceChatSnapshot snapshot;
        snapshot.Draft = LoadDraft(workspaceKey, tabKey);

        const auto chatDirectory = _chatDirectory(workspaceKey, tabKey);
        std::error_code ec;
        if (!std::filesystem::exists(chatDirectory, ec) || ec)
        {
            return snapshot;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator{ chatDirectory, ec })
        {
            if (ec)
            {
                break;
            }
            if (entry.is_regular_file() && entry.path().extension() == L".jsonl")
            {
                files.emplace_back(entry.path());
            }
        }

        std::sort(files.begin(), files.end());
        std::deque<ChatMessageEntry> recentMessages;

        for (const auto& file : files)
        {
            std::ifstream input{ file, std::ios::binary };
            if (!input)
            {
                continue;
            }

            for (std::string line; std::getline(input, line);)
            {
                if (line.empty())
                {
                    continue;
                }

                const auto json = _parseJsonLine(line);
                if (!json)
                {
                    continue;
                }

                recentMessages.emplace_back(_messageFromJson(*json));
                while (recentMessages.size() > maxMessages)
                {
                    recentMessages.pop_front();
                }
            }
        }

        snapshot.Messages.assign(recentMessages.begin(), recentMessages.end());
        return snapshot;
    }

    std::wstring WorkspaceChatStore::LoadDraft(std::wstring_view workspaceKey, std::wstring_view tabKey) const
    {
        const auto path = _draftPath(workspaceKey, tabKey);
        std::ifstream input{ path, std::ios::binary };
        if (!input)
        {
            return {};
        }

        const std::string jsonString{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
        if (jsonString.empty())
        {
            return {};
        }

        const auto json = _parseJsonLine(jsonString);
        if (!json)
        {
            return {};
        }

        return _toWide(*json, "text");
    }

    bool WorkspaceChatStore::SaveDraft(std::wstring_view workspaceKey, std::wstring_view tabKey, std::wstring_view draft) const
    {
        const auto path = _draftPath(workspaceKey, tabKey);
        if (!_ensureParent(path))
        {
            return false;
        }

        Json::Value value{ Json::objectValue };
        value["workspaceId"] = _toUtf8(workspaceKey);
        value["text"] = _toUtf8(draft);

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        const auto serialized = Json::writeString(builder, value);

        std::ofstream output{ path, std::ios::binary | std::ios::trunc };
        if (!output)
        {
            return false;
        }

        output.write(serialized.data(), gsl::narrow_cast<std::streamsize>(serialized.size()));
        return output.good();
    }

    bool WorkspaceChatStore::AppendMessage(const ChatMessageEntry& entry, std::wstring_view tabKey) const
    {
        const auto path = _chatDirectory(entry.WorkspaceId, tabKey) / (std::filesystem::path{ _localDateStamp() + L".jsonl" });
        return _appendJsonLine(path, _messageToJson(entry));
    }
}
