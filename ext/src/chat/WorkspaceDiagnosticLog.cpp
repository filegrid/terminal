#include "pch.h"
#include "WorkspaceDiagnosticLog.h"

#include "WorkspaceStoragePaths.h"

#include <fmt/format.h>
#include <til/unicode.h>

#include <fstream>
#include <mutex>

namespace terminal::workspacechat
{
    namespace
    {
        std::wstring _timestampNow()
        {
            SYSTEMTIME localTime{};
            GetLocalTime(&localTime);

            TIME_ZONE_INFORMATION timeZoneInfo{};
            const auto timeZoneState = GetTimeZoneInformation(&timeZoneInfo);
            long biasMinutes = timeZoneInfo.Bias;
            if (timeZoneState == TIME_ZONE_ID_DAYLIGHT)
            {
                biasMinutes += timeZoneInfo.DaylightBias;
            }
            else if (timeZoneState == TIME_ZONE_ID_STANDARD)
            {
                biasMinutes += timeZoneInfo.StandardBias;
            }

            const auto utcOffsetMinutes = -biasMinutes;
            const auto offsetHours = utcOffsetMinutes / 60;
            const auto offsetMinutes = std::abs(utcOffsetMinutes % 60);

            return fmt::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}{:+03}:{:02}",
                               localTime.wYear,
                               localTime.wMonth,
                               localTime.wDay,
                               localTime.wHour,
                               localTime.wMinute,
                               localTime.wSecond,
                               localTime.wMilliseconds,
                               offsetHours,
                               offsetMinutes);
        }

        bool _containsLineBreak(std::wstring_view value) noexcept
        {
            return value.find(L'\r') != std::wstring_view::npos ||
                   value.find(L'\n') != std::wstring_view::npos;
        }

        uint64_t _lineCount(std::wstring_view value) noexcept
        {
            if (value.empty())
            {
                return 0;
            }

            uint64_t count = 1;
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (value[i] == L'\r')
                {
                    ++count;
                    if (i + 1 < value.size() && value[i + 1] == L'\n')
                    {
                        ++i;
                    }
                }
                else if (value[i] == L'\n')
                {
                    ++count;
                }
            }
            return count;
        }
    }

    std::string DiagnosticUtf8(const std::wstring_view value)
    {
        return til::u16u8(std::wstring{ value });
    }

    void AddOptionalDiagnosticString(Json::Value& object, const std::string_view key, const std::wstring_view value)
    {
        if (!value.empty())
        {
            object[std::string{ key }] = DiagnosticUtf8(value);
        }
    }

    void AddDiagnosticTextFields(Json::Value& object, const std::string_view prefix, const std::wstring_view value, const size_t previewLimit)
    {
        const std::string base{ prefix };
        object[base + "Length"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(value.size()) };
        object[base + "LineCount"] = Json::UInt64{ _lineCount(value) };
        object[base + "HasLineBreaks"] = _containsLineBreak(value);

        const auto previewLength = std::min(value.size(), previewLimit);
        object[base + "Preview"] = DiagnosticUtf8(value.substr(0, previewLength));
    }

    std::wstring WindowClassName(const HWND hwnd)
    {
        if (!hwnd)
        {
            return {};
        }

        wchar_t buffer[256]{};
        const auto copied = GetClassNameW(hwnd, buffer, gsl::narrow_cast<int>(std::size(buffer)));
        if (copied <= 0)
        {
            return {};
        }

        return std::wstring{ buffer, gsl::narrow_cast<size_t>(copied) };
    }

    void AppendHwndDiagnostic(Json::Value& payload, const std::string_view keyPrefix, const HWND hwnd)
    {
        if (!hwnd)
        {
            return;
        }

        payload[std::string{ keyPrefix } + "Hwnd"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(reinterpret_cast<uintptr_t>(hwnd)) };
        AddOptionalDiagnosticString(payload, std::string{ keyPrefix } + "HwndClass", WindowClassName(hwnd));
    }

    void AppendGuiThreadFocusDiagnostics(Json::Value& payload, const std::string_view keyPrefix)
    {
        GUITHREADINFO info{};
        info.cbSize = sizeof(info);
        if (!GetGUIThreadInfo(0, &info))
        {
            return;
        }

        AppendHwndDiagnostic(payload, std::string{ keyPrefix } + "Active", info.hwndActive);
        AppendHwndDiagnostic(payload, std::string{ keyPrefix } + "Focus", info.hwndFocus);
        AppendHwndDiagnostic(payload, std::string{ keyPrefix } + "Capture", info.hwndCapture);
        AppendHwndDiagnostic(payload, std::string{ keyPrefix } + "Caret", info.hwndCaret);
    }

    bool ActivateWindowForKeyboardInput(const HWND hwnd)
    {
        if (!hwnd)
        {
            return false;
        }

        if (IsIconic(hwnd))
        {
            ShowWindow(hwnd, SW_RESTORE);
        }
        else
        {
            ShowWindow(hwnd, SW_SHOW);
        }

        BringWindowToTop(hwnd);
        SetActiveWindow(hwnd);
        return SetForegroundWindow(hwnd) != FALSE;
    }

    std::wstring NormalizeWorkspaceChatSubmitText(const std::wstring_view text)
    {
        std::wstring normalized;
        normalized.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i)
        {
            const auto ch = text[i];
            if (ch == L'\r')
            {
                if (i + 1 < text.size() && text[i + 1] == L'\n')
                {
                    ++i;
                }
                normalized.append(L"<br>");
            }
            else if (ch == L'\n')
            {
                normalized.append(L"<br>");
            }
            else
            {
                normalized.push_back(ch);
            }
        }
        return normalized;
    }

    bool AppendWorkspaceDiagnosticLog(const std::wstring_view eventName, const Json::Value& payload)
    {
        static std::mutex lock;
        std::scoped_lock guard{ lock };

        const auto logsDirectory = ResolveWorkspaceLogsDirectory();
        if (logsDirectory.empty())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(logsDirectory, ec);
        if (ec)
        {
            return false;
        }

        Json::Value root{ Json::objectValue };
        root["ts"] = DiagnosticUtf8(_timestampNow());
        root["event"] = DiagnosticUtf8(eventName);
        root["pid"] = Json::UInt64{ GetCurrentProcessId() };
        root["tid"] = Json::UInt64{ GetCurrentThreadId() };
        root["payload"] = payload;

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        const auto serialized = Json::writeString(builder, root);

        std::ofstream output{ logsDirectory / std::filesystem::path{ L"workspace-chat-diagnostics.jsonl" }, std::ios::binary | std::ios::app };
        if (!output)
        {
            return false;
        }

        output.write(serialized.data(), gsl::narrow_cast<std::streamsize>(serialized.size()));
        output.put('\n');
        return output.good();
    }
}
