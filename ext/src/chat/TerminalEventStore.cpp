#include "pch.h"
#include "TerminalEventStore.h"

#include <json/json.h>
#include <shlobj.h>
#include <til/unicode.h>
#include <wil/resource.h>

#include <fstream>

namespace terminal::workspacechat
{
    namespace
    {
        constexpr std::wstring_view _workspacesDirectoryName{ L"workspaces" };
        constexpr std::wstring_view _terminalDirectoryName{ L"terminal" };

        std::wstring _sanitizePathComponent(std::wstring_view value)
        {
            std::wstring sanitized;
            sanitized.reserve(value.size());
            for (const auto ch : value)
            {
                switch (ch)
                {
                case L'<':
                case L'>':
                case L':':
                case L'"':
                case L'/':
                case L'\\':
                case L'|':
                case L'?':
                case L'*':
                    sanitized.push_back(L'_');
                    break;
                default:
                    sanitized.push_back(ch);
                    break;
                }
            }

            if (sanitized.empty())
            {
                sanitized = L"_";
            }
            return sanitized;
        }

        std::filesystem::path _workspaceRoot()
        {
            if (const auto userProfile = wil::TryGetEnvironmentVariableW<std::wstring>(L"USERPROFILE"); !userProfile.empty())
            {
                return std::filesystem::path{ userProfile } / L".wt";
            }

            wil::unique_cotaskmem_string profileFolder;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &profileFolder)) && profileFolder)
            {
                return std::filesystem::path{ profileFolder.get() } / L".wt";
            }

            return {};
        }

        std::filesystem::path _workspaceDirectory(std::wstring_view workspaceKey)
        {
            return _workspaceRoot() / _workspacesDirectoryName / _sanitizePathComponent(workspaceKey);
        }

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
    }

    bool TerminalEventStore::AppendEvent(const TerminalEventEntry& entry) const
    {
        const auto path = _workspaceDirectory(entry.WorkspaceId) / _terminalDirectoryName / (std::filesystem::path{ _localDateStamp() + L".jsonl" });

        std::error_code ec;
        if (const auto parent = path.parent_path(); !parent.empty())
        {
            std::filesystem::create_directories(parent, ec);
            if (ec)
            {
                return false;
            }
        }

        Json::Value value{ Json::objectValue };
        value["ts"] = _toUtf8(entry.Timestamp);
        value["workspaceId"] = _toUtf8(entry.WorkspaceId);
        value["tabId"] = _toUtf8(entry.TabId);
        value["paneId"] = _toUtf8(entry.PaneId);
        value["eventId"] = _toUtf8(entry.EventId);
        value["kind"] = _toUtf8(entry.Kind);
        value["text"] = _toUtf8(entry.Text);
        value["correlationId"] = _toUtf8(entry.CorrelationId);
        if (!entry.WorkingDirectory.empty())
        {
            value["cwd"] = _toUtf8(entry.WorkingDirectory);
        }
        if (!entry.Command.empty())
        {
            value["cmd"] = _toUtf8(entry.Command);
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
}
