#include "TerminalInputHarness.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <optional>

namespace terminal::workspacechat
{
    namespace
    {
        std::wstring _trim(std::wstring_view value)
        {
            const auto begin = std::find_if_not(value.begin(), value.end(), [](const auto ch) {
                return std::iswspace(ch) != 0;
            });
            if (begin == value.end())
            {
                return {};
            }

            const auto end = std::find_if_not(value.rbegin(), value.rend(), [](const auto ch) {
                return std::iswspace(ch) != 0;
            }).base();
            return std::wstring{ begin, end };
        }

        bool _equalsInsensitive(std::wstring_view lhs, std::wstring_view rhs)
        {
            if (lhs.size() != rhs.size())
            {
                return false;
            }

            for (size_t i = 0; i < lhs.size(); ++i)
            {
                if (std::towlower(lhs[i]) != std::towlower(rhs[i]))
                {
                    return false;
                }
            }
            return true;
        }

        std::optional<size_t> _driveIndex(std::wstring_view value)
        {
            if (value.empty() || std::iswalpha(value[0]) == 0)
            {
                return std::nullopt;
            }

            const auto lowered = static_cast<wchar_t>(std::towlower(value[0]));
            return static_cast<size_t>(lowered - L'a');
        }

        bool _isWindowsDriveSwitch(std::wstring_view value)
        {
            return value.size() == 2 && std::iswalpha(value[0]) != 0 && value[1] == L':';
        }

        std::wstring _inferOperatingSystemFromPath(std::wstring_view path)
        {
            if (path.empty())
            {
                return {};
            }

            if ((path.size() >= 2 && std::iswalpha(path[0]) != 0 && path[1] == L':') ||
                path.starts_with(L"\\\\") ||
                path.find(L'\\') != std::wstring_view::npos)
            {
                return L"windows";
            }

            if (path.starts_with(L"/") || path.starts_with(L"~"))
            {
                return L"linux";
            }

            return {};
        }

        void _recordDriveDirectory(TerminalInputState& state, std::wstring_view directory)
        {
            if (directory.size() < 2 || directory[1] != L':')
            {
                return;
            }

            if (const auto index = _driveIndex(directory))
            {
                state.LastWorkingDirectoryByDrive[*index] = std::wstring{ directory };
            }
        }

        void _setOperatingSystem(TerminalInputState& state, std::wstring value)
        {
            if (!value.empty())
            {
                state.OperatingSystem = std::move(value);
            }
        }

        void _setShellType(TerminalInputState& state, std::wstring shell, std::wstring operatingSystem = {})
        {
            if (!shell.empty())
            {
                state.ShellType = std::move(shell);
            }
            _setOperatingSystem(state, std::move(operatingSystem));
        }

        bool _isKnownDirectoryCommand(std::wstring_view command)
        {
            return _equalsInsensitive(command, L"cd") ||
                   _equalsInsensitive(command, L"chdir") ||
                   _equalsInsensitive(command, L"set-location") ||
                   _equalsInsensitive(command, L"sl") ||
                   _equalsInsensitive(command, L"pushd") ||
                   _equalsInsensitive(command, L"push-location") ||
                   _equalsInsensitive(command, L"popd") ||
                   _equalsInsensitive(command, L"pop-location");
        }

        std::vector<std::wstring> _splitCommandSegments(std::wstring_view line)
        {
            std::vector<std::wstring> segments;
            std::wstring current;
            bool inSingleQuotes = false;
            bool inDoubleQuotes = false;

            const auto flush = [&]() {
                const auto segment = _trim(current);
                if (!segment.empty())
                {
                    segments.emplace_back(segment);
                }
                current.clear();
            };

            for (size_t i = 0; i < line.size(); ++i)
            {
                const auto ch = line[i];
                if (ch == L'\'' && !inDoubleQuotes)
                {
                    inSingleQuotes = !inSingleQuotes;
                    current.push_back(ch);
                    continue;
                }
                if (ch == L'"' && !inSingleQuotes)
                {
                    inDoubleQuotes = !inDoubleQuotes;
                    current.push_back(ch);
                    continue;
                }

                if (!inSingleQuotes && !inDoubleQuotes)
                {
                    const auto next = i + 1 < line.size() ? line[i + 1] : wchar_t{};
                    if ((ch == L'&' && next == L'&') || (ch == L'|' && next == L'|'))
                    {
                        flush();
                        ++i;
                        continue;
                    }
                    if (ch == L';')
                    {
                        flush();
                        continue;
                    }
                }

                current.push_back(ch);
            }

            flush();
            return segments;
        }

