#include "WorkspaceCore.h"

#include "WorkspacePersistencePaths.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cwctype>
#include <fstream>
#include <cmath>
#include <iterator>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>

namespace terminal::workspace
{
    namespace
    {
        constexpr int32_t _workspaceManagerWorkspaceSelectionBase{ 1000 };
        constexpr int32_t _workspaceManagerWorkspaceSelectionStride{ 100 };
        constexpr int32_t _workspaceManagerNodeSelectionBase{ 10 };

        std::vector<std::wstring> _captureVisibleWorkspaceNodeOrder(const std::vector<WorkspaceNode>& nodes);
        std::vector<size_t> _orderedVisibleWorkspaceNodeIndices(const Workspace& workspace);
        void _writeMultilineValue(std::wostringstream& stream,
                                  std::wstring_view indent,
                                  std::wstring_view key,
                                  std::wstring_view value);
        std::vector<std::wstring> _parseMultilineEntries(std::wstring_view value);
        constexpr std::wstring_view _workspaceWindowStateMappingName{ L"Local\\FileGridTerminalWorkspaceWindowState.v1" };
        constexpr std::wstring_view _workspaceWindowStateMutexName{ L"Local\\FileGridTerminalWorkspaceWindowStateMutex.v1" };
        constexpr uint32_t _workspaceWindowStateVersion{ 1 };
        constexpr size_t _workspaceWindowStateCapacity{ 128 };
        constexpr size_t _workspaceWindowStateWorkspaceIdCapacity{ 128 };
        constexpr size_t _workspaceWindowStateWindowNameCapacity{ 256 };
        constexpr size_t _workspaceWindowStateWorkspaceNameCapacity{ 256 };
        constexpr size_t _workspaceWindowStateProcessNameCapacity{ 128 };
        constexpr uint64_t WorkspaceWindowStateHeartbeatIntervalMs{ 3000 };
        constexpr uint64_t WorkspaceWindowStateHeartbeatTimeoutMs{ 12000 };

        struct RuntimeWorkspaceStateRecord
        {
            uint64_t WindowId;
            uint32_t ProcessId;
            uint64_t LastSeenTick;
            wchar_t WorkspaceId[_workspaceWindowStateWorkspaceIdCapacity];
            wchar_t ProcessName[_workspaceWindowStateProcessNameCapacity];
            wchar_t WorkspaceName[_workspaceWindowStateWorkspaceNameCapacity];
            wchar_t WindowName[_workspaceWindowStateWindowNameCapacity];
        };

        struct RuntimeWorkspaceStateBlock
        {
            uint32_t Version;
            uint32_t Reserved;
            RuntimeWorkspaceStateRecord Records[_workspaceWindowStateCapacity];
        };

        template<size_t N>
        void _copyWorkspaceStateString(std::wstring_view value, wchar_t (&destination)[N]) noexcept;
        template<size_t N>
        std::wstring_view _workspaceStateStringView(const wchar_t (&value)[N]) noexcept;
        std::wstring _resolveWorkspaceName(std::wstring_view workspaceId);
        std::wstring _queryProcessImageName(const uint32_t processId);
        bool _isWorkspaceWindowProcessAlive(const WorkspaceStateWindow& window) noexcept;
        template<typename TManager, typename TApply>
        std::optional<TManager> _persistManagerChange(TManager manager, TApply&& apply);
        bool _workspaceStateRecordExpired(const RuntimeWorkspaceStateRecord& record, uint64_t now);
        void _clearWorkspaceStateRecord(RuntimeWorkspaceStateRecord& record) noexcept;
        void _initializeWorkspaceStateBlock(RuntimeWorkspaceStateBlock& block) noexcept;
        void _pruneWorkspaceStateBlock(RuntimeWorkspaceStateBlock& block, uint64_t now) noexcept;
        WorkspaceStateManager _loadRuntimeWorkspaceStateManagerFromBlock(const RuntimeWorkspaceStateBlock& block);
        void _saveRuntimeWorkspaceStateManagerToBlock(const WorkspaceStateManager& manager,
                                                      RuntimeWorkspaceStateBlock& block,
                                                      uint64_t now,
                                                      uint64_t refreshedWindowId = 0) noexcept;
        template<typename TCallback>
        auto _withRuntimeWorkspaceStateBlock(TCallback&& callback);
    }

    namespace
    {
        std::wstring_view _trimLeft(const std::wstring_view value)
        {
            const auto pos = value.find_first_not_of(L" \t\r\n");
            return pos == std::wstring_view::npos ? std::wstring_view{} : value.substr(pos);
        }

        std::wstring_view _trimRight(const std::wstring_view value)
        {
            const auto pos = value.find_last_not_of(L" \t\r\n");
            return pos == std::wstring_view::npos ? std::wstring_view{} : value.substr(0, pos + 1);
        }

        std::wstring_view _trim(const std::wstring_view value)
        {
            return _trimRight(_trimLeft(value));
        }

        std::wstring _queryProcessImageName(const uint32_t processId)
        {
            if (processId == 0)
            {
                return {};
            }

            const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
            if (process == nullptr)
            {
                return {};
            }

            auto closeProcess = wil::scope_exit([&]() noexcept {
                CloseHandle(process);
            });

            std::wstring buffer(MAX_PATH, L'\0');
            auto length = gsl::narrow_cast<DWORD>(buffer.size());
            if (!QueryFullProcessImageNameW(process, 0, buffer.data(), &length) || length == 0)
            {
                return {};
            }

            buffer.resize(length);
            return std::filesystem::path{ buffer }.filename().wstring();
        }

        bool _isWorkspaceWindowProcessAlive(const WorkspaceStateWindow& window) noexcept
        {
            if (window.ProcessId == 0 || window.ProcessName.empty())
            {
                return false;
            }

            const auto currentProcessName = _queryProcessImageName(window.ProcessId);
            return !currentProcessName.empty() && _wcsicmp(currentProcessName.c_str(), window.ProcessName.c_str()) == 0;
        }

        template<size_t N>
        void _copyWorkspaceStateString(const std::wstring_view value, wchar_t (&destination)[N]) noexcept
        {
            std::fill(std::begin(destination), std::end(destination), L'\0');
            const auto length = (std::min)(value.size(), N - 1);
            if (length == 0)
            {
                return;
            }

            std::copy_n(value.data(), length, destination);
        }

