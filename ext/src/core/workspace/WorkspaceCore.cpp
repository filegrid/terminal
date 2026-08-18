// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "WorkspaceCore.h"

#include "WorkspacePersistencePaths.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

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
                    std::wstring input{ L"cd \"" };
                    input.append(startupDirectory);
                    input.append(L"\"\r");
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
                    return std::move(metadata->first);
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
            if (!workspace.TabOrder.empty())
            {
                std::wstring serializedOrder;
                for (const auto& nodeId : workspace.TabOrder)
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
            std::wostringstream stream;
            stream << L"version: 1\n";
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
            if (!node.StartupAction.empty())
            {
                if (_containsLineBreak(node.StartupAction))
                {
                    _writeMultilineValue(stream, L"", L"startupAction", node.StartupAction);
                }
                else
                {
                    stream << L"startupAction: " << _quote(node.StartupAction) << L"\n";
                }
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

    std::filesystem::path WorkspaceManager::DefaultPath()
    {
        return terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
    }

    WorkspaceManager WorkspaceManager::Load()
    {
        return LoadFromPath(DefaultPath());
    }

    WorkspaceManager WorkspaceManager::LoadFromPath(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
        {
            return {};
        }

        if (std::filesystem::is_regular_file(path, ec) && !ec)
        {
            return {};
        }

        WorkspaceManager manager;
        if (!std::filesystem::is_directory(path, ec) || ec)
        {
            return manager;
        }

        const auto persistedWorkspaces = _enumeratePersistedWorkspaceDirectories(path);
        if (!persistedWorkspaces.has_value())
        {
            return manager;
        }

        std::vector<Workspace> workspaces;
        workspaces.reserve(persistedWorkspaces->size());
        for (auto persistedWorkspace : *persistedWorkspaces)
        {
            auto workspace = std::move(persistedWorkspace.Definition);
            std::vector<WorkspaceNode> loadedNodes;
            for (const auto& nodeEntry : std::filesystem::directory_iterator(persistedWorkspace.Directory, ec))
            {
                if (ec)
                {
                    return {};
                }
                if (!nodeEntry.is_directory())
                {
                    continue;
                }

                const auto nodeFile = nodeEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceNodeMetadataFileName };
                if (!std::filesystem::exists(nodeFile, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                auto nodeMetadata = _loadWorkspaceNodeMetadataFile(nodeFile);
                if (!nodeMetadata.has_value())
                {
                    continue;
                }

                auto node = std::move(*nodeMetadata);
                node.Name = nodeEntry.path().filename().wstring();
                node.Id = node.Name;
                loadedNodes.emplace_back(std::move(node));
            }

            std::stable_sort(loadedNodes.begin(), loadedNodes.end(), [](const auto& lhs, const auto& rhs) {
                return _toLower(lhs.Name) < _toLower(rhs.Name);
            });
            workspace.Nodes.reserve(loadedNodes.size());
            for (auto& node : loadedNodes)
            {
                workspace.Nodes.emplace_back(std::move(node));
            }

            if (!workspace.Nodes.empty())
            {
                std::vector<WorkspaceNode> orderedVisibleNodes;
                for (const auto nodeIndex : _orderedVisibleWorkspaceNodeIndices(workspace))
                {
                    orderedVisibleNodes.emplace_back(workspace.Nodes.at(nodeIndex));
                }
                std::ignore = ApplyVisibleWorkspaceNodeOrder(workspace, orderedVisibleNodes);
            }
            workspaces.emplace_back(std::move(workspace));
        }

        manager.SetWorkspaces(std::move(workspaces));
        return manager;
    }

    bool WorkspaceManager::Save() const
    {
        return SaveToPath(DefaultPath());
    }

    bool WorkspaceManager::SaveToPath(const std::filesystem::path& path) const
    {
        if (path.has_extension())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec)
        {
            return false;
        }

        std::unordered_set<std::wstring> desiredWorkspaceDirs;
        for (size_t workspaceIndex = 0; workspaceIndex < _workspaces.size(); ++workspaceIndex)
        {
            const auto& workspace = _workspaces.at(workspaceIndex);
            const auto workspaceDirName = _makeUniqueDirectoryName(
                SanitizeWorkspaceDirectoryName(workspace.Name, L"workspace"),
                desiredWorkspaceDirs);
            const auto workspaceDir = path / workspaceDirName;
            std::filesystem::create_directories(workspaceDir, ec);
            if (ec)
            {
                return false;
            }

            if (!_writeUtf8TextFile(workspaceDir / std::filesystem::path{ terminal::workspacepaths::WorkspaceMetadataFileName },
                                    _serializeWorkspaceMetadata(workspace)))
            {
                return false;
            }

            std::unordered_set<std::wstring> desiredNodeDirs;
            for (size_t nodeIndex = 0; nodeIndex < workspace.Nodes.size(); ++nodeIndex)
            {
                const auto& node = workspace.Nodes.at(nodeIndex);
                const auto nodeDirName = _makeUniqueDirectoryName(
                    SanitizeWorkspaceDirectoryName(node.Name, L"tab"),
                    desiredNodeDirs);
                const auto nodeDir = workspaceDir / nodeDirName;
                std::filesystem::create_directories(nodeDir, ec);
                if (ec)
                {
                    return false;
                }

                if (!_writeUtf8TextFile(nodeDir / std::filesystem::path{ terminal::workspacepaths::WorkspaceNodeMetadataFileName },
                                        _serializeWorkspaceNodeMetadata(node)))
                {
                    return false;
                }
            }

            for (const auto& existingNodeEntry : std::filesystem::directory_iterator(workspaceDir, ec))
            {
                if (ec)
                {
                    return false;
                }
                if (!existingNodeEntry.is_directory())
                {
                    continue;
                }
                if (!std::filesystem::exists(existingNodeEntry.path() / std::filesystem::path{ terminal::workspacepaths::WorkspaceNodeMetadataFileName }, ec) || ec)
                {
                    ec.clear();
                    continue;
                }

                if (desiredNodeDirs.find(_toLower(existingNodeEntry.path().filename().wstring())) == desiredNodeDirs.end())
                {
                    std::filesystem::remove_all(existingNodeEntry.path(), ec);
                    if (ec)
                    {
                        return false;
                    }
                }
            }
        }

        for (const auto& existingWorkspaceEntry : std::filesystem::directory_iterator(path, ec))
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

            if (desiredWorkspaceDirs.find(_toLower(existingWorkspaceEntry.path().filename().wstring())) == desiredWorkspaceDirs.end())
            {
                std::filesystem::remove_all(existingWorkspaceEntry.path(), ec);
                if (ec)
                {
                    return false;
                }
            }
        }

        std::filesystem::remove(path / std::filesystem::path{ terminal::workspacepaths::LegacyWorkspaceFileName }, ec);
        if (ec)
        {
            return false;
        }

        if (!_writeUtf8TextFile(path / std::filesystem::path{ terminal::workspacepaths::WorkspaceOrderFileName },
                                _serializeWorkspaceOrder(_workspaces)))
        {
            return false;
        }

        if (path == terminal::workspacepaths::ResolveWorkspaceDefinitionsPath())
        {
            return _removeLegacyWorkspaceDirectoriesIfPresent();
        }

        return true;
    }

    const Workspace* WorkspaceManager::FindById(std::wstring_view id) const noexcept
    {
        const auto it = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == id;
        });
        return it == _workspaces.end() ? nullptr : &*it;
    }

    bool ApplyVisibleWorkspaceNodeOrder(Workspace& workspace, const std::vector<WorkspaceNode>& orderedVisibleNodes)
    {
        const auto visibleNodeCount = static_cast<size_t>(std::count_if(workspace.Nodes.begin(), workspace.Nodes.end(), [](const auto& node) {
            return node.ShowTab;
        }));
        if (orderedVisibleNodes.size() < visibleNodeCount)
        {
            return false;
        }

        std::vector<std::wstring_view> seenNodeIds;
        seenNodeIds.reserve(orderedVisibleNodes.size());
        for (const auto& node : orderedVisibleNodes)
        {
            if (node.Id.empty())
            {
                return false;
            }

            if (std::find(seenNodeIds.begin(), seenNodeIds.end(), std::wstring_view{ node.Id }) != seenNodeIds.end())
            {
                return false;
            }

            seenNodeIds.emplace_back(node.Id);
        }

        std::vector<WorkspaceNode> reorderedNodes;
        reorderedNodes.reserve(workspace.Nodes.size() + orderedVisibleNodes.size() - visibleNodeCount);

        size_t visibleNodeCursor = 0;
        for (const auto& existingNode : workspace.Nodes)
        {
            if (!existingNode.ShowTab)
            {
                reorderedNodes.emplace_back(existingNode);
                continue;
            }

            reorderedNodes.emplace_back(orderedVisibleNodes.at(visibleNodeCursor++));
        }

        while (visibleNodeCursor < orderedVisibleNodes.size())
        {
            reorderedNodes.emplace_back(orderedVisibleNodes.at(visibleNodeCursor++));
        }

        workspace.Nodes = std::move(reorderedNodes);
        return true;
    }

    std::optional<Workspace> PrepareWorkspaceForCapture(const std::optional<Workspace>& currentWorkspaceDefinition,
                                                        std::vector<WorkspaceNode> capturedNodes)
    {
        const auto capturedTabOrder = _captureVisibleWorkspaceNodeOrder(capturedNodes);
        Workspace workspace;
        if (currentWorkspaceDefinition.has_value())
        {
            workspace = *currentWorkspaceDefinition;

            std::vector<WorkspaceNode> mergedNodes;
            mergedNodes.reserve(workspace.Nodes.size() + capturedNodes.size());
            std::vector<bool> consumedCapturedNodes(capturedNodes.size(), false);

            for (const auto& existingNode : workspace.Nodes)
            {
                if (!WorkspaceNodeLoadsTab(existingNode))
                {
                    mergedNodes.emplace_back(existingNode);
                    continue;
                }

                if (!existingNode.Id.empty())
                {
                    auto matched = false;
                    for (size_t capturedIndex = 0; capturedIndex < capturedNodes.size(); ++capturedIndex)
                    {
                        const auto& capturedNode = capturedNodes.at(capturedIndex);
                        if (!consumedCapturedNodes.at(capturedIndex) && capturedNode.Id == existingNode.Id)
                        {
                            mergedNodes.emplace_back(capturedNode);
                            consumedCapturedNodes.at(capturedIndex) = true;
                            matched = true;
                            break;
                        }
                    }

                    if (matched)
                    {
                        continue;
                    }
                }

                // Visible nodes missing from the live capture were removed from the workspace.
            }

            for (size_t capturedIndex = 0; capturedIndex < capturedNodes.size(); ++capturedIndex)
            {
                if (!consumedCapturedNodes.at(capturedIndex))
                {
                    mergedNodes.emplace_back(std::move(capturedNodes.at(capturedIndex)));
                }
            }

            workspace.Nodes = std::move(mergedNodes);
        }
        else
        {
            workspace.Nodes = std::move(capturedNodes);
        }

        workspace.TabOrder = capturedTabOrder;

        if (workspace.Nodes.empty())
        {
            return std::nullopt;
        }

        return workspace;
    }

    std::optional<Workspace> ResolveWorkspaceDefinition(const std::wstring_view currentWorkspaceId,
                                                        const std::optional<Workspace>& selectedWorkspace,
                                                        const WorkspaceManager& manager)
    {
        if (currentWorkspaceId.empty())
        {
            return std::nullopt;
        }

        if (selectedWorkspace && selectedWorkspace->Id == currentWorkspaceId)
        {
            return selectedWorkspace;
        }

        if (const auto workspace = manager.FindById(currentWorkspaceId))
        {
            return *workspace;
        }

        return std::nullopt;
    }

    bool WorkspaceNodeLoadsTab(const WorkspaceNode& node) noexcept
    {
        return node.ShowTab;
    }

    namespace
    {
        std::vector<size_t> _orderedVisibleWorkspaceNodeIndices(const Workspace& workspace)
        {
            std::vector<size_t> orderedIndices;
            orderedIndices.reserve(workspace.Nodes.size());

            std::vector<bool> consumed(workspace.Nodes.size(), false);
            for (const auto& nodeId : workspace.TabOrder)
            {
                if (nodeId.empty())
                {
                    continue;
                }

                for (size_t index = 0; index < workspace.Nodes.size(); ++index)
                {
                    const auto& node = workspace.Nodes.at(index);
                    if (consumed[index] || !WorkspaceNodeLoadsTab(node) || node.Id != nodeId)
                    {
                        continue;
                    }

                    orderedIndices.emplace_back(index);
                    consumed[index] = true;
                    break;
                }
            }

            for (size_t index = 0; index < workspace.Nodes.size(); ++index)
            {
                if (!consumed[index] && WorkspaceNodeLoadsTab(workspace.Nodes.at(index)))
                {
                    orderedIndices.emplace_back(index);
                }
            }

            return orderedIndices;
        }

        std::vector<std::wstring> _captureVisibleWorkspaceNodeOrder(const std::vector<WorkspaceNode>& nodes)
        {
            std::vector<std::wstring> orderedIds;
            orderedIds.reserve(nodes.size());
            for (const auto& node : nodes)
            {
                if (WorkspaceNodeLoadsTab(node) && !node.Id.empty())
                {
                    orderedIds.emplace_back(node.Id);
                }
            }
            return orderedIds;
        }
    }

    std::optional<WorkspaceNode> FindWorkspaceNodeById(const Workspace& workspace, const std::wstring_view nodeId)
    {
        if (const auto index = FindWorkspaceNodeIndexById(workspace, nodeId))
        {
            return workspace.Nodes.at(*index);
        }
        return std::nullopt;
    }

    std::optional<WorkspaceNode> FindWorkspaceNodeById(const WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view nodeId)
    {
        if (const auto workspace = manager.FindById(workspaceId))
        {
            return FindWorkspaceNodeById(*workspace, nodeId);
        }
        return std::nullopt;
    }

    std::optional<WorkspaceNode> ResolveCurrentWorkspaceNode(const std::wstring_view currentWorkspaceId,
                                                             const std::optional<Workspace>& selectedWorkspace,
                                                             const WorkspaceManager& manager,
                                                             const std::wstring_view nodeId)
    {
        if (nodeId.empty() || currentWorkspaceId.empty())
        {
            return std::nullopt;
        }

        if (selectedWorkspace && selectedWorkspace->Id == currentWorkspaceId)
        {
            if (const auto node = FindWorkspaceNodeById(*selectedWorkspace, nodeId))
            {
                return node;
            }
        }

        return FindWorkspaceNodeById(manager, currentWorkspaceId, nodeId);
    }

    std::optional<size_t> FindWorkspaceNodeIndexById(const Workspace& workspace, const std::wstring_view nodeId)
    {
        if (nodeId.empty())
        {
            return std::nullopt;
        }

        for (size_t i = 0; i < workspace.Nodes.size(); ++i)
        {
            if (workspace.Nodes.at(i).Id == nodeId)
            {
                return i;
            }
        }

        return std::nullopt;
    }

    std::optional<size_t> FindWorkspaceVisibleNodeIndex(const Workspace& workspace, const size_t visibleOrdinal)
    {
        const auto orderedIndices = _orderedVisibleWorkspaceNodeIndices(workspace);
        return visibleOrdinal < orderedIndices.size() ? std::optional<size_t>{ orderedIndices.at(visibleOrdinal) } : std::nullopt;
    }

    std::optional<size_t> ResolveWorkspaceBackedNodeIndex(const std::optional<Workspace>& workspaceDefinition,
                                                          const std::wstring_view runtimeNodeId,
                                                          const std::optional<size_t> visibleOrdinal)
    {
        if (!workspaceDefinition.has_value())
        {
            return visibleOrdinal;
        }

        if (const auto nodeIndex = FindWorkspaceNodeIndexById(*workspaceDefinition, runtimeNodeId))
        {
            return nodeIndex;
        }

        if (visibleOrdinal.has_value())
        {
            return FindWorkspaceVisibleNodeIndex(*workspaceDefinition, *visibleOrdinal);
        }

        return std::nullopt;
    }

    std::optional<WorkspaceNode> ResolveWorkspaceBackedNode(const std::optional<Workspace>& workspaceDefinition,
                                                            const std::wstring_view runtimeNodeId,
                                                            const std::optional<size_t> visibleOrdinal)
    {
        if (!workspaceDefinition.has_value())
        {
            return std::nullopt;
        }

        if (const auto nodeIndex = ResolveWorkspaceBackedNodeIndex(workspaceDefinition, runtimeNodeId, visibleOrdinal);
            nodeIndex.has_value() && *nodeIndex < workspaceDefinition->Nodes.size())
        {
            return workspaceDefinition->Nodes.at(*nodeIndex);
        }

        return std::nullopt;
    }

    namespace
    {
        std::optional<size_t> _resolveWorkspaceLiveTabVisibleOrdinal(const std::vector<WorkspaceLiveTabSnapshot>& tabs, const size_t targetTabIndex)
        {
            if (targetTabIndex >= tabs.size() || !tabs.at(targetTabIndex).LoadsWorkspaceNode)
            {
                return std::nullopt;
            }

            size_t visibleOrdinal = 0;
            for (size_t index = 0; index < targetTabIndex; ++index)
            {
                if (tabs.at(index).LoadsWorkspaceNode)
                {
                    ++visibleOrdinal;
                }
            }
            return visibleOrdinal;
        }
    }

    std::optional<size_t> ResolveWorkspaceBackedTabIndex(const std::optional<Workspace>& workspaceDefinition,
                                                         const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                         const size_t targetTabIndex)
    {
        if (targetTabIndex >= tabs.size())
        {
            return std::nullopt;
        }

        return ResolveWorkspaceBackedNodeIndex(workspaceDefinition,
                                               tabs.at(targetTabIndex).RuntimeNodeId,
                                               _resolveWorkspaceLiveTabVisibleOrdinal(tabs, targetTabIndex));
    }

    std::optional<WorkspaceNode> ResolveWorkspaceBackedTabNode(const std::optional<Workspace>& workspaceDefinition,
                                                               const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                               const size_t targetTabIndex)
    {
        if (targetTabIndex >= tabs.size())
        {
            return std::nullopt;
        }

        return ResolveWorkspaceBackedNode(workspaceDefinition,
                                          tabs.at(targetTabIndex).RuntimeNodeId,
                                          _resolveWorkspaceLiveTabVisibleOrdinal(tabs, targetTabIndex));
    }

    std::optional<size_t> FindWorkspaceBackedTabSnapshotIndex(const std::optional<Workspace>& workspaceDefinition,
                                                              const std::vector<WorkspaceLiveTabSnapshot>& tabs,
                                                              const size_t nodeIndex)
    {
        for (size_t tabIndex = 0; tabIndex < tabs.size(); ++tabIndex)
        {
            if (const auto currentNodeIndex = ResolveWorkspaceBackedTabIndex(workspaceDefinition, tabs, tabIndex);
                currentNodeIndex.has_value() && currentNodeIndex.value() == nodeIndex)
            {
                return tabIndex;
            }
        }

        return std::nullopt;
    }

    std::vector<std::wstring> VisibleWorkspaceNodeIds(const Workspace& workspace)
    {
        std::vector<std::wstring> values;
        for (const auto nodeIndex : _orderedVisibleWorkspaceNodeIndices(workspace))
        {
            values.emplace_back(workspace.Nodes.at(nodeIndex).Id);
        }
        return values;
    }

    std::vector<bool> VisibleWorkspaceNodeInputVisibility(const Workspace& workspace)
    {
        std::vector<bool> values;
        for (const auto nodeIndex : _orderedVisibleWorkspaceNodeIndices(workspace))
        {
            values.emplace_back(workspace.Nodes.at(nodeIndex).ShowInputPanel);
        }
        return values;
    }

    int32_t WorkspaceManagerNavSelectionForWorkspace(const size_t workspaceIndex) noexcept
    {
        return _workspaceManagerWorkspaceSelectionBase + gsl::narrow_cast<int32_t>(workspaceIndex * _workspaceManagerWorkspaceSelectionStride);
    }

    int32_t WorkspaceManagerNavSelectionForWorkspaceNode(const size_t workspaceIndex, const size_t nodeIndex) noexcept
    {
        return WorkspaceManagerNavSelectionForWorkspace(workspaceIndex) + _workspaceManagerNodeSelectionBase + gsl::narrow_cast<int32_t>(nodeIndex);
    }

    std::optional<size_t> ResolveWorkspaceIndexFromManagerNavSelection(const int32_t navSelection) noexcept
    {
        if (navSelection < _workspaceManagerWorkspaceSelectionBase)
        {
            return std::nullopt;
        }

        return gsl::narrow_cast<size_t>((navSelection - _workspaceManagerWorkspaceSelectionBase) / _workspaceManagerWorkspaceSelectionStride);
    }

    std::optional<size_t> ResolveWorkspaceNodeIndexFromManagerNavSelection(const int32_t navSelection) noexcept
    {
        if (navSelection < _workspaceManagerWorkspaceSelectionBase)
        {
            return std::nullopt;
        }

        const auto subSelection = (navSelection - _workspaceManagerWorkspaceSelectionBase) % _workspaceManagerWorkspaceSelectionStride;
        if (subSelection < _workspaceManagerNodeSelectionBase)
        {
            return std::nullopt;
        }

        return gsl::narrow_cast<size_t>(subSelection - _workspaceManagerNodeSelectionBase);
    }

    int32_t ResolveWorkspaceManagerNavSelectionForEditor(const size_t workspaceCount, const size_t selectedWorkspaceIndex) noexcept
    {
        if (workspaceCount == 0)
        {
            return 0;
        }

        return WorkspaceManagerNavSelectionForWorkspace(std::min(selectedWorkspaceIndex, workspaceCount - 1));
    }

    int32_t ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(const int32_t previousNavSelection,
                                                                     const std::wstring_view selectedWorkspaceId,
                                                                     const std::wstring_view removedWorkspaceId,
                                                                     const size_t removedWorkspaceIndex,
                                                                     const size_t remainingWorkspaceCount) noexcept
    {
        const auto previousWorkspaceIndex = ResolveWorkspaceIndexFromManagerNavSelection(previousNavSelection);
        if (!previousWorkspaceIndex.has_value())
        {
            return previousNavSelection;
        }

        if (remainingWorkspaceCount == 0)
        {
            return 0;
        }

        if (selectedWorkspaceId == removedWorkspaceId)
        {
            return WorkspaceManagerNavSelectionForWorkspace(std::min(removedWorkspaceIndex, remainingWorkspaceCount - 1));
        }

        if (*previousWorkspaceIndex > removedWorkspaceIndex)
        {
            return previousNavSelection - _workspaceManagerWorkspaceSelectionStride;
        }

        return previousNavSelection;
    }

    int32_t ResolveWorkspaceManagerNavSelectionAfterNodeRemoval(const int32_t previousNavSelection,
                                                                const std::wstring_view selectedWorkspaceId,
                                                                const std::wstring_view workspaceId,
                                                                const size_t selectedWorkspaceIndex,
                                                                const size_t removedNodeIndex) noexcept
    {
        if (selectedWorkspaceId != workspaceId)
        {
            return previousNavSelection;
        }

        const auto selectedWorkspace = ResolveWorkspaceIndexFromManagerNavSelection(previousNavSelection);
        const auto selectedNode = ResolveWorkspaceNodeIndexFromManagerNavSelection(previousNavSelection);
        if (!selectedWorkspace.has_value() || !selectedNode.has_value() || *selectedWorkspace != selectedWorkspaceIndex)
        {
            return previousNavSelection;
        }

        if (*selectedNode == removedNodeIndex)
        {
            if (removedNodeIndex > 0)
            {
                return WorkspaceManagerNavSelectionForWorkspaceNode(selectedWorkspaceIndex, removedNodeIndex - 1);
            }

            return WorkspaceManagerNavSelectionForWorkspace(selectedWorkspaceIndex);
        }

        if (*selectedNode > removedNodeIndex)
        {
            return previousNavSelection - 1;
        }

        return previousNavSelection;
    }

    size_t ResolveWorkspaceEditorSelectedIndex(const WorkspaceManager& manager,
                                               const std::wstring_view selectedWorkspaceId,
                                               const std::wstring_view currentWorkspaceId,
                                               const size_t fallbackIndex) noexcept
    {
        const auto& workspaces = manager.Workspaces();
        if (workspaces.empty())
        {
            return 0;
        }

        if (!selectedWorkspaceId.empty())
        {
            if (const auto it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
                    return workspace.Id == selectedWorkspaceId;
                });
                it != workspaces.end())
            {
                return static_cast<size_t>(std::distance(workspaces.begin(), it));
            }
        }

        if (!currentWorkspaceId.empty())
        {
            if (const auto it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
                    return workspace.Id == currentWorkspaceId;
                });
                it != workspaces.end())
            {
                return static_cast<size_t>(std::distance(workspaces.begin(), it));
            }
        }

        return std::min(fallbackIndex, workspaces.size() - 1);
    }

    WorkspaceEditorLoadState LoadWorkspaceEditorState(const std::wstring_view selectedWorkspaceId,
                                                      const std::wstring_view currentWorkspaceId,
                                                      const size_t fallbackIndex)
    {
        auto manager = WorkspaceManager::Load();
        return WorkspaceEditorLoadState{
            .Manager = manager,
            .SelectedWorkspaceIndex = ResolveWorkspaceEditorSelectedIndex(manager, selectedWorkspaceId, currentWorkspaceId, fallbackIndex),
        };
    }

    WorkspaceEditorSavePlan PrepareWorkspaceEditorForSave(WorkspaceManager& editedManager,
                                                          const WorkspaceManager& persistedManager,
                                                          const std::wstring_view currentWorkspaceId,
                                                          const std::wstring_view lastOpenedWorkspaceId,
                                                          const size_t fallbackSelectedWorkspaceIndex)
    {
        WorkspaceEditorSavePlan plan;

        std::optional<size_t> currentWorkspaceIndex;
        if (!currentWorkspaceId.empty())
        {
            const auto& persistedWorkspaces = persistedManager.Workspaces();
            if (const auto currentIt = std::find_if(persistedWorkspaces.begin(), persistedWorkspaces.end(), [&](const auto& workspace) {
                    return workspace.Id == currentWorkspaceId;
                });
                currentIt != persistedWorkspaces.end())
            {
                currentWorkspaceIndex = gsl::narrow_cast<size_t>(std::distance(persistedWorkspaces.begin(), currentIt));
            }
        }

        FinalizeWorkspaceManagerNames(editedManager);

        if (currentWorkspaceIndex.has_value() && *currentWorkspaceIndex < editedManager.Workspaces().size())
        {
            plan.CurrentWorkspaceExists = true;
            plan.ResolvedCurrentWorkspaceId = editedManager.Workspaces().at(*currentWorkspaceIndex).Id;
        }
        else
        {
            plan.CurrentWorkspaceExists = currentWorkspaceId.empty();
        }

        plan.LastOpenedWorkspaceExists = lastOpenedWorkspaceId.empty() || editedManager.FindById(lastOpenedWorkspaceId) != nullptr;
        if (!editedManager.Workspaces().empty())
        {
            plan.SelectedWorkspaceIndex = std::min(fallbackSelectedWorkspaceIndex, editedManager.Workspaces().size() - 1);
        }
        return plan;
    }

    std::optional<WorkspaceEditorDefinitionRemovalPlan> PrepareWorkspaceDefinitionRemoval(WorkspaceManager& manager,
                                                                                          const std::wstring_view workspaceId,
                                                                                          const std::wstring_view selectedWorkspaceId,
                                                                                          const std::wstring_view currentWorkspaceId,
                                                                                          const size_t fallbackSelectedWorkspaceIndex,
                                                                                          const int32_t previousNavSelection,
                                                                                          const std::wstring_view lastOpenedWorkspaceId)
    {
        size_t removedWorkspaceIndex = 0;
        if (!RemoveWorkspaceDefinition(manager, workspaceId, &removedWorkspaceIndex))
        {
            return std::nullopt;
        }

        WorkspaceEditorDefinitionRemovalPlan plan;
        plan.RemovedCurrentWorkspace = workspaceId == currentWorkspaceId;
        plan.LastOpenedWorkspaceExists = lastOpenedWorkspaceId.empty() || manager.FindById(lastOpenedWorkspaceId) != nullptr;
        plan.NavSelection = ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(previousNavSelection,
                                                                                    selectedWorkspaceId,
                                                                                    workspaceId,
                                                                                    removedWorkspaceIndex,
                                                                                    manager.Workspaces().size());
        if (!manager.Workspaces().empty())
        {
            plan.SelectedWorkspaceIndex = ResolveWorkspaceEditorSelectedIndex(manager,
                                                                              selectedWorkspaceId,
                                                                              currentWorkspaceId,
                                                                              fallbackSelectedWorkspaceIndex);
        }

        return plan;
    }

    WorkspaceEditorNodeRemovalPlan PrepareWorkspaceNodeRemoval(WorkspaceManager& manager,
                                                               const std::wstring_view workspaceId,
                                                               const std::wstring_view nodeId,
                                                               const std::wstring_view selectedWorkspaceId,
                                                               const std::wstring_view currentWorkspaceId,
                                                               const size_t fallbackSelectedWorkspaceIndex,
                                                               const int32_t previousNavSelection,
                                                               const std::wstring_view lastOpenedWorkspaceId)
    {
        const auto mutation = RemoveWorkspaceNode(manager, workspaceId, nodeId);

        WorkspaceEditorNodeRemovalPlan plan;
        plan.Disposition = mutation.Disposition;
        if (mutation.Disposition == WorkspaceNodeMutationDisposition::NotFound)
        {
            return plan;
        }

        plan.RemovedCurrentWorkspace = workspaceId == currentWorkspaceId;
        plan.LastOpenedWorkspaceExists = lastOpenedWorkspaceId.empty() || manager.FindById(lastOpenedWorkspaceId) != nullptr;
        if (!manager.Workspaces().empty())
        {
            plan.SelectedWorkspaceIndex = ResolveWorkspaceEditorSelectedIndex(manager,
                                                                              selectedWorkspaceId,
                                                                              currentWorkspaceId,
                                                                              fallbackSelectedWorkspaceIndex);
        }

        if (mutation.Disposition == WorkspaceNodeMutationDisposition::RemovedWorkspace)
        {
            plan.NavSelection = ResolveWorkspaceManagerNavSelectionAfterWorkspaceRemoval(previousNavSelection,
                                                                                        selectedWorkspaceId,
                                                                                        workspaceId,
                                                                                        mutation.WorkspaceIndex,
                                                                                        manager.Workspaces().size());
        }
        else if (previousNavSelection >= _workspaceManagerWorkspaceSelectionBase)
        {
            plan.NavSelection = ResolveWorkspaceManagerNavSelectionAfterNodeRemoval(previousNavSelection,
                                                                                   selectedWorkspaceId,
                                                                                   workspaceId,
                                                                                   plan.SelectedWorkspaceIndex,
                                                                                   mutation.NodeIndex);
        }
        else
        {
            plan.NavSelection = previousNavSelection;
        }

        return plan;
    }

    WorkspaceCurrentState ResolveWorkspaceCurrentState(const std::wstring_view currentWorkspaceId,
                                                       const WorkspaceManager& manager,
                                                       const std::wstring_view defaultDisplayName,
                                                       const std::wstring_view unsavedTabRowName)
    {
        WorkspaceCurrentState state;
        if (currentWorkspaceId.empty())
        {
            state.DisplayName = std::wstring{ defaultDisplayName };
            state.TabRowName = std::wstring{ unsavedTabRowName };
            return state;
        }

        state.DisplayName = std::wstring{ currentWorkspaceId };
        state.TabRowName = state.DisplayName;
        if (const auto workspace = manager.FindById(currentWorkspaceId))
        {
            state.Exists = true;
            state.DisplayName = workspace->Name;
            state.TabRowName = workspace->Name;
            state.BackgroundColor = workspace->BackgroundColor;
            state.Locked = workspace->Locked;
        }

        return state;
    }

    WorkspaceStartupState PrepareWorkspaceStartupState(const std::wstring_view workspaceId, const WorkspaceManager& manager)
    {
        WorkspaceStartupState state;
        if (const auto workspace = manager.FindById(workspaceId))
        {
            state.PendingNodeIds = VisibleWorkspaceNodeIds(*workspace);
            state.PendingNodeInputVisibility = VisibleWorkspaceNodeInputVisibility(*workspace);
        }
        return state;
    }

    WorkspaceFlyoutState BuildWorkspaceFlyoutState(const std::wstring_view currentWorkspaceId,
                                                   const WorkspaceManager& manager,
                                                   const WorkspaceStateManager& stateManager)
    {
        WorkspaceFlyoutState state;
        state.Entries.reserve(manager.Workspaces().size());
        for (const auto& workspace : manager.Workspaces())
        {
            state.Entries.emplace_back(WorkspaceFlyoutEntry{
                .Definition = workspace,
                .IsOpen = stateManager.FindOpenWorkspaceWindowId(workspace.Id).has_value(),
            });
        }
        state.CurrentWorkspaceExists = currentWorkspaceId.empty() || manager.FindById(currentWorkspaceId) != nullptr;
        return state;
    }

    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromProfile(const std::wstring_view source)
    {
        WorkspaceRuntimeMetadata metadata;
        const auto loweredSource = _toLower(source);
        if (loweredSource == L"windows.terminal.ssh")
        {
            metadata.ShellType = L"ssh";
            return metadata;
        }

        if (loweredSource == L"windows.terminal.powershellcore")
        {
            metadata.ShellType = L"powershell";
            metadata.OperatingSystem = L"windows";
            return metadata;
        }

        if (_isWslProfileSource(loweredSource))
        {
            metadata.OperatingSystem = L"linux";
            metadata.ShellType = L"wsl";
            return metadata;
        }

        return metadata;
    }

    WorkspaceRuntimeMetadata InferWorkspaceRuntimeMetadataFromCommandline(const std::wstring_view value)
    {
        WorkspaceRuntimeMetadata metadata;
        if (value.empty())
        {
            return metadata;
        }

        const auto lowered = _toLower(value);
        if (lowered.find(L"ssh.exe") != std::wstring::npos ||
            lowered == L"ssh" ||
            lowered.starts_with(L"ssh "))
        {
            metadata.ShellType = L"ssh";
            return metadata;
        }

        if (lowered.find(L"powershell.exe") != std::wstring::npos ||
            lowered.find(L"pwsh.exe") != std::wstring::npos ||
            lowered == L"powershell" ||
            lowered == L"pwsh" ||
            lowered.starts_with(L"powershell ") ||
            lowered.starts_with(L"pwsh "))
        {
            metadata.ShellType = L"powershell";
            metadata.OperatingSystem = L"windows";
            return metadata;
        }

        if (lowered.find(L"cmd.exe") != std::wstring::npos ||
            lowered == L"cmd" ||
            lowered.starts_with(L"cmd "))
        {
            metadata.ShellType = L"cmd";
            metadata.OperatingSystem = L"windows";
            return metadata;
        }

        if (_isWslCommandline(lowered))
        {
            metadata.OperatingSystem = L"linux";
            metadata.ShellType = L"wsl";
            return metadata;
        }

        if (lowered.find(L"/bin/bash") != std::wstring::npos ||
            lowered.find(L"/bin/sh") != std::wstring::npos ||
            lowered.find(L"/bin/zsh") != std::wstring::npos ||
            lowered.find(L"/bin/fish") != std::wstring::npos)
        {
            metadata.OperatingSystem = L"linux";
        }

        return metadata;
    }

    bool IsWorkspaceSshCommandline(const std::wstring_view value)
    {
        const auto lowered = _toLower(value);
        return lowered.find(L"ssh.exe") != std::wstring::npos ||
               lowered == L"ssh" ||
               lowered.starts_with(L"ssh ");
    }

    bool HasWorkspaceSshTtyOption(const std::wstring_view commandline)
    {
        if (commandline.empty())
        {
            return false;
        }

        const auto argv = _splitCommandlineArguments(commandline);
        for (size_t index = 1; index < argv.size(); ++index)
        {
            const auto arg = _toLower(_trim(argv[index]));
            if (arg == L"-t" || arg == L"-tt")
            {
                return true;
            }
        }

        return false;
    }

    bool IsWorkspaceSshTransport(const std::wstring_view profileSource,
                                 const std::wstring_view profileCommandline,
                                 const std::wstring_view commandline)
    {
        if (IsWorkspaceSshCommandline(commandline))
        {
            return true;
        }

        const auto loweredSource = _toLower(profileSource);
        return loweredSource == L"windows.terminal.ssh" || IsWorkspaceSshCommandline(profileCommandline);
    }

    WorkspaceRuntimeLaunchState PrepareWorkspaceRuntimeLaunchState(const std::wstring_view /*startingDirectory*/,
                                                                   const std::wstring_view profileSource,
                                                                   const std::wstring_view profileCommandline,
                                                                   const std::wstring_view commandline)
    {
        WorkspaceRuntimeLaunchState state;
        state.IsSshTransport = IsWorkspaceSshTransport(profileSource, profileCommandline, commandline);
        state.HasSshTtyOption = HasWorkspaceSshTtyOption(commandline);
        state.StartingDirectory.clear();
        if (!commandline.empty() && commandline != profileCommandline)
        {
            state.ExplicitCommandline = std::wstring{ commandline };
        }

        auto metadata = InferWorkspaceRuntimeMetadataFromCommandline(commandline.empty() ? profileCommandline : commandline);
        if (metadata.OperatingSystem.empty() || metadata.ShellType.empty())
        {
            const auto profileMetadata = InferWorkspaceRuntimeMetadataFromProfile(profileSource);
            if (metadata.OperatingSystem.empty())
            {
                metadata.OperatingSystem = profileMetadata.OperatingSystem;
            }
            if (metadata.ShellType.empty())
            {
                metadata.ShellType = profileMetadata.ShellType;
            }
        }

        state.OperatingSystem = metadata.OperatingSystem.empty() ? L"linux" : metadata.OperatingSystem;
        state.ShellType = std::move(metadata.ShellType);
        return state;
    }

    WorkspaceNodeLaunchResolution ResolveWorkspaceNodeLaunchResolution(const WorkspaceNodeLaunchResolutionInput& input)
    {
        WorkspaceNodeLaunchResolution resolution;

        if (!input.ObservedStartupAction.empty())
        {
            resolution.StartupAction = input.ObservedStartupAction;
        }
        else if (!input.RuntimeStartupAction.empty())
        {
            resolution.StartupAction = input.RuntimeStartupAction;
        }
        else if (!input.RuntimeExplicitCommandline.empty())
        {
            resolution.StartupAction = input.RuntimeExplicitCommandline;
        }
        else if (input.PersistedNode.has_value())
        {
            resolution.StartupAction = input.PersistedNode->StartupAction;
        }
        else if (!input.TerminalCommandline.empty() && input.TerminalCommandline != input.ProfileCommandline)
        {
            resolution.StartupAction = input.TerminalCommandline;
        }

        if (!input.ObservedWorkingDirectory.empty())
        {
            resolution.StartingDirectory = input.ObservedWorkingDirectory;
        }
        else if (!input.RuntimeStartingDirectory.empty())
        {
            resolution.StartingDirectory = input.RuntimeStartingDirectory;
        }
        else if (input.PersistedNode.has_value())
        {
            resolution.StartingDirectory = input.PersistedNode->StartupDirectory;
        }
        const auto profileMetadata = InferWorkspaceRuntimeMetadataFromProfile(input.ProfileSource);
        if (profileMetadata.ShellType == L"wsl")
        {
            resolution.OperatingSystem = L"linux";
            resolution.ShellType = L"wsl";
            return resolution;
        }

        if (!input.ObservedOperatingSystem.empty())
        {
            resolution.OperatingSystem = input.ObservedOperatingSystem;
        }
        else if (!input.RuntimeOperatingSystem.empty())
        {
            resolution.OperatingSystem = input.RuntimeOperatingSystem;
        }
        else
        {
            auto metadata = InferWorkspaceRuntimeMetadataFromCommandline(input.TerminalCommandline.empty() ? input.ProfileCommandline : input.TerminalCommandline);
            if (metadata.OperatingSystem.empty())
            {
                metadata = profileMetadata;
            }
            if (!metadata.OperatingSystem.empty())
            {
                resolution.OperatingSystem = metadata.OperatingSystem;
            }
            else if (input.PersistedNode.has_value())
            {
                resolution.OperatingSystem = input.PersistedNode->OperatingSystem;
            }
            else
            {
                resolution.OperatingSystem = L"linux";
            }
        }

        if (!input.ObservedShellType.empty())
        {
            resolution.ShellType = input.ObservedShellType;
        }
        else if (!input.RuntimeShellType.empty())
        {
            resolution.ShellType = input.RuntimeShellType;
        }
        else
        {
            auto metadata = InferWorkspaceRuntimeMetadataFromCommandline(input.TerminalCommandline.empty() ? input.ProfileCommandline : input.TerminalCommandline);
            if (metadata.ShellType.empty())
            {
                metadata = profileMetadata;
            }
            if (!metadata.ShellType.empty())
            {
                resolution.ShellType = metadata.ShellType;
            }
            else if (input.PersistedNode.has_value())
            {
                resolution.ShellType = input.PersistedNode->ShellType;
            }
        }

        return resolution;
    }

    std::wstring ResolveTrackedWorkspaceDirectory(const WorkspaceTrackedDirectoryInput& input)
    {
        const auto reportedWorkingDirectory = std::wstring{ _trim(input.ReportedWorkingDirectory) };
        if (input.IsSshTransport ||
            _toLower(input.RuntimeShellType) == L"ssh" ||
            _toLower(input.RuntimeOperatingSystem) == L"linux")
        {
            return reportedWorkingDirectory;
        }

        const auto processWorkingDirectory = std::wstring{ _trim(input.ProcessWorkingDirectory) };
        if (!processWorkingDirectory.empty())
        {
            return processWorkingDirectory;
        }

        if (!reportedWorkingDirectory.empty())
        {
            return reportedWorkingDirectory;
        }

        return std::wstring{ _trim(input.RuntimeStartingDirectory) };
    }

    bool IsWorkspaceDirty(const Workspace& capturedWorkspace,
                          const std::wstring_view currentWorkspaceId,
                          const std::optional<Workspace>& baselineWorkspace,
                          const std::optional<Workspace>& persistedWorkspace)
    {
        if (currentWorkspaceId.empty())
        {
            return true;
        }

        if (baselineWorkspace.has_value())
        {
            return !WorkspaceLayoutEquivalent(*baselineWorkspace, capturedWorkspace);
        }

        if (persistedWorkspace.has_value())
        {
            return !WorkspaceLayoutEquivalent(*persistedWorkspace, capturedWorkspace);
        }

        return true;
    }

    bool WorkspaceManager::ReorderWorkspaceNodes(std::wstring_view workspaceId, const std::vector<std::wstring>& orderedNodeIds)
    {
        const auto workspaceIt = std::find_if(_workspaces.begin(), _workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == _workspaces.end())
        {
            return false;
        }

        auto& workspace = *workspaceIt;
        const auto visibleNodeCount = static_cast<size_t>(std::count_if(workspace.Nodes.begin(), workspace.Nodes.end(), [](const auto& node) {
            return node.ShowTab;
        }));
        if (orderedNodeIds.size() != visibleNodeCount)
        {
            return false;
        }

        std::vector<WorkspaceNode> orderedVisibleNodes;
        orderedVisibleNodes.reserve(orderedNodeIds.size());
        std::vector<std::wstring_view> consumedNodeIds;
        consumedNodeIds.reserve(orderedNodeIds.size());
        for (const auto& nodeId : orderedNodeIds)
        {
            if (std::find(consumedNodeIds.begin(), consumedNodeIds.end(), std::wstring_view{ nodeId }) != consumedNodeIds.end())
            {
                return false;
            }

            const auto nodeIt = std::find_if(workspace.Nodes.begin(), workspace.Nodes.end(), [&](const auto& node) {
                return node.Id == nodeId && node.ShowTab;
            });
            if (nodeIt == workspace.Nodes.end())
            {
                return false;
            }

            orderedVisibleNodes.emplace_back(*nodeIt);
            consumedNodeIds.emplace_back(orderedVisibleNodes.back().Id);
        }

        workspace.TabOrder.assign(orderedNodeIds.begin(), orderedNodeIds.end());
        // Keep the persisted node list aligned with the tab order as well.
        // Hidden nodes retain their relative positions; only visible nodes move.
        size_t visibleCursor = 0;
        for (auto& node : workspace.Nodes)
        {
            if (node.ShowTab)
            {
                node = orderedVisibleNodes.at(visibleCursor++);
            }
        }
        return true;
    }

    std::vector<Workspace>& WorkspaceManager::Workspaces() noexcept
    {
        return _workspaces;
    }

    const std::vector<Workspace>& WorkspaceManager::Workspaces() const noexcept
    {
        return _workspaces;
    }

    void WorkspaceManager::SetWorkspaces(std::vector<Workspace> workspaces)
    {
        _workspaces = std::move(workspaces);
    }

    std::wstring SanitizeWorkspaceDirectoryName(std::wstring_view value, std::wstring_view fallback) noexcept
    {
        return terminal::workspacepaths::SanitizeWorkspaceDirectoryName(value, fallback);
    }

    std::wstring NormalizeWorkspaceColor(const std::wstring_view color) noexcept
    {
        if (color.size() != 7 || color[0] != L'#')
        {
            return {};
        }

        std::wstring normalized;
        normalized.reserve(color.size());
        normalized.push_back(L'#');

        for (size_t i = 1; i < color.size(); ++i)
        {
            if (!_isHexDigit(color[i]))
            {
                return {};
            }
            normalized.push_back(static_cast<wchar_t>(std::towupper(color[i])));
        }

        return normalized;
    }

    std::wstring PickUnusedWorkspaceColor(const std::vector<Workspace>& workspaces)
    {
        std::unordered_set<std::wstring> usedColors;
        usedColors.reserve(workspaces.size());
        for (const auto& workspace : workspaces)
        {
            if (const auto normalized = NormalizeWorkspaceColor(workspace.BackgroundColor); !normalized.empty())
            {
                usedColors.emplace(std::move(normalized));
            }
        }

        for (const auto color : _workspaceColorPalette)
        {
            if (!usedColors.contains(std::wstring{ color }))
            {
                return std::wstring{ color };
            }
        }

        return std::wstring{ _workspaceColorPalette[workspaces.size() % _workspaceColorPalette.size()] };
    }

    std::wstring MakeUniquePersistedName(const std::wstring_view baseName, std::unordered_set<std::wstring>& usedNames)
    {
        std::wstring candidate{ baseName };
        auto lowered = _toLower(candidate);
        for (size_t index = 2; !usedNames.emplace(lowered).second; ++index)
        {
            candidate = baseName;
            candidate += L" (";
            candidate += std::to_wstring(index);
            candidate += L")";
            lowered = _toLower(candidate);
        }
        return candidate;
    }

    void NormalizeWorkspacePersistableNames(Workspace& workspace)
    {
        workspace.Name = SanitizeWorkspaceDirectoryName(workspace.Name, L"workspace");
        workspace.Id = workspace.Name;

        const auto previousTabOrder = workspace.TabOrder;
        std::vector<std::wstring> originalNodeIds;
        originalNodeIds.reserve(workspace.Nodes.size());
        for (const auto& node : workspace.Nodes)
        {
            originalNodeIds.emplace_back(node.Id);
        }

        std::unordered_set<std::wstring> usedNodeNames;
        for (auto& node : workspace.Nodes)
        {
            node.Name = SanitizeWorkspaceDirectoryName(node.Name, L"tab");
            node.Name = MakeUniquePersistedName(node.Name, usedNodeNames);
            node.Id = node.Name;
        }

        std::unordered_map<std::wstring, std::wstring> normalizedNodeIds;
        for (size_t nodeIndex = 0; nodeIndex < workspace.Nodes.size() && nodeIndex < originalNodeIds.size(); ++nodeIndex)
        {
            normalizedNodeIds.emplace(originalNodeIds.at(nodeIndex), workspace.Nodes.at(nodeIndex).Id);
        }

        std::vector<std::wstring> normalizedTabOrder;
        normalizedTabOrder.reserve(previousTabOrder.size());
        for (const auto& nodeId : previousTabOrder)
        {
            if (const auto it = normalizedNodeIds.find(nodeId); it != normalizedNodeIds.end() && !it->second.empty())
            {
                normalizedTabOrder.emplace_back(it->second);
            }
        }
        workspace.TabOrder = std::move(normalizedTabOrder);
    }

    bool WorkspaceNodeEquivalent(const WorkspaceNode& lhs, const WorkspaceNode& rhs)
    {
        const auto lhsTabColor = NormalizeWorkspaceColor(lhs.TabColor).empty() ? lhs.TabColor : NormalizeWorkspaceColor(lhs.TabColor);
        const auto rhsTabColor = NormalizeWorkspaceColor(rhs.TabColor).empty() ? rhs.TabColor : NormalizeWorkspaceColor(rhs.TabColor);
        return lhs.Name == rhs.Name &&
               lhs.ProfileGuid == rhs.ProfileGuid &&
               lhs.ProfileName == rhs.ProfileName &&
               lhs.Icon == rhs.Icon &&
               lhsTabColor == rhsTabColor &&
               lhs.ShowTab == rhs.ShowTab &&
               lhs.StartupDirectory == rhs.StartupDirectory &&
               lhs.StartupAction == rhs.StartupAction &&
               lhs.OperatingSystem == rhs.OperatingSystem &&
               lhs.ShellType == rhs.ShellType &&
               lhs.ShowInputPanel == rhs.ShowInputPanel &&
               lhs.UseNodeNameAsTabTitle == rhs.UseNodeNameAsTabTitle &&
               (lhs.ConnectionRef.empty() || rhs.ConnectionRef.empty() || lhs.ConnectionRef == rhs.ConnectionRef);
    }

    bool WorkspaceLayoutEquivalent(const Workspace& lhs, const Workspace& rhs)
    {
        if (lhs.TabOrder != rhs.TabOrder)
        {
            return false;
        }

        if (lhs.Nodes.size() != rhs.Nodes.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs.Nodes.size(); ++i)
        {
            if (!WorkspaceNodeEquivalent(lhs.Nodes[i], rhs.Nodes[i]))
            {
                return false;
            }
        }

        return true;
    }

    WorkspaceSavePlan PrepareWorkspaceForSave(const Workspace& capturedWorkspace,
                                              const std::vector<Workspace>& existingWorkspaces,
                                              const std::wstring_view targetWorkspaceId,
                                              const std::wstring_view explicitWorkspaceName,
                                              const std::wstring_view fallbackWindowName,
                                              const std::wstring_view fallbackTargetName,
                                              const std::wstring_view fallbackSingleTabTitle,
                                              const std::wstring_view generatedFallbackName)
    {
        WorkspaceSavePlan plan;
        plan.Workspaces = existingWorkspaces;

        auto workspace = capturedWorkspace;
        const auto existingWorkspaceIt = !targetWorkspaceId.empty() ?
                                             std::find_if(plan.Workspaces.begin(), plan.Workspaces.end(), [&](const auto& existingWorkspace) {
                                                 return existingWorkspace.Id == targetWorkspaceId;
                                             }) :
                                             plan.Workspaces.end();

        if (existingWorkspaceIt != plan.Workspaces.end())
        {
            workspace.Name = existingWorkspaceIt->Name;
            workspace.Description = existingWorkspaceIt->Description;
            workspace.BackgroundColor = existingWorkspaceIt->BackgroundColor;
            workspace.Locked = true;

            for (auto& node : workspace.Nodes)
            {
                if (node.ConnectionRef.empty() || node.Id.empty())
                {
                    const auto existingNodeIt = std::find_if(existingWorkspaceIt->Nodes.begin(), existingWorkspaceIt->Nodes.end(), [&](const auto& existingNode) {
                        return existingNode.Id == node.Id && !existingNode.ConnectionRef.empty();
                    });
                    if (existingNodeIt != existingWorkspaceIt->Nodes.end())
                    {
                        node.ConnectionRef = existingNodeIt->ConnectionRef;
                    }
                }
            }

            *existingWorkspaceIt = workspace;
            plan.SavedWorkspaceIndex = static_cast<size_t>(std::distance(plan.Workspaces.begin(), existingWorkspaceIt));
        }
        else
        {
            if (!explicitWorkspaceName.empty())
            {
                workspace.Name = explicitWorkspaceName;
            }
            else if (!fallbackWindowName.empty())
            {
                workspace.Name = fallbackWindowName;
            }
            else if (!fallbackTargetName.empty())
            {
                workspace.Name = fallbackTargetName;
            }
            else if (!fallbackSingleTabTitle.empty())
            {
                workspace.Name = fallbackSingleTabTitle;
            }

            if (workspace.Name.empty())
            {
                workspace.Name = generatedFallbackName;
            }

            workspace.BackgroundColor = PickUnusedWorkspaceColor(plan.Workspaces);
            workspace.Locked = true;
            plan.Workspaces.emplace_back(workspace);
            plan.SavedWorkspaceIndex = plan.Workspaces.size() - 1;
        }

        std::unordered_set<std::wstring> usedWorkspaceNames;
        for (auto& candidate : plan.Workspaces)
        {
            const auto previousTabOrder = candidate.TabOrder;
            std::vector<std::wstring> originalNodeIds;
            originalNodeIds.reserve(candidate.Nodes.size());
            for (const auto& node : candidate.Nodes)
            {
                originalNodeIds.emplace_back(node.Id);
            }

            NormalizeWorkspacePersistableNames(candidate);

            std::unordered_map<std::wstring, std::wstring> normalizedNodeIds;
            for (size_t nodeIndex = 0; nodeIndex < candidate.Nodes.size() && nodeIndex < originalNodeIds.size(); ++nodeIndex)
            {
                normalizedNodeIds.emplace(originalNodeIds.at(nodeIndex), candidate.Nodes.at(nodeIndex).Id);
            }

            std::vector<std::wstring> normalizedTabOrder;
            normalizedTabOrder.reserve(previousTabOrder.size());
            for (const auto& nodeId : previousTabOrder)
            {
                if (const auto it = normalizedNodeIds.find(nodeId); it != normalizedNodeIds.end() && !it->second.empty())
                {
                    normalizedTabOrder.emplace_back(it->second);
                }
            }
            candidate.TabOrder = std::move(normalizedTabOrder);

            candidate.Name = MakeUniquePersistedName(candidate.Name, usedWorkspaceNames);
            candidate.Id = candidate.Name;
        }

        plan.SavedWorkspace = plan.Workspaces.at(plan.SavedWorkspaceIndex);
        return plan;
    }

    WorkspaceOpenPlan PrepareWorkspaceForOpen(const std::wstring_view workspaceId,
                                              const bool openInNewWindow,
                                              const std::wstring_view currentWorkspaceId,
                                              const bool currentWorkspaceNeedsSave,
                                              const WorkspaceManager& manager,
                                              const WorkspaceStateManager& stateManager)
    {
        WorkspaceOpenPlan plan;
        if (workspaceId.empty())
        {
            return plan;
        }

        if (const auto existingWindowId = stateManager.FindOpenWorkspaceWindowId(workspaceId))
        {
            plan.Disposition = WorkspaceOpenDisposition::SummonExistingWindow;
            plan.ExistingWindowId = existingWindowId;
            return plan;
        }

        const auto workspace = manager.FindById(workspaceId);
        if (workspace == nullptr)
        {
            return plan;
        }

        plan.TargetWorkspace = *workspace;
        plan.PendingNodeIds = VisibleWorkspaceNodeIds(*workspace);
        plan.PendingNodeInputVisibility = VisibleWorkspaceNodeInputVisibility(*workspace);
        // Workspaces are immutable at runtime and always get their own window.
        // Retain the legacy parameters for the adapter ABI while intentionally
        // ignoring the old replace-current/save path.
        (void)openInNewWindow;
        (void)currentWorkspaceId;
        (void)currentWorkspaceNeedsSave;
        plan.ConfirmSaveCurrentWorkspace = false;
        plan.Disposition = WorkspaceOpenDisposition::OpenInNewWindow;
        return plan;
    }

    WorkspaceOpenExecutionPlan ResolveWorkspaceOpenExecutionPlan(const WorkspaceOpenPlan& openPlan,
                                                                 const bool hasStartupActions,
                                                                 const bool hasTabsToReplace)
    {
        WorkspaceOpenExecutionPlan plan;
        plan.ExistingWindowId = openPlan.ExistingWindowId;
        plan.ConfirmSaveCurrentWorkspace = openPlan.ConfirmSaveCurrentWorkspace;

        switch (openPlan.Disposition)
        {
        case WorkspaceOpenDisposition::SummonExistingWindow:
            plan.Disposition = WorkspaceOpenExecutionDisposition::SummonExistingWindow;
            return plan;
        case WorkspaceOpenDisposition::Missing:
            plan.Disposition = WorkspaceOpenExecutionDisposition::Missing;
            return plan;
        case WorkspaceOpenDisposition::OpenInNewWindow:
            if (!hasStartupActions)
            {
                plan.Disposition = WorkspaceOpenExecutionDisposition::NoStartupActions;
                return plan;
            }
            plan.Disposition = WorkspaceOpenExecutionDisposition::OpenInNewWindow;
            plan.UpdatePendingWorkspaceLaunch = true;
            return plan;
        case WorkspaceOpenDisposition::ReplaceCurrentWindow:
            // Compatibility for callers that still construct the old disposition:
            // route it to the same new-window behavior rather than replacing tabs.
            (void)hasTabsToReplace;
            plan.Disposition = hasStartupActions ? WorkspaceOpenExecutionDisposition::OpenInNewWindow :
                                                   WorkspaceOpenExecutionDisposition::NoStartupActions;
            plan.UpdatePendingWorkspaceLaunch = hasStartupActions;
            return plan;
        default:
            plan.Disposition = WorkspaceOpenExecutionDisposition::Missing;
            return plan;
        }
    }

    WorkspaceSshStartupPlan PrepareSshStartupPlan(const std::wstring_view pendingStartupAction,
                                                  const std::wstring_view startingDirectory,
                                                  const std::wstring_view operatingSystem,
                                                  const std::wstring_view shellType,
                                                  const std::optional<WorkspaceNode>& workspaceNode)
    {
        WorkspaceSshStartupPlan plan;
        plan.StartingDirectory = std::wstring{ startingDirectory };
        plan.OperatingSystem = std::wstring{ operatingSystem };
        plan.ShellType = std::wstring{ shellType };

        if (workspaceNode)
        {
            if (plan.StartingDirectory.empty())
            {
                plan.StartingDirectory = workspaceNode->StartupDirectory;
            }
            if (plan.OperatingSystem.empty())
            {
                plan.OperatingSystem = workspaceNode->OperatingSystem;
            }
            if (plan.ShellType.empty())
            {
                plan.ShellType = workspaceNode->ShellType;
            }

            plan.StartupAction = workspaceNode->StartupAction;
            plan.DeferredStartupInputs = _buildSshDeferredStartupInputs(*workspaceNode);
        }
        else
        {
            plan.StartupAction = std::wstring{ pendingStartupAction };
        }

        if (plan.DeferredStartupInputs.empty() && !pendingStartupAction.empty())
        {
            plan.DeferredStartupInputs.emplace_back(pendingStartupAction);
        }

        plan.StartupInputPending = !plan.DeferredStartupInputs.empty();
        return plan;
    }

    WorkspaceNode BuildWorkspaceCapturedNode(const WorkspaceLiveTabCaptureState& state)
    {
        WorkspaceNode node = state.PersistedNode.value_or(WorkspaceNode{});
        if (!state.PersistedNode.has_value())
        {
            node.UseNodeNameAsTabTitle = false;
            node.Name = state.LiveTabTitle.empty() ? state.StartupTabTitle : state.LiveTabTitle;
            if (node.Name.empty())
            {
                node.Name = state.GeneratedNodeName;
            }
            node.Id = node.Name;
        }

        node.ProfileGuid = state.ProfileGuid;
        if (!state.ProfileName.empty() || !state.PersistedNode.has_value())
        {
            node.ProfileName = state.ProfileName;
        }
        node.StartupDirectory = state.LaunchResolution.StartingDirectory;
        node.StartupAction = state.LaunchResolution.StartupAction;
        node.OperatingSystem = state.LaunchResolution.OperatingSystem;
        node.ShellType = state.LaunchResolution.ShellType;
        node.ShowInputPanel = state.ShowInputPanel;
        node.TabColor = state.TabColor;
        return node;
    }

    bool SetWorkspaceLocked(WorkspaceManager& manager, const std::wstring_view workspaceId, const bool locked)
    {
        if (const auto workspace = std::find_if(manager.Workspaces().begin(), manager.Workspaces().end(), [&](const auto& candidate) {
                return candidate.Id == workspaceId;
            });
            workspace != manager.Workspaces().end())
        {
            // A workspace is permanently locked outside the management tab.
            (void)locked;
            workspace->Locked = true;
            return true;
        }

        return false;
    }

    bool SetWorkspaceNodeInputVisibility(WorkspaceManager& manager, const std::wstring_view workspaceId, const size_t nodeIndex, const bool showInputPanel)
    {
        if (const auto workspace = std::find_if(manager.Workspaces().begin(), manager.Workspaces().end(), [&](const auto& candidate) {
                return candidate.Id == workspaceId;
            });
            workspace != manager.Workspaces().end() && nodeIndex < workspace->Nodes.size())
        {
            workspace->Nodes.at(nodeIndex).ShowInputPanel = showInputPanel;
            return true;
        }

        return false;
    }

    bool RemoveWorkspaceDefinition(WorkspaceManager& manager, const std::wstring_view workspaceId, size_t* removedWorkspaceIndex)
    {
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return false;
        }

        if (removedWorkspaceIndex)
        {
            *removedWorkspaceIndex = static_cast<size_t>(std::distance(workspaces.begin(), workspaceIt));
        }
        workspaces.erase(workspaceIt);
        return true;
    }

    WorkspaceNodeMutationResult RemoveWorkspaceNode(WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view nodeId)
    {
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return {};
        }

        auto& nodes = workspaceIt->Nodes;
        const auto nodeIt = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
            return node.Id == nodeId;
        });
        if (nodeIt == nodes.end())
        {
            return {};
        }

        WorkspaceNodeMutationResult result;
        result.WorkspaceIndex = static_cast<size_t>(std::distance(workspaces.begin(), workspaceIt));
        result.NodeIndex = static_cast<size_t>(std::distance(nodes.begin(), nodeIt));
        if (nodes.size() == 1)
        {
            workspaces.erase(workspaceIt);
            result.Disposition = WorkspaceNodeMutationDisposition::RemovedWorkspace;
            return result;
        }

        nodes.erase(nodeIt);
        result.Disposition = WorkspaceNodeMutationDisposition::RemovedNode;
        return result;
    }

    void FinalizeWorkspaceManagerNames(WorkspaceManager& manager)
    {
        std::unordered_set<std::wstring> usedWorkspaceNames;
        for (auto& workspace : manager.Workspaces())
        {
            NormalizeWorkspacePersistableNames(workspace);
            workspace.Name = MakeUniquePersistedName(workspace.Name, usedWorkspaceNames);
            workspace.Id = workspace.Name;
        }
    }

    std::optional<std::wstring> RenameWorkspace(WorkspaceManager& manager, const std::wstring_view workspaceId, const std::wstring_view newName)
    {
        auto& workspaces = manager.Workspaces();
        const auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& workspace) {
            return workspace.Id == workspaceId;
        });
        if (workspaceIt == workspaces.end())
        {
            return std::nullopt;
        }

        const auto workspaceIndex = static_cast<size_t>(std::distance(workspaces.begin(), workspaceIt));
        workspaceIt->Name = SanitizeWorkspaceDirectoryName(newName, L"workspace");
        FinalizeWorkspaceManagerNames(manager);
        if (workspaceIndex >= workspaces.size())
        {
            return std::nullopt;
        }

        return workspaces.at(workspaceIndex).Name;
    }

    std::wstring ResolveWorkspaceSaveTargetId(const std::wstring_view currentWorkspaceId, const std::wstring_view lastWorkspaceId, const WorkspaceManager& manager)
    {
        if (!currentWorkspaceId.empty())
        {
            return std::wstring{ currentWorkspaceId };
        }

        if (lastWorkspaceId.empty())
        {
            return {};
        }

        return manager.FindById(lastWorkspaceId) ? std::wstring{ lastWorkspaceId } : std::wstring{};
    }

    std::wstring ResolveWorkspaceSaveTargetName(const std::wstring_view currentWorkspaceId, const std::wstring_view lastWorkspaceId, const WorkspaceManager& manager)
    {
        if (const auto targetId = ResolveWorkspaceSaveTargetId(currentWorkspaceId, lastWorkspaceId, manager); !targetId.empty())
        {
            if (const auto workspace = manager.FindById(targetId))
            {
                return workspace->Name;
            }
        }

        return {};
    }

    std::wstring SuggestWorkspaceSaveName(const std::wstring_view resolvedTargetName,
                                          const std::wstring_view windowName,
                                          const std::wstring_view singleTabTitle,
                                          const size_t workspaceCount,
                                          const std::wstring_view generatedFallbackName)
    {
        std::wstring suggestedName;
        if (!resolvedTargetName.empty())
        {
            suggestedName = resolvedTargetName;
        }
        else if (!windowName.empty())
        {
            suggestedName = windowName;
        }
        else if (!singleTabTitle.empty())
        {
            suggestedName = singleTabTitle;
        }
        else if (!generatedFallbackName.empty())
        {
            suggestedName = generatedFallbackName;
        }
        else
        {
            suggestedName = L"Workspace " + std::to_wstring(workspaceCount + 1);
        }

        return SanitizeWorkspaceDirectoryName(suggestedName, L"Workspace");
    }

    std::filesystem::path WorkspaceStateManager::DefaultPath()
    {
        return terminal::workspacepaths::ResolveWorkspaceDefinitionsPath();
    }

    WorkspaceStateManager WorkspaceStateManager::Load()
    {
        return LoadFromPath(DefaultPath());
    }

    WorkspaceStateManager WorkspaceStateManager::LoadFromPath(const std::filesystem::path& path)
    {
        WorkspaceStateManager manager;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
        {
            return manager;
        }

        if (std::filesystem::is_regular_file(path, ec) && !ec)
        {
            return manager;
        }

        if (!std::filesystem::is_directory(path, ec) || ec)
        {
            return manager;
        }

        const auto persistedWorkspaces = _enumeratePersistedWorkspaceDirectories(path);
        if (!persistedWorkspaces.has_value())
        {
            return manager;
        }

        for (const auto& workspace : *persistedWorkspaces)
        {
            const auto statePath = workspace.Directory / std::filesystem::path{ terminal::workspacepaths::WorkspaceStateFileName };
            if (!std::filesystem::exists(statePath, ec) || ec)
            {
                ec.clear();
                continue;
            }

            _loadWorkspaceStateFile(manager, statePath, workspace.Definition.Id);
        }

        return manager;
    }

    bool WorkspaceStateManager::Save() const
    {
        return SaveToPath(DefaultPath());
    }

    bool WorkspaceStateManager::SaveToPath(const std::filesystem::path& path) const
    {
        if (path.has_extension())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec)
        {
            return false;
        }

        const auto persistedWorkspaces = _enumeratePersistedWorkspaceDirectoriesIfPresent(path);
        if (!persistedWorkspaces.has_value())
        {
            return false;
        }

        std::unordered_set<std::wstring> touchedWorkspaces;
        for (const auto& window : _windows)
        {
            if (window.WorkspaceId.empty())
            {
                continue;
            }

            touchedWorkspaces.emplace(window.WorkspaceId);
        }

        for (const auto& workspace : *persistedWorkspaces)
        {
            std::vector<WorkspaceStateWindow> windows;
            for (const auto& window : _windows)
            {
                if (window.WorkspaceId == workspace.Definition.Id)
                {
                    windows.emplace_back(window);
                }
            }

            const auto statePath = workspace.Directory / std::filesystem::path{ terminal::workspacepaths::WorkspaceStateFileName };
            if (windows.empty())
            {
                std::filesystem::remove(statePath, ec);
                if (ec)
                {
                    return false;
                }
                continue;
            }

            if (!_writeUtf8TextFile(statePath, _serializeWorkspaceStateFile(windows)))
            {
                return false;
            }
        }

        for (const auto& workspaceId : touchedWorkspaces)
        {
            const auto existingIt = std::find_if(persistedWorkspaces->begin(), persistedWorkspaces->end(), [&](const auto& workspace) {
                return workspace.Definition.Id == workspaceId;
            });
            if (existingIt != persistedWorkspaces->end())
            {
                continue;
            }

            const auto workspaceDir = path / SanitizeWorkspaceDirectoryName(workspaceId, L"workspace");
            const auto statePath = workspaceDir / std::filesystem::path{ terminal::workspacepaths::WorkspaceStateFileName };

            std::vector<WorkspaceStateWindow> windows;
            for (const auto& window : _windows)
            {
                if (window.WorkspaceId == workspaceId)
                {
                    windows.emplace_back(window);
                }
            }

            if (windows.empty())
            {
                continue;
            }

            std::filesystem::create_directories(workspaceDir, ec);
            if (ec)
            {
                return false;
            }

            if (!_writeUtf8TextFile(statePath, _serializeWorkspaceStateFile(windows)))
            {
                return false;
            }
        }

        if (path == terminal::workspacepaths::ResolveWorkspaceDefinitionsPath())
        {
            return _removeLegacyWorkspaceStateFilesIfPresent();
        }

        return true;
    }

    const std::vector<WorkspaceStateWindow>& WorkspaceStateManager::Windows() const noexcept
    {
        return _windows;
    }

    void WorkspaceStateManager::SetWindows(std::vector<WorkspaceStateWindow> windows)
    {
        _windows = std::move(windows);
    }

    void WorkspaceStateManager::UpsertWindow(WorkspaceStateWindow window)
    {
        const auto it = std::find_if(_windows.begin(), _windows.end(), [&](const auto& candidate) {
            return candidate.WindowId == window.WindowId;
        });

        if (it != _windows.end())
        {
            *it = std::move(window);
        }
        else
        {
            _windows.emplace_back(std::move(window));
        }
    }

    void WorkspaceStateManager::RemoveWindow(uint64_t windowId) noexcept
    {
        _windows.erase(std::remove_if(_windows.begin(), _windows.end(), [&](const auto& window) {
            return window.WindowId == windowId;
        }), _windows.end());
    }

    void WorkspaceStateManager::UpdateWindowState(const uint64_t windowId, const std::wstring_view windowName, const std::wstring_view workspaceId)
    {
        if (workspaceId.empty())
        {
            RemoveWindow(windowId);
            return;
        }

        WorkspaceStateWindow window;
        window.WindowId = windowId;
        window.WindowName = std::wstring{ windowName };
        window.WorkspaceId = std::wstring{ workspaceId };
        UpsertWindow(std::move(window));
    }

    std::optional<uint64_t> WorkspaceStateManager::FindOpenWorkspaceWindowId(const std::wstring_view workspaceId) const noexcept
    {
        if (workspaceId.empty())
        {
            return std::nullopt;
        }

        const auto found = std::find_if(_windows.begin(), _windows.end(), [&](const auto& window) {
            return window.WorkspaceId == workspaceId && window.WindowId != 0;
        });
        if (found == _windows.end())
        {
            return std::nullopt;
        }

        return found->WindowId;
    }
}