        std::vector<std::wstring> _tokenize(std::wstring_view segment)
        {
            std::vector<std::wstring> tokens;
            std::wstring current;
            bool inSingleQuotes = false;
            bool inDoubleQuotes = false;

            const auto flush = [&]() {
                if (!current.empty())
                {
                    tokens.emplace_back(std::move(current));
                    current.clear();
                }
            };

            for (const auto ch : segment)
            {
                if (ch == L'\'' && !inDoubleQuotes)
                {
                    inSingleQuotes = !inSingleQuotes;
                    continue;
                }
                if (ch == L'"' && !inSingleQuotes)
                {
                    inDoubleQuotes = !inDoubleQuotes;
                    continue;
                }

                if (!inSingleQuotes && !inDoubleQuotes && std::iswspace(ch) != 0)
                {
                    flush();
                    continue;
                }

                current.push_back(ch);
            }

            flush();
            return tokens;
        }

        void _setWorkingDirectory(TerminalInputState& state, std::wstring directory)
        {
            directory = _trim(directory);
            if (directory.empty())
            {
                return;
            }

            _recordDriveDirectory(state, directory);
            _setOperatingSystem(state, _inferOperatingSystemFromPath(directory));

            if (directory == state.LastWorkingDirectory)
            {
                return;
            }

            if (!state.LastWorkingDirectory.empty())
            {
                state.PreviousWorkingDirectory = state.LastWorkingDirectory;
            }
            state.LastWorkingDirectory = std::move(directory);
        }

        bool _isWindowsBasePath(std::wstring_view path)
        {
            if (path.size() >= 2 && std::iswalpha(path[0]) != 0 && path[1] == L':')
            {
                return true;
            }
            if (path.starts_with(L"\\\\") || path.starts_with(L"\\"))
            {
                return true;
            }
            return path.find(L'\\') != std::wstring_view::npos;
        }

        bool _isPosixBasePath(std::wstring_view path)
        {
            return path.starts_with(L"/") || path.starts_with(L"~");
        }

        bool _looksLikeWindowsPath(std::wstring_view path, std::wstring_view base, std::wstring_view operatingSystem)
        {
            if (_isWindowsBasePath(path))
            {
                return true;
            }
            if (_isPosixBasePath(path))
            {
                return _equalsInsensitive(operatingSystem, L"windows");
            }

            if (_equalsInsensitive(operatingSystem, L"linux"))
            {
                return false;
            }

            if (_isWindowsBasePath(base))
            {
                return true;
            }
            if (_isPosixBasePath(base))
            {
                return false;
            }

            return _equalsInsensitive(operatingSystem, L"windows");
        }

        std::wstring _normalizeWindowsPath(std::wstring_view currentDirectory, std::wstring_view target)
        {
            auto normalizedTarget = std::wstring{ target };
            std::replace(normalizedTarget.begin(), normalizedTarget.end(), L'/', L'\\');

            const std::filesystem::path targetPath{ normalizedTarget };
            if (!targetPath.is_absolute() && currentDirectory.empty())
            {
                return {};
            }

            const auto resolved = targetPath.is_absolute() ? targetPath : (std::filesystem::path{ currentDirectory } / targetPath);
            return resolved.lexically_normal().wstring();
        }

        std::vector<std::wstring> _splitPosixPath(std::wstring_view path)
        {
            std::vector<std::wstring> parts;
            std::wstring current;
            for (const auto ch : path)
            {
                if (ch == L'/')
                {
                    if (!current.empty())
                    {
                        parts.emplace_back(std::move(current));
                        current.clear();
                    }
                    continue;
                }
                current.push_back(ch);
            }
            if (!current.empty())
            {
                parts.emplace_back(std::move(current));
            }
            return parts;
        }