        template<size_t N>
        std::wstring_view _workspaceStateStringView(const wchar_t (&value)[N]) noexcept
        {
            size_t length = 0;
            while (length < N && value[length] != L'\0')
            {
                ++length;
            }

            return { value, length };
        }

        std::wstring _resolveWorkspaceName(const std::wstring_view workspaceId)
        {
            if (workspaceId.empty())
            {
                return {};
            }

            const auto manager = WorkspaceManager::Load();
            if (const auto workspace = manager.FindById(workspaceId))
            {
                return workspace->Name;
            }

            return {};
        }

        bool _workspaceStateRecordExpired(const RuntimeWorkspaceStateRecord& record, const uint64_t now)
        {
            if (record.WindowId == 0)
            {
                return true;
            }

            const auto workspaceId = _workspaceStateStringView(record.WorkspaceId);
            if (workspaceId.empty())
            {
                return true;
            }

            const WorkspaceStateWindow window{
                .WindowId = record.WindowId,
                .ProcessId = record.ProcessId,
                .ProcessName = std::wstring{ _workspaceStateStringView(record.ProcessName) },
                .WindowName = std::wstring{ _workspaceStateStringView(record.WindowName) },
                .WorkspaceId = std::wstring{ workspaceId },
            };
            if (!_isWorkspaceWindowProcessAlive(window))
            {
                return true;
            }

            const auto expectedWorkspaceName = _workspaceStateStringView(record.WorkspaceName);
            if (expectedWorkspaceName.empty())
            {
                return true;
            }

            const auto actualWorkspaceName = _resolveWorkspaceName(workspaceId);
            if (actualWorkspaceName.empty() || actualWorkspaceName != expectedWorkspaceName)
            {
                return true;
            }

            return now > record.LastSeenTick && now - record.LastSeenTick > WorkspaceWindowStateHeartbeatTimeoutMs;
        }

        void _clearWorkspaceStateRecord(RuntimeWorkspaceStateRecord& record) noexcept
        {
            record = {};
        }

        void _initializeWorkspaceStateBlock(RuntimeWorkspaceStateBlock& block) noexcept
        {
            block = {};
            block.Version = _workspaceWindowStateVersion;
        }

        void _pruneWorkspaceStateBlock(RuntimeWorkspaceStateBlock& block, const uint64_t now) noexcept
        {
            if (block.Version != _workspaceWindowStateVersion)
            {
                _initializeWorkspaceStateBlock(block);
                return;
            }

            for (auto& record : block.Records)
            {
                if (_workspaceStateRecordExpired(record, now))
                {
                    _clearWorkspaceStateRecord(record);
                }
            }
        }

        WorkspaceStateManager _loadRuntimeWorkspaceStateManagerFromBlock(const RuntimeWorkspaceStateBlock& block)
        {
            WorkspaceStateManager manager;
            std::vector<WorkspaceStateWindow> windows;
            windows.reserve(_workspaceWindowStateCapacity);

            for (const auto& record : block.Records)
            {
                const auto workspaceId = _workspaceStateStringView(record.WorkspaceId);
                if (record.WindowId == 0 || workspaceId.empty())
                {
                    continue;
                }

                windows.emplace_back(WorkspaceStateWindow{
                    .WindowId = record.WindowId,
                    .ProcessId = record.ProcessId,
                    .ProcessName = std::wstring{ _workspaceStateStringView(record.ProcessName) },
                    .WindowName = std::wstring{ _workspaceStateStringView(record.WindowName) },
                    .WorkspaceId = std::wstring{ workspaceId },
                });
            }

            manager.SetWindows(std::move(windows));
            return manager;
        }

        void _saveRuntimeWorkspaceStateManagerToBlock(const WorkspaceStateManager& manager,
                                                      RuntimeWorkspaceStateBlock& block,
                                                      const uint64_t now,
                                                      const uint64_t refreshedWindowId) noexcept
        {
            const auto previousBlock = block;
            _initializeWorkspaceStateBlock(block);

            size_t index = 0;
            for (const auto& window : manager.Windows())
            {
                if (index >= _workspaceWindowStateCapacity || window.WindowId == 0 || window.WorkspaceId.empty())
                {
                    continue;
                }

                auto& record = block.Records[index++];
                record.WindowId = window.WindowId;
                record.ProcessId = window.ProcessId;
                const auto previous = std::find_if(std::begin(previousBlock.Records), std::end(previousBlock.Records), [&](const auto& candidate) {
                    return candidate.WindowId == window.WindowId;
                });
                record.LastSeenTick = window.WindowId == refreshedWindowId || previous == std::end(previousBlock.Records) ? now : previous->LastSeenTick;
                _copyWorkspaceStateString(window.WorkspaceId, record.WorkspaceId);
                _copyWorkspaceStateString(window.ProcessName, record.ProcessName);
                _copyWorkspaceStateString(_resolveWorkspaceName(window.WorkspaceId), record.WorkspaceName);
                _copyWorkspaceStateString(window.WindowName, record.WindowName);
            }
        }

        template<typename TCallback>
        auto _withRuntimeWorkspaceStateBlock(TCallback&& callback)
        {
            using TResult = decltype(callback(std::declval<RuntimeWorkspaceStateBlock&>(), uint64_t{}));

            // A named page-file mapping only exists while at least one handle to it is
            // open. Keep one handle alive for the lifetime of this process; otherwise
            // every call below closes the last handle and the next caller observes a
            // newly zeroed block instead of shared runtime state.
            static const HANDLE processLifetimeMapping = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                                                             nullptr,
                                                                             PAGE_READWRITE,
                                                                             0,
                                                                             sizeof(RuntimeWorkspaceStateBlock),
                                                                             _workspaceWindowStateMappingName.data());

            auto result = TResult{};
            if (processLifetimeMapping == nullptr)
            {
                return result;
            }

            const auto mutex = CreateMutexW(nullptr, FALSE, _workspaceWindowStateMutexName.data());
            if (mutex == nullptr)
            {
                return result;
            }