        std::wstring _joinPosixPath(const bool absolute, const std::vector<std::wstring>& parts)
        {
            std::wstring result = absolute ? L"/" : L"";
            for (size_t i = 0; i < parts.size(); ++i)
            {
                if (i > 0 || absolute)
                {
                    if (!result.empty() && result.back() != L'/')
                    {
                        result.push_back(L'/');
                    }
                }
                result.append(parts[i]);
            }

            if (result.empty() && absolute)
            {
                result = L"/";
            }
            return result;
        }

        std::wstring _normalizePosixPath(std::wstring_view currentDirectory, std::wstring_view target)
        {
            if (target.starts_with(L"~"))
            {
                return std::wstring{ target };
            }

            const auto absolute = target.starts_with(L"/");
            if (!absolute && currentDirectory.empty())
            {
                return {};
            }

            std::vector<std::wstring> parts;
            bool resultAbsolute = absolute;

            if (!absolute)
            {
                resultAbsolute = currentDirectory.starts_with(L"/");
                parts = _splitPosixPath(currentDirectory);
            }

            for (const auto& part : _splitPosixPath(target))
            {
                if (part.empty() || part == L".")
                {
                    continue;
                }
                if (part == L"..")
                {
                    if (!parts.empty() && parts.back() != L"..")
                    {
                        parts.pop_back();
                    }
                    else if (!resultAbsolute)
                    {
                        parts.emplace_back(part);
                    }
                    continue;
                }
                parts.emplace_back(part);
            }

            return _joinPosixPath(resultAbsolute, parts);
        }

        std::wstring _resolvePath(std::wstring_view currentDirectory, std::wstring_view target, std::wstring_view operatingSystem)
        {
            const auto trimmedTarget = _trim(target);
            if (trimmedTarget.empty())
            {
                return {};
            }

            if (_isPosixBasePath(trimmedTarget))
            {
                const auto posixBase = _isPosixBasePath(currentDirectory) ? currentDirectory : std::wstring_view{};
                return _normalizePosixPath(posixBase, trimmedTarget);
            }

            if (_looksLikeWindowsPath(trimmedTarget, currentDirectory, operatingSystem))
            {
                const auto windowsBase = _isWindowsBasePath(currentDirectory) ? currentDirectory : std::wstring_view{};
                return _normalizeWindowsPath(windowsBase, trimmedTarget);
            }

            const auto posixBase = _isPosixBasePath(currentDirectory) ? currentDirectory : std::wstring_view{};
            return _normalizePosixPath(posixBase, trimmedTarget);
        }

        std::optional<std::wstring> _extractPathArgument(const std::vector<std::wstring>& tokens)
        {
            if (tokens.size() < 2)
            {
                return std::nullopt;
            }

            for (size_t i = 1; i < tokens.size(); ++i)
            {
                const auto& token = tokens[i];
                if (_equalsInsensitive(token, L"/d") || token == L"--")
                {
                    continue;
                }
                if (_equalsInsensitive(token, L"-path") || _equalsInsensitive(token, L"-literalpath"))
                {
                    if (i + 1 < tokens.size())
                    {
                        return tokens[i + 1];
                    }
                    return std::nullopt;
                }
                return token;
            }

            return std::nullopt;
        }

        void _applyDriveSwitch(TerminalInputState& state, std::wstring_view commandSegment)
        {
            const auto drive = static_cast<wchar_t>(std::towupper(commandSegment[0]));
            std::wstring restored;
            if (const auto index = _driveIndex(commandSegment))
            {
                restored = state.LastWorkingDirectoryByDrive[*index];
            }

            if (restored.empty())
            {
                restored.push_back(drive);
                restored.append(L":\\");
            }

            _setOperatingSystem(state, L"windows");
            _setWorkingDirectory(state, std::move(restored));
        }

        void _applyShellHeuristics(TerminalInputState& state,
                                   std::wstring_view commandSegment,
                                   const std::vector<std::wstring>& tokens)
        {
            if (_isWindowsDriveSwitch(commandSegment))
            {
                _setOperatingSystem(state, L"windows");
                return;
            }

            if (tokens.empty())
            {
                return;
            }

            const auto& command = tokens.front();
            if (_equalsInsensitive(command, L"set-location") ||
                _equalsInsensitive(command, L"sl") ||
                _equalsInsensitive(command, L"push-location") ||
                _equalsInsensitive(command, L"pop-location"))
            {
                _setShellType(state, L"powershell", L"windows");
                return;
            }

            if (_equalsInsensitive(command, L"powershell") ||
                _equalsInsensitive(command, L"powershell.exe") ||
                _equalsInsensitive(command, L"pwsh") ||
                _equalsInsensitive(command, L"pwsh.exe"))
            {
                _setShellType(state, L"powershell", L"windows");
                return;
            }

            if (_equalsInsensitive(command, L"cmd") || _equalsInsensitive(command, L"cmd.exe"))
            {
                _setShellType(state, L"cmd", L"windows");
                return;
            }

            if (_equalsInsensitive(command, L"ssh") || _equalsInsensitive(command, L"ssh.exe"))
            {
                _setShellType(state, L"ssh", L"linux");
                return;
            }

            if (_equalsInsensitive(command, L"chdir"))
            {
                _setShellType(state, L"cmd", L"windows");
                return;
            }

            if (const auto pathArg = _extractPathArgument(tokens))
            {
                _setOperatingSystem(state, _inferOperatingSystemFromPath(*pathArg));
            }
        }

        void _applyDirectoryHeuristics(TerminalInputState& state, std::wstring_view commandSegment)
        {
            const auto tokens = _tokenize(commandSegment);
            if (tokens.empty())
            {
                return;
            }

            if (_isWindowsDriveSwitch(commandSegment))
            {
                _applyDriveSwitch(state, commandSegment);
                return;
            }

            _applyShellHeuristics(state, commandSegment, tokens);

            const auto& command = tokens.front();
            if (!_isKnownDirectoryCommand(command))
            {
                return;
            }

            if (_equalsInsensitive(command, L"popd") || _equalsInsensitive(command, L"pop-location"))
            {
                if (!state.DirectoryStack.empty())
                {
                    auto restored = std::move(state.DirectoryStack.back());
                    state.DirectoryStack.pop_back();
                    _setWorkingDirectory(state, std::move(restored));
                }
                return;
            }

            const auto pathArg = _extractPathArgument(tokens);
            if (!pathArg.has_value())
            {
                return;
            }

            if (*pathArg == L"-")
            {
                _setWorkingDirectory(state, state.PreviousWorkingDirectory);
                return;
            }

            const auto resolvedPath = _resolvePath(state.LastWorkingDirectory, *pathArg, state.OperatingSystem);
            if (resolvedPath.empty())
            {
                return;
            }

            if (_equalsInsensitive(command, L"pushd") || _equalsInsensitive(command, L"push-location"))
            {
                if (!state.LastWorkingDirectory.empty())
                {
                    state.DirectoryStack.emplace_back(state.LastWorkingDirectory);
                }
            }

            _setWorkingDirectory(state, resolvedPath);
        }
    }

    std::vector<std::wstring> SplitTerminalInputLines(std::wstring_view text)
    {
        std::vector<std::wstring> lines;
        std::wstring current;

        const auto flush = [&]() {
            const auto line = _trim(current);
            if (!line.empty())
            {
                lines.emplace_back(line);
            }
            current.clear();
        };

        for (size_t i = 0; i < text.size(); ++i)
        {
            const auto ch = text[i];
            if (ch == L'\r' || ch == L'\n')
            {
                flush();
                if (ch == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
                {
                    ++i;
                }
                continue;
            }
            current.push_back(ch);
        }

        flush();
        return lines;
    }

    TerminalInputSnapshot TrackTerminalInput(TerminalInputState& state,
                                             std::wstring_view line,
                                             std::wstring_view knownWorkingDirectory)
    {
        if (const auto knownDirectory = _trim(knownWorkingDirectory); !knownDirectory.empty())
        {
            _setWorkingDirectory(state, knownDirectory);
        }

        const auto trimmedLine = _trim(line);
        if (!trimmedLine.empty())
        {
            const auto segments = _splitCommandSegments(trimmedLine);
            for (const auto& segment : segments)
            {
                state.LastCommand = segment;
                _applyDirectoryHeuristics(state, segment);
            }

            if (segments.empty())
            {
                state.LastCommand = trimmedLine;
            }
        }

        TerminalInputSnapshot snapshot;
        snapshot.WorkingDirectory = state.LastWorkingDirectory;
        snapshot.Command = state.LastCommand;
        snapshot.OperatingSystem = state.OperatingSystem;
        snapshot.ShellType = state.ShellType;
        return snapshot;
    }
}