            const auto waitResult = WaitForSingleObject(mutex, 500);
            if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED)
            {
                CloseHandle(mutex);
                return result;
            }

            auto* block = static_cast<RuntimeWorkspaceStateBlock*>(MapViewOfFile(processLifetimeMapping,
                                                                                  FILE_MAP_READ | FILE_MAP_WRITE,
                                                                                  0,
                                                                                  0,
                                                                                  sizeof(RuntimeWorkspaceStateBlock)));
            if (block == nullptr)
            {
                ReleaseMutex(mutex);
                CloseHandle(mutex);
                return result;
            }

            const auto now = GetTickCount64();
            _pruneWorkspaceStateBlock(*block, now);
            result = callback(*block, now);

            UnmapViewOfFile(block);
            ReleaseMutex(mutex);
            CloseHandle(mutex);
            return result;
        }

        template<typename TManager, typename TApply>
        std::optional<TManager> _persistManagerChange(TManager manager, TApply&& apply)
        {
            if (!apply(manager) || !manager.Save())
            {
                return std::nullopt;
            }

            return manager;
        }

        std::wstring _unquote(std::wstring_view value)
        {
            value = _trim(value);
            if (value.size() >= 2)
            {
                const auto first = value.front();
                const auto last = value.back();
                if ((first == L'\'' && last == L'\'') || (first == L'"' && last == L'"'))
                {
                    value.remove_prefix(1);
                    value.remove_suffix(1);
                }
            }

            std::wstring result;
            result.reserve(value.size());
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (value[i] == L'\'' && i + 1 < value.size() && value[i + 1] == L'\'')
                {
                    result.push_back(L'\'');
                    ++i;
                }
                else
                {
                    result.push_back(value[i]);
                }
            }
            return result;
        }

        std::wstring _quote(std::wstring_view value)
        {
            std::wstring result;
            result.reserve(value.size() + 2);
            result.push_back(L'\'');
            for (const auto ch : value)
            {
                result.push_back(ch);
                if (ch == L'\'')
                {
                    result.push_back(L'\'');
                }
            }
            result.push_back(L'\'');
            return result;
        }

        std::wstring _quotePosixShellPath(std::wstring_view value)
        {
            std::wstring quoted{ L"\"" };
            size_t offset = 0;
            if (value == L"~" || value.starts_with(L"~/"))
            {
                quoted.append(L"$HOME");
                offset = 1;
            }

            for (const auto ch : value.substr(offset))
            {
                if (ch == L'\\' || ch == L'\"' || ch == L'$' || ch == L'`')
                {
                    quoted.push_back(L'\\');
                }
                quoted.push_back(ch);
            }
            quoted.push_back(L'\"');
            return quoted;
        }

        std::wstring _toLower(std::wstring_view value)
        {
            std::wstring lowered;
            lowered.reserve(value.size());
            for (const auto ch : value)
            {
                lowered.push_back(static_cast<wchar_t>(std::towlower(ch)));
            }
            return lowered;
        }

        bool _isHexDigit(const wchar_t ch) noexcept
        {
            return (ch >= L'0' && ch <= L'9') ||
                   (ch >= L'a' && ch <= L'f') ||
                   (ch >= L'A' && ch <= L'F');
        }

        bool _isAlpha(const wchar_t ch) noexcept
        {
            return std::iswalpha(ch) != 0;
        }

        static constexpr std::array<std::wstring_view, 12> _workspaceColorPalette{
            L"#C50F1F",
            L"#0063B1",
            L"#0F7B0F",
            L"#CA5010",
            L"#8E562E",
            L"#744DA9",
            L"#038387",
            L"#881798",
            L"#498205",
            L"#515C6B",
            L"#567C73",
            L"#7A7574",
        };

        bool _isPowerShellShellType(std::wstring_view shellType)
        {
            const auto lowered = _toLower(shellType);
            return lowered == L"powershell" || lowered == L"pwsh";
        }

        bool _isWslProfileSource(std::wstring_view source)
        {
            const auto lowered = _toLower(source);
            return lowered == L"microsoft.wsl" || lowered == L"windows.terminal.wsl";
        }

        bool _isWslCommandline(std::wstring_view value)
        {
            const auto lowered = _toLower(value);
            return lowered.find(L"wsl.exe") != std::wstring::npos ||
                   lowered == L"wsl" ||
                   lowered.starts_with(L"wsl ") ||
                   lowered.find(L"bash.exe") != std::wstring::npos;
        }

        std::vector<std::wstring_view> _splitCommandlineArguments(const std::wstring_view value)
        {
            std::vector<std::wstring_view> parts;
            size_t start = std::wstring_view::npos;
            wchar_t quote{};
            for (size_t index = 0; index < value.size(); ++index)
            {
                const auto ch = value[index];
                if (quote != L'\0')
                {
                    if (ch == quote)
                    {
                        quote = L'\0';
                    }
                    continue;
                }

                if (ch == L'"' || ch == L'\'')
                {
                    if (start == std::wstring_view::npos)
                    {
                        start = index;
                    }
                    quote = ch;
                    continue;
                }

                if (std::iswspace(ch))
                {
                    if (start != std::wstring_view::npos)
                    {
                        parts.emplace_back(value.substr(start, index - start));
                        start = std::wstring_view::npos;
                    }
                    continue;
                }

                if (start == std::wstring_view::npos)
                {
                    start = index;
                }
            }

            if (start != std::wstring_view::npos)
            {
                parts.emplace_back(value.substr(start));
            }
            return parts;
        }

        std::wstring _buildWindowsSshStartupDirectoryInput(std::wstring_view directory)
        {
            auto normalized = std::wstring{ _trim(directory) };
            std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
            if (normalized.empty())
            {
                return {};
            }

            if (normalized.size() >= 2 && _isAlpha(normalized[0]) && normalized[1] == L':')
            {
                std::wstring input;
                input.push_back(static_cast<wchar_t>(std::towupper(normalized[0])));
                input.push_back(L':');
                input.push_back(L'\r');

                auto remainder = normalized.substr(2);
                if (remainder.empty())
                {
                    return input;
                }

                if (remainder.front() != L'\\')
                {
                    remainder.insert(remainder.begin(), L'\\');
                }

                if (remainder == L"\\")
                {
                    input.append(L"cd \\");
                }
                else
                {
                    input.append(L"cd \"");
                    input.append(remainder);
                    input.append(L"\"");
                }
                input.push_back(L'\r');
                return input;
            }

            std::wstring input{ L"cd \"" };
            input.append(normalized);
            input.append(L"\"\r");
            return input;
        }

        std::wstring _inferOperatingSystemFromPath(std::wstring_view value)
        {
            if (value.empty())
            {
                return {};
            }

            if ((value.size() >= 2 && _isAlpha(value[0]) && value[1] == L':') ||
                value.starts_with(L"\\\\") ||
                value.find(L'\\') != std::wstring_view::npos)
            {
                return L"windows";
            }

            if (value.starts_with(L"/") || value.starts_with(L"~"))
            {
                return L"linux";
            }

            return {};
        }

        std::vector<std::wstring> _buildSshDeferredStartupInputs(const WorkspaceNode& node)
        {
            std::vector<std::wstring> inputs;

            const auto startupDirectory = std::wstring{ _trim(node.StartupDirectory) };
            const auto startupAction = std::wstring{ _trim(node.StartupAction) };
            const auto operatingSystem = _toLower(node.OperatingSystem);
            if (!startupDirectory.empty())
            {
                if (operatingSystem == L"windows" || _inferOperatingSystemFromPath(startupDirectory) == L"windows")
                {
                    if (_isPowerShellShellType(node.ShellType))
                    {
                        std::wstring input{ L"Set-Location -LiteralPath " };
                        input.append(_quote(startupDirectory));
                        input.push_back(L'\r');
                        inputs.emplace_back(std::move(input));
                    }
                    else
                    {
                        inputs.emplace_back(_buildWindowsSshStartupDirectoryInput(startupDirectory));
                    }
                }
                else
                {
                    std::wstring input{ L"cd -- " };
                    input.append(_quotePosixShellPath(startupDirectory));
                    input.push_back(L'\r');
                    inputs.emplace_back(std::move(input));
                }
            }

            if (!startupAction.empty())
            {
                inputs.emplace_back(startupAction);
            }

            return inputs;
        }

        std::pair<std::wstring, std::wstring> _parseKeyValue(std::wstring_view content)
        {
            if (content.starts_with(L"- "))
            {
                content.remove_prefix(2);
            }

            const auto colon = content.find(L':');
            if (colon == std::wstring_view::npos)
            {
                return {};
            }

            std::wstring key{ _trim(content.substr(0, colon)) };
            std::wstring value{ _unquote(content.substr(colon + 1)) };
            return { std::move(key), std::move(value) };
        }

        bool _parseBool(const std::wstring& value, const bool fallback) noexcept
        {
            if (value == L"true")
            {
                return true;
            }
            if (value == L"false")
            {
                return false;
            }
            return fallback;
        }

        uint64_t _parseUnsigned(std::wstring_view value, const uint64_t fallback = 0) noexcept
        {
            value = _trim(value);
            if (value.empty())
            {
                return fallback;
            }

            errno = 0;
            std::wstring parsedValue{ value };
            wchar_t* end = nullptr;
            const auto parsed = std::wcstoull(parsedValue.c_str(), &end, 10);
            return (errno == 0 && end != parsedValue.c_str()) ? parsed : fallback;
        }

        std::string _toUtf8(std::wstring_view value)
        {
            std::string utf8;
            utf8.reserve(value.size());

            for (size_t i = 0; i < value.size(); ++i)
            {
                uint32_t codePoint = static_cast<uint16_t>(value[i]);
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 1 < value.size())
                {
                    const auto low = static_cast<uint16_t>(value[i + 1]);
                    if (low >= 0xDC00 && low <= 0xDFFF)
                    {
                        codePoint = 0x10000 + (((codePoint - 0xD800) << 10) | (low - 0xDC00));
                        ++i;
                    }
                }

                if (codePoint <= 0x7F)
                {
                    utf8.push_back(static_cast<char>(codePoint));
                }
                else if (codePoint <= 0x7FF)
                {
                    utf8.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                    utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                }
                else if (codePoint <= 0xFFFF)
                {
                    utf8.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                    utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                    utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                }
                else
                {
                    utf8.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
                    utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
                    utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                    utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                }
            }

            return utf8;
        }

        std::wstring _fromUtf8(const std::string& value)
        {
            std::wstring wide;
            wide.reserve(value.size());

            for (size_t i = 0; i < value.size();)
            {
                const auto lead = static_cast<unsigned char>(value[i]);
                uint32_t codePoint = 0;
                size_t width = 0;

                if ((lead & 0x80u) == 0)
                {
                    codePoint = lead;
                    width = 1;
                }
                else if ((lead & 0xE0u) == 0xC0u && i + 1 < value.size())
                {
                    codePoint = ((lead & 0x1Fu) << 6) |
                                (static_cast<unsigned char>(value[i + 1]) & 0x3Fu);
                    width = 2;
                }
                else if ((lead & 0xF0u) == 0xE0u && i + 2 < value.size())
                {
                    codePoint = ((lead & 0x0Fu) << 12) |
                                ((static_cast<unsigned char>(value[i + 1]) & 0x3Fu) << 6) |
                                (static_cast<unsigned char>(value[i + 2]) & 0x3Fu);
                    width = 3;
                }
                else if ((lead & 0xF8u) == 0xF0u && i + 3 < value.size())
                {
                    codePoint = ((lead & 0x07u) << 18) |
                                ((static_cast<unsigned char>(value[i + 1]) & 0x3Fu) << 12) |
                                ((static_cast<unsigned char>(value[i + 2]) & 0x3Fu) << 6) |
                                (static_cast<unsigned char>(value[i + 3]) & 0x3Fu);
                    width = 4;
                }
                else
                {
                    wide.push_back(L'?');
                    ++i;
                    continue;
                }

                if (codePoint <= 0xFFFF)
                {
                    wide.push_back(static_cast<wchar_t>(codePoint));
                }
                else
                {
                    codePoint -= 0x10000;
                    wide.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
                    wide.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
                }

                i += width;
            }

            return wide;
        }

        void _applyWindowStateField(WorkspaceStateWindow& window, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"windowId")
            {
                window.WindowId = _parseUnsigned(value);
            }
            else if (key == L"windowName")
            {
                window.WindowName = value;
            }
            else if (key == L"workspaceId")
            {
                window.WorkspaceId = value;
            }
        }

        void _applyWorkspaceField(Workspace& workspace, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"description")
            {
                workspace.Description = value;
            }
            else if (key == L"backgroundColor")
            {
                workspace.BackgroundColor = value;
            }
            else if (key == L"icon")
            {
                workspace.Icon = value;
            }
            else if (key == L"locked")
            {
                // Workspace windows are always locked. Keep accepting the legacy
                // field so existing definitions load, but never restore its value.
                workspace.Locked = true;
            }
            else if (key == L"tabOrder")
            {
                workspace.TabOrder.clear();

                std::wistringstream stream{ value };
                for (std::wstring line; std::getline(stream, line);)
                {
                    const auto trimmed = std::wstring{ _trim(line) };
                    if (!trimmed.empty())
                    {
                        workspace.TabOrder.emplace_back(trimmed);
                    }
                }
            }
        }

        void _applyNodeField(WorkspaceNode& node, const std::wstring& key, const std::wstring& value)
        {
            if (key == L"connectionRef")
            {
                node.ConnectionRef = value;
            }
            else if (key == L"profileGuid")
            {
                node.ProfileGuid = value;
            }
            else if (key == L"profileName")
            {
                node.ProfileName = value;
            }
            else if (key == L"icon")
            {
                node.Icon = value;
            }
            else if (key == L"tabColor")
            {
                node.TabColor = value;
            }
            else if (key == L"showTab")
            {
                node.ShowTab = _parseBool(value, node.ShowTab);
            }
            else if (key == L"startupDirectory")
            {
                node.StartupDirectory = value;
            }
            else if (key == L"startupAction")
            {
                node.StartupAction = value;
            }
            else if (key == L"commands")
            {
                std::wistringstream stream{ value };
                std::vector<WorkspaceNodeCommand> commands;
                for (std::wstring line; std::getline(stream, line);)
                {
                    std::vector<std::wstring> fields;
                    std::wstring field;
                    bool escaped{};
                    for (const auto ch : line)
                    {
                        if (escaped)
                        {
                            field.push_back(ch == L'n' ? L'\n' : ch);
                            escaped = false;
                        }
                        else if (ch == L'\\')
                        {
                            escaped = true;
                        }
                        else if (ch == L'|')
                        {
                            fields.emplace_back(std::move(field));
                            field.clear();
                        }
                        else
                        {
                            field.push_back(ch);
                        }
                    }
                    if (escaped)
                    {
                        field.push_back(L'\\');
                    }
                    fields.emplace_back(std::move(field));
                    if (fields.size() == 4)
                    {
                        commands.emplace_back(WorkspaceNodeCommand{ std::move(fields[0]), std::move(fields[1]), std::move(fields[2]), std::move(fields[3]) });
                    }
                }
                node.Commands = std::move(commands);
            }
            else if (key == L"multiWindowMode")
            {
                node.MultiWindowPreference.DisplayMode = value == L"tab" ? WorkspaceWindowDisplayMode::Tab : WorkspaceWindowDisplayMode::Split;
            }
            else if (key == L"tabPlacement")
            {
                node.MultiWindowPreference.TabPlacement = value == L"top-right" ? WorkspaceTabPlacement::TopRight :
                                                           value == L"bottom-right" ? WorkspaceTabPlacement::BottomRight : WorkspaceTabPlacement::TopLeft;
            }
            else if (key == L"splitWeights")
            {
                std::wistringstream stream{ value };
                std::wstring item;
                node.MultiWindowPreference.SplitWeights.clear();
                while (std::getline(stream, item, L','))
                {
                    try { node.MultiWindowPreference.SplitWeights.emplace_back(std::stod(item)); } catch (...) { node.MultiWindowPreference.SplitWeights.clear(); break; }
                }
            }
            else if (key == L"operatingSystem")
            {
                node.OperatingSystem = value;
            }
            else if (key == L"shellType")
            {
                node.ShellType = value;
            }
            else if (key == L"showInputPanel")
            {
                node.ShowInputPanel = _parseBool(value, node.ShowInputPanel);
            }
            else if (key == L"useNodeNameAsTabTitle")
            {
                node.UseNodeNameAsTabTitle = _parseBool(value, node.UseNodeNameAsTabTitle);
            }
        }

        struct PendingMultilineValue
        {
            uint32_t ContentIndent{};
            std::wstring Key;
            std::wstring Value;
        };

        std::wstring _makeUniqueDirectoryName(const std::wstring& baseName, std::unordered_set<std::wstring>& usedNames)
        {
            std::wstring candidate = baseName;
            auto lowered = _toLower(candidate);
            for (size_t index = 2; !usedNames.emplace(lowered).second; ++index)
            {
                candidate = baseName;
                candidate.append(L" (");
                candidate.append(std::to_wstring(index));
                candidate.push_back(L')');
                lowered = _toLower(candidate);
            }
            return candidate;
        }

        bool _writeUtf8TextFile(const std::filesystem::path& path, const std::wstring& content)
        {
            std::error_code ec;
            const auto parent = path.parent_path();
            if (!parent.empty())
            {
                std::filesystem::create_directories(parent, ec);
                if (ec)
                {
                    return false;
                }
            }

            const auto utf8 = _toUtf8(content);
            std::ofstream output{ path, std::ios::binary | std::ios::trunc };
            if (!output)
            {
                return false;
            }

            output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
            return output.good();
        }

        std::optional<std::wstring> _readUtf8TextFile(const std::filesystem::path& path)
        {
            std::ifstream input{ path, std::ios::binary };
            if (!input)
            {
                return std::nullopt;
            }

            std::string utf8{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
            return _fromUtf8(utf8);
        }

        template<typename T, typename ApplyFn>
        std::optional<std::pair<T, size_t>> _loadFlatYamlObject(const std::filesystem::path& path, ApplyFn&& applyField)
        {
            const auto content = _readUtf8TextFile(path);
            if (!content.has_value())
            {
                return std::nullopt;
            }

            T result{};
            std::optional<PendingMultilineValue> pendingMultilineValue;
            size_t order = 0;

            auto applyPendingValue = [&]() {
                if (pendingMultilineValue)
                {
                    applyField(result, pendingMultilineValue->Key, pendingMultilineValue->Value, order);
                    pendingMultilineValue.reset();
                }
            };

            std::wistringstream stream{ *content };
            for (std::wstring line; std::getline(stream, line);)
            {
                const std::wstring_view rawView{ line };
                const auto contentView = _trimRight(rawView);
                const auto trimmed = _trim(contentView);
                const auto indent = rawView.find_first_not_of(L' ');
                const auto safeIndent = indent == std::wstring_view::npos ? static_cast<uint32_t>(rawView.size()) : static_cast<uint32_t>(indent);

                if (pendingMultilineValue)
                {
                    if (safeIndent >= pendingMultilineValue->ContentIndent)
                    {
                        if (!pendingMultilineValue->Value.empty())
                        {
                            pendingMultilineValue->Value.push_back(L'\n');
                        }

                        const auto start = std::min(static_cast<size_t>(pendingMultilineValue->ContentIndent), rawView.size());
                        pendingMultilineValue->Value.append(rawView.substr(start));
                        continue;
                    }

                    applyPendingValue();
                }

                if (trimmed.empty() || trimmed.starts_with(L"#") || safeIndent != 0)
                {
                    continue;
                }

                const auto [key, value] = _parseKeyValue(trimmed);
                if (key.empty())
                {
                    continue;
                }

                if (value == L"|")
                {
                    pendingMultilineValue = PendingMultilineValue{ 2, key, {} };
                    continue;
                }

                applyField(result, key, value, order);
            }

            applyPendingValue();
            return std::pair<T, size_t>{ std::move(result), order };
        }

        std::optional<std::pair<Workspace, size_t>> _loadWorkspaceMetadataFile(const std::filesystem::path& path)
        {
            return _loadFlatYamlObject<Workspace>(path, [](Workspace& workspace, const std::wstring& key, const std::wstring& value, size_t&) {
                if (key != L"version")
                {
                    _applyWorkspaceField(workspace, key, value);
                }
            });
        }

        std::optional<WorkspaceNode> _loadWorkspaceNodeMetadataFile(const std::filesystem::path& path)
        {
            if (const auto metadata = _loadFlatYamlObject<WorkspaceNode>(path, [](WorkspaceNode& node, const std::wstring& key, const std::wstring& value, size_t&) {
                    if (key != L"version")
                    {
                        _applyNodeField(node, key, value);
                    }
                }))
                {
                    auto node = std::move(metadata->first);
                    // Reading accepts both schemas. New files become a
                    // normalized command list in memory; legacy files remain
                    // readable through EffectiveWorkspaceNodeCommands until
                    // their next save writes the new schema.
                    NormalizeWorkspaceNodeMultiWindowConfig(node);
                    return node;
                }

            return std::nullopt;
        }

        struct PersistedWorkspaceDirectory
        {
            std::filesystem::path Directory;
            Workspace Definition;
        };

        std::optional<std::vector<PersistedWorkspaceDirectory>> _enumeratePersistedWorkspaceDirectories(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::vector<PersistedWorkspaceDirectory> workspaces;
            for (const auto& workspaceEntry : std::filesystem::directory_iterator(path, ec))
            {
                if (ec)
                {
                    return std::nullopt;
                }
                if (!workspaceEntry.is_directory())
                {
                    continue;
                }

                const auto workspaceFile = workspaceEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceMetadataFileName };
                if (!std::filesystem::exists(workspaceFile, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                const auto metadata = _loadWorkspaceMetadataFile(workspaceFile);
                if (!metadata.has_value())
                {
                    continue;
                }

                auto workspace = std::move(metadata->first);
                workspace.Name = workspaceEntry.path().filename().wstring();
                workspace.Id = workspace.Name;

                workspaces.emplace_back(PersistedWorkspaceDirectory{
                    workspaceEntry.path(),
                    std::move(workspace),
                });
            }

            std::stable_sort(workspaces.begin(), workspaces.end(), [](const auto& lhs, const auto& rhs) {
                return _toLower(lhs.Definition.Name) < _toLower(rhs.Definition.Name);
            });
            return workspaces;
        }

        std::optional<std::vector<PersistedWorkspaceDirectory>> _enumeratePersistedWorkspaceDirectoriesIfPresent(const std::filesystem::path& path)
        {
            std::error_code ec;
            if (path.empty() || !std::filesystem::exists(path, ec) || ec)
            {
                return std::vector<PersistedWorkspaceDirectory>{};
            }

            if (!std::filesystem::is_directory(path, ec) || ec)
            {
                return std::vector<PersistedWorkspaceDirectory>{};
            }

            return _enumeratePersistedWorkspaceDirectories(path);
        }

        std::wstring _serializeWorkspaceOrder(const std::vector<Workspace>& workspaces)
        {
            std::wostringstream stream;
            stream << L"version: 1\n";
            if (!workspaces.empty())
            {
                std::wstring serializedOrder;
                for (const auto& workspace : workspaces)
                {
                    const auto& workspaceName = workspace.Name.empty() ? workspace.Id : workspace.Name;
                    if (workspaceName.empty())
                    {
                        continue;
                    }

                    if (!serializedOrder.empty())
                    {
                        serializedOrder.push_back(L'\n');
                    }
                    serializedOrder.append(workspaceName);
                }

                if (!serializedOrder.empty())
                {
                    _writeMultilineValue(stream, L"", L"workspaces", serializedOrder);
                }
            }
            return stream.str();
        }

        std::wstring _serializeWorkspaceMetadata(const Workspace& workspace)
        {
            std::wostringstream stream;
            stream << L"version: 1\n";
            if (!workspace.Description.empty())
            {
                stream << L"description: " << _quote(workspace.Description) << L"\n";
            }
            if (!workspace.BackgroundColor.empty())
            {
                stream << L"backgroundColor: " << _quote(workspace.BackgroundColor) << L"\n";
            }
            if (!workspace.Icon.empty())
            {
                stream << L"icon: " << _quote(workspace.Icon) << L"\n";
            }
            // New definitions have no unlock state.
            stream << L"locked: true\n";
            // Always persist the effective visible-node order. A workspace can
            // be assembled or edited by changing Nodes directly, in which case
            // TabOrder may be empty or stale even though the UI has a definite
            // order. Without this field, the directory loader falls back to
            // alphabetical node-directory order on the next launch.
            const auto effectiveTabOrder = [&]() {
                std::vector<std::wstring> order;
                for (const auto nodeIndex : _orderedVisibleWorkspaceNodeIndices(workspace))
                {
                    const auto& nodeId = workspace.Nodes.at(nodeIndex).Id;
                    if (!nodeId.empty())
                    {
                        order.emplace_back(nodeId);
                    }
                }
                return order;
            }();
            if (!effectiveTabOrder.empty())
            {
                std::wstring serializedOrder;
                for (const auto& nodeId : effectiveTabOrder)
                {
                    if (nodeId.empty())
                    {
                        continue;
                    }

                    if (!serializedOrder.empty())
                    {
                        serializedOrder.push_back(L'\n');
                    }
                    serializedOrder.append(nodeId);
                }

                if (!serializedOrder.empty())
                {
                    _writeMultilineValue(stream, L"", L"tabOrder", serializedOrder);
                }
            }
            return stream.str();
        }

        std::vector<std::wstring> _parseMultilineEntries(const std::wstring_view value)
        {
            std::vector<std::wstring> entries;
            std::wistringstream stream{ std::wstring{ value } };
            for (std::wstring line; std::getline(stream, line);)
            {
                const auto trimmed = std::wstring{ _trim(line) };
                if (!trimmed.empty())
                {
                    entries.emplace_back(trimmed);
                }
            }
            return entries;
        }

        bool _containsLineBreak(std::wstring_view value) noexcept
        {
            return value.find(L'\r') != std::wstring_view::npos ||
                   value.find(L'\n') != std::wstring_view::npos;
        }

        void _writeMultilineValue(std::wostringstream& stream,
                                  std::wstring_view indent,
                                  std::wstring_view key,
                                  std::wstring_view value)
        {
            stream << indent << key << L": |\n";

            size_t start = 0;
            while (start <= value.size())
            {
                auto end = start;
                while (end < value.size() && value[end] != L'\r' && value[end] != L'\n')
                {
                    ++end;
                }

                stream << indent << L"  " << value.substr(start, end - start) << L"\n";
                if (end >= value.size())
                {
                    break;
                }

                if (value[end] == L'\r' && end + 1 < value.size() && value[end + 1] == L'\n')
                {
                    start = end + 2;
                }
                else
                {
                    start = end + 1;
                }
            }
        }

        std::wstring _serializeWorkspaceNodeMetadata(const WorkspaceNode& node)
        {
            // Persistence has a single forward format. A legacy node is
            // converted here, after having been read compatibly from
            // startupAction, so saving performs the schema migration.
            auto serializedNode = node;
            if (serializedNode.Commands.empty())
            {
                serializedNode.Commands = EffectiveWorkspaceNodeCommands(serializedNode);
            }
            NormalizeWorkspaceNodeMultiWindowConfig(serializedNode);
            std::wostringstream stream;
            // v2 introduces the ordered commands and multi-window preference
            // fields. Readers intentionally continue to accept v1 nodes.
            stream << L"version: 2\n";
            if (!node.ConnectionRef.empty())
            {
                stream << L"connectionRef: " << _quote(node.ConnectionRef) << L"\n";
            }
            if (!node.ProfileGuid.empty())
            {
                stream << L"profileGuid: " << _quote(node.ProfileGuid) << L"\n";
            }
            if (!node.ProfileName.empty())
            {
                stream << L"profileName: " << _quote(node.ProfileName) << L"\n";
            }
            if (!node.Icon.empty())
            {
                stream << L"icon: " << _quote(node.Icon) << L"\n";
            }
            if (!node.TabColor.empty())
            {
                stream << L"tabColor: " << _quote(node.TabColor) << L"\n";
            }
            stream << L"showTab: " << (node.ShowTab ? L"true" : L"false") << L"\n";
            if (!node.StartupDirectory.empty())
            {
                stream << L"startupDirectory: " << _quote(node.StartupDirectory) << L"\n";
            }
            if (!serializedNode.Commands.empty())
            {
                const auto escape = [](std::wstring_view value) {
                    std::wstring escaped;
                    for (const auto ch : value)
                    {
                        if (ch == L'\\' || ch == L'|') { escaped.push_back(L'\\'); escaped.push_back(ch); }
                        else if (ch == L'\n') { escaped.append(L"\\n"); }
                        else if (ch != L'\r') { escaped.push_back(ch); }
                    }
                    return escaped;
                };
                std::wstring serialized;
                for (const auto& command : serializedNode.Commands)
                {
                    if (!serialized.empty()) { serialized.push_back(L'\n'); }
                    serialized.append(escape(command.Id)); serialized.push_back(L'|');
                    serialized.append(escape(command.Icon)); serialized.push_back(L'|');
                    serialized.append(escape(command.Name)); serialized.push_back(L'|');
                    serialized.append(escape(command.Command));
                }
                _writeMultilineValue(stream, L"", L"commands", serialized);
                stream << L"multiWindowMode: " << (serializedNode.MultiWindowPreference.DisplayMode == WorkspaceWindowDisplayMode::Tab ? L"tab" : L"split") << L"\n";
                const auto placement = serializedNode.MultiWindowPreference.TabPlacement == WorkspaceTabPlacement::TopRight ? L"top-right" :
                                       serializedNode.MultiWindowPreference.TabPlacement == WorkspaceTabPlacement::BottomRight ? L"bottom-right" : L"top-left";
                stream << L"tabPlacement: " << placement << L"\n";
                stream << L"splitWeights: ";
                for (size_t index = 0; index < serializedNode.MultiWindowPreference.SplitWeights.size(); ++index)
                {
                    if (index != 0) { stream << L','; }
                    stream << serializedNode.MultiWindowPreference.SplitWeights[index];
                }
                stream << L"\n";
            }
            if (!node.OperatingSystem.empty())
            {
                stream << L"operatingSystem: " << _quote(node.OperatingSystem) << L"\n";
            }
            if (!node.ShellType.empty())
            {
                stream << L"shellType: " << _quote(node.ShellType) << L"\n";
            }
            stream << L"showInputPanel: " << (node.ShowInputPanel ? L"true" : L"false") << L"\n";
            stream << L"useNodeNameAsTabTitle: " << (node.UseNodeNameAsTabTitle ? L"true" : L"false") << L"\n";
            return stream.str();
        }

        std::wstring _serializeWorkspaceStateFile(const std::vector<WorkspaceStateWindow>& windows)
        {
            std::wostringstream stream;
            stream << L"version: 1\n";
            stream << L"windows:\n";
            for (const auto& window : windows)
            {
                stream << L"  - windowId: " << _quote(std::to_wstring(window.WindowId)) << L"\n";
                if (!window.WindowName.empty())
                {
                    stream << L"    windowName: " << _quote(window.WindowName) << L"\n";
                }
            }
            return stream.str();
        }

        void _loadWorkspaceStateFile(WorkspaceStateManager& manager, const std::filesystem::path& path, const std::wstring_view workspaceId)
        {
            const auto content = _readUtf8TextFile(path);
            if (!content.has_value())
            {
                return;
            }

            std::wistringstream stream{ *content };

            enum class ParseSection
            {
                None,
                Windows,
            };

            ParseSection section = ParseSection::None;
            std::optional<WorkspaceStateWindow> currentWindow;

            auto appendWindow = [&]() {
                if (!currentWindow)
                {
                    return;
                }

                if (currentWindow->WorkspaceId.empty())
                {
                    currentWindow->WorkspaceId.assign(workspaceId);
                }
                manager.UpsertWindow(std::move(*currentWindow));
                currentWindow.reset();
            };

            for (std::wstring line; std::getline(stream, line);)
            {
                const auto contentView = _trimRight(std::wstring_view{ line });
                const auto trimmed = _trim(contentView);
                if (trimmed.empty() || trimmed.starts_with(L"#"))
                {
                    continue;
                }

                const auto indent = contentView.find_first_not_of(L' ');
                const auto safeIndent = indent == std::wstring_view::npos ? 0u : static_cast<uint32_t>(indent);

                if (safeIndent == 0)
                {
                    appendWindow();

                    if (trimmed.starts_with(L"version:"))
                    {
                        section = ParseSection::None;
                        continue;
                    }
                    if (trimmed == L"windows:")
                    {
                        section = ParseSection::Windows;
                        continue;
                    }
                    section = ParseSection::None;
                    continue;
                }

                if (section != ParseSection::Windows)
                {
                    continue;
                }

                if (safeIndent == 2 && trimmed.starts_with(L"- "))
                {
                    appendWindow();
                    currentWindow.emplace();
                    const auto [key, value] = _parseKeyValue(trimmed);
                    if (!key.empty())
                    {
                        _applyWindowStateField(*currentWindow, key, value);
                    }
                    continue;
                }

                if (safeIndent >= 4 && currentWindow)
                {
                    const auto [key, value] = _parseKeyValue(trimmed);
                    if (!key.empty())
                    {
                        _applyWindowStateField(*currentWindow, key, value);
                    }
                }
            }

            appendWindow();
        }

        bool _removeLegacyWorkspaceDirectoriesIfPresent()
        {
            const auto legacyPath = terminal::workspacepaths::ResolveLegacyWorkspaceDefinitionsPath();
            const auto currentPath = terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
            if (legacyPath.empty() || legacyPath == currentPath)
            {
                return true;
            }

            std::error_code ec;
            if (!std::filesystem::exists(legacyPath, ec) || ec || !std::filesystem::is_directory(legacyPath, ec) || ec)
            {
                return !ec;
            }

            for (const auto& existingWorkspaceEntry : std::filesystem::directory_iterator(legacyPath, ec))
            {
                if (ec)
                {
                    return false;
                }
                if (!existingWorkspaceEntry.is_directory())
                {
                    continue;
                }
                if (!std::filesystem::exists(existingWorkspaceEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceMetadataFileName }, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                std::filesystem::remove_all(existingWorkspaceEntry.path(), ec);
                if (ec)
                {
                    return false;
                }
            }

            std::filesystem::remove(legacyPath / std::filesystem::path{ terminal::workspacepaths::LegacyWorkspaceFileName }, ec);
            return !ec;
        }

        bool _removeLegacyWorkspaceStateFilesIfPresent()
        {
            const auto legacyPath = terminal::workspacepaths::ResolveLegacyWorkspaceDefinitionsPath();
            const auto currentPath = terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
            if (legacyPath.empty() || legacyPath == currentPath)
            {
                return true;
            }

            std::error_code ec;
            if (!std::filesystem::exists(legacyPath, ec) || ec || !std::filesystem::is_directory(legacyPath, ec) || ec)
            {
                return !ec;
            }

            for (const auto& existingEntry : std::filesystem::directory_iterator(legacyPath, ec))
            {
                if (ec)
                {
                    return false;
                }
                if (!existingEntry.is_regular_file())
                {
                    continue;
                }

                const auto fileName = existingEntry.path().filename().wstring();
                if (fileName == terminal::workspacepaths::WorkspaceStateFileName ||
                    fileName.rfind(std::wstring{ terminal::workspacepaths::LegacyWorkspaceStateFilePrefix }, 0) == 0)
                {
                    std::filesystem::remove(existingEntry.path(), ec);
                    if (ec)
                    {
                        return false;
                    }
                }
            }

            return true;
        }
    }

    #include "WorkspaceCoreMultiWindow.cpp"
    #include "WorkspaceCoreManagerStorage.cpp"

    #include "deprecated/WorkspaceCaptureDeprecated.cpp"
    #include "WorkspaceCoreNavigation.cpp"

    #include "WorkspaceCoreEditor.cpp"

    #include "WorkspaceCoreRuntime.cpp"
}
