// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SshHostGenerator.h"
#include "../../inc/DefaultSettings.h"

#include "DynamicProfileUtils.h"
#include <shellapi.h>

static constexpr std::wstring_view SshHostGeneratorNamespace{ L"Windows.Terminal.SSH" };

static constexpr std::wstring_view PROFILE_TITLE_PREFIX = L"SSH - ";
static constexpr std::wstring_view PROFILE_ICON_PATH = L"\uE977"; // PC1
static constexpr std::wstring_view GENERATOR_ICON_PATH = L"\uE969"; // StorageNetworkWireless

// OpenSSH is installed under System32 when installed via Optional Features
static constexpr std::wstring_view SSH_EXE_PATH1 = L"%SystemRoot%\\System32\\OpenSSH\\ssh.exe";

// OpenSSH (x86/x64) is installed under Program Files when installed via MSI
static constexpr std::wstring_view SSH_EXE_PATH2 = L"%ProgramFiles%\\OpenSSH\\ssh.exe";

// OpenSSH (x86) is installed under Program Files x86 when installed via MSI on x64 machine
static constexpr std::wstring_view SSH_EXE_PATH3 = L"%ProgramFiles(x86)%\\OpenSSH\\ssh.exe";

static constexpr std::wstring_view SSH_SYSTEM_CONFIG_PATH = L"%ProgramData%\\ssh\\ssh_config";
static constexpr std::wstring_view SSH_USER_CONFIG_PATH = L"%UserProfile%\\.ssh\\config";

static constexpr std::wstring_view SSH_CONFIG_HOST_KEY{ L"Host" };
static constexpr std::wstring_view SSH_CONFIG_HOSTNAME_KEY{ L"HostName" };
static constexpr std::wstring_view SSH_CONFIG_PORT_KEY{ L"Port" };

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static void _logSshGeneratorDebug(const std::wstring& message) noexcept
{
    const auto line = fmt::format(FMT_COMPILE(L"[WT SSH] {}\n"), message);
    OutputDebugStringW(line.c_str());

    try
    {
        std::ofstream file{ std::filesystem::temp_directory_path() / "wt-ssh-debug.log", std::ios::app | std::ios::binary };
        file << til::u16u8(line);
    }
    CATCH_LOG();
}

/*static*/ const std::wregex SshHostGenerator::_configKeyValueRegex{ LR"(^\s*(\w+)\s+([^\s]+.*[^\s])\s*$)" };

winrt::hstring _getProfileName(const std::wstring_view& hostName) noexcept
{
    return til::hstring_format(FMT_COMPILE(L"{0}{1}"), PROFILE_TITLE_PREFIX, hostName);
}

winrt::hstring _getProfileCommandLine(const std::wstring_view& sshExePath, const std::wstring_view& hostName) noexcept
{
    return til::hstring_format(FMT_COMPILE(LR"("{0}" {1})"), sshExePath, hostName);
}

bool _isSshExecutable(std::wstring_view executable) noexcept
{
    const auto filename = std::filesystem::path{ executable }.filename().native();
    return til::equals_insensitive_ascii(filename, L"ssh") || til::equals_insensitive_ascii(filename, L"ssh.exe");
}

bool _optionTakesValue(const std::wstring_view& option) noexcept
{
    if (option.size() < 2 || option[0] != L'-' || option[1] == L'-')
    {
        return false;
    }

    switch (option[1])
    {
    case L'B':
    case L'b':
    case L'c':
    case L'D':
    case L'E':
    case L'e':
    case L'F':
    case L'I':
    case L'i':
    case L'J':
    case L'L':
    case L'l':
    case L'm':
    case L'O':
    case L'o':
    case L'p':
    case L'Q':
    case L'R':
    case L'S':
    case L'W':
    case L'w':
        return true;
    default:
        return false;
    }
}

bool _tryParsePort(const std::wstring_view& value, uint16_t& port) noexcept
{
    try
    {
        if (value.empty())
        {
            return false;
        }

        const auto parsed = std::stoul(std::wstring{ value });
        if (parsed == 0 || parsed > std::numeric_limits<uint16_t>::max())
        {
            return false;
        }

        port = gsl::narrow<uint16_t>(parsed);
        return true;
    }
    CATCH_LOG();

    return false;
}

bool _tryParseSshUri(std::wstring_view destination, std::wstring& hostName, uint16_t& port) noexcept
{
    if (!til::starts_with_insensitive_ascii(destination, L"ssh://"))
    {
        return false;
    }

    destination.remove_prefix(6);

    const auto pathSeparator = destination.find_first_of(L"/?#");
    if (pathSeparator != std::wstring_view::npos)
    {
        destination = destination.substr(0, pathSeparator);
    }

    const auto userInfoSeparator = destination.rfind(L'@');
    if (userInfoSeparator != std::wstring_view::npos)
    {
        destination.remove_prefix(userInfoSeparator + 1);
    }

    if (destination.empty())
    {
        return false;
    }

    auto hostPart = destination;
    std::wstring_view portPart;

    if (destination.front() == L'[')
    {
        const auto closingBracket = destination.find(L']');
        if (closingBracket == std::wstring_view::npos)
        {
            return false;
        }

        hostPart = destination.substr(1, closingBracket - 1);
        if (closingBracket + 1 < destination.size() && destination[closingBracket + 1] == L':')
        {
            portPart = destination.substr(closingBracket + 2);
        }
    }
    else if (const auto colon = destination.rfind(L':'); colon != std::wstring_view::npos)
    {
        hostPart = destination.substr(0, colon);
        portPart = destination.substr(colon + 1);
    }

    if (hostPart.empty())
    {
        return false;
    }

    hostName.assign(hostPart);
    if (!portPart.empty())
    {
        return _tryParsePort(portPart, port);
    }

    return true;
}

bool _tryParseDestinationToken(std::wstring_view destination, std::wstring& hostName) noexcept
{
    const auto userInfoSeparator = destination.rfind(L'@');
    if (userInfoSeparator != std::wstring_view::npos)
    {
        destination.remove_prefix(userInfoSeparator + 1);
    }

    if (destination.empty())
    {
        return false;
    }

    if (destination.front() == L'[' && destination.back() == L']' && destination.size() > 2)
    {
        destination = destination.substr(1, destination.size() - 2);
    }

    hostName.assign(destination);
    return !hostName.empty();
}

bool _tryResolveSshArguments(const wil::unique_hlocal_ptr<PWSTR[]>& argv,
                            const int argc,
                            const int sshArgIndex,
                            const std::vector<SshHostGenerator::ConfiguredHost>& configuredHosts,
                            SshHostGenerator::ConfiguredHost& resolvedHost) noexcept
{
    std::wstring destination;
    uint16_t port = 22;
    bool hasExplicitPort = false;

    for (auto i = sshArgIndex + 1; i < argc; ++i)
    {
        const std::wstring_view argument{ argv[i] };
        if (argument.empty())
        {
            continue;
        }

        if (argument == L"--")
        {
            if (i + 1 < argc)
            {
                destination = argv[++i];
            }
            break;
        }

        if (argument[0] == L'-')
        {
            if (argument.starts_with(L"-p"))
            {
                std::wstring_view portValue;
                if (argument.size() > 2)
                {
                    portValue = argument.substr(2);
                }
                else if (i + 1 < argc)
                {
                    portValue = argv[++i];
                }

                uint16_t parsedPort{};
                if (_tryParsePort(portValue, parsedPort))
                {
                    port = parsedPort;
                    hasExplicitPort = true;
                }
                continue;
            }

            if (_optionTakesValue(argument) && argument.size() == 2)
            {
                if (i + 1 < argc)
                {
                    ++i;
                }
                continue;
            }

            continue;
        }

        destination = argument;
        break;
    }

    if (destination.empty())
    {
        return false;
    }

    std::wstring resolvedHostName;
    uint16_t resolvedPort = port;

    if (_tryParseSshUri(destination, resolvedHostName, resolvedPort))
    {
        resolvedHost.Host = destination;
        resolvedHost.HostName = resolvedHostName;
        resolvedHost.Port = resolvedPort;
        return true;
    }

    std::wstring destinationToken;
    if (!_tryParseDestinationToken(destination, destinationToken))
    {
        return false;
    }

    resolvedHost.Host = destinationToken;
    resolvedHost.HostName = destinationToken;
    resolvedHost.Port = resolvedPort;

    const auto found = std::find_if(configuredHosts.begin(), configuredHosts.end(), [&](const auto& host) {
        return til::equals_insensitive_ascii(host.Host, destinationToken);
    });
    if (found != configuredHosts.end())
    {
        resolvedHost = *found;
        if (hasExplicitPort)
        {
            resolvedHost.Port = resolvedPort;
        }
    }

    return true;
}

/*static*/ bool SshHostGenerator::_tryFindSshExePath(std::wstring& sshExePath) noexcept
{
    try
    {
        for (const auto& path : { SSH_EXE_PATH1, SSH_EXE_PATH2, SSH_EXE_PATH3 })
        {
            if (std::filesystem::exists(wil::ExpandEnvironmentStringsW<std::wstring>(path.data())))
            {
                sshExePath = path;
                return true;
            }
        }
    }
    CATCH_LOG();

    return false;
}

/*static*/ bool SshHostGenerator::_tryParseConfigKeyValue(const std::wstring_view& line, std::wstring& key, std::wstring& value) noexcept
{
    try
    {
        if (!line.empty() && !line.starts_with(L"#"))
        {
            std::wstring input{ line };
            std::wsmatch match;
            if (std::regex_search(input, match, SshHostGenerator::_configKeyValueRegex))
            {
                key = match[1];
                value = match[2];
                return true;
            }
        }
    }
    CATCH_LOG();

    return false;
}

/*static*/ void SshHostGenerator::_getConfiguredHostsFromConfigFile(const std::wstring_view& configPath, std::vector<ConfiguredHost>& hosts) noexcept
{
    try
    {
        const std::filesystem::path resolvedConfigPath{ wil::ExpandEnvironmentStringsW<std::wstring>(configPath.data()) };
        _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"parse-config path={} exists={}"), resolvedConfigPath.native(), std::filesystem::exists(resolvedConfigPath)));
        if (std::filesystem::exists(resolvedConfigPath))
        {
            std::wifstream inputStream(resolvedConfigPath);

            std::wstring line;
            std::wstring key;
            std::wstring value;
            ConfiguredHost currentHost{};
            bool hasHostName = false;

            const auto finalizeCurrentHost = [&]() {
                if (!currentHost.Host.empty() && hasHostName)
                {
                    _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"config-host alias={} host={} port={}"), currentHost.Host, currentHost.HostName, currentHost.Port));
                    hosts.emplace_back(currentHost);
                }

                currentHost = {};
                hasHostName = false;
            };

            while (std::getline(inputStream, line))
            {
                if (_tryParseConfigKeyValue(line, key, value))
                {
                    if (til::equals_insensitive_ascii(key, SSH_CONFIG_HOST_KEY))
                    {
                        finalizeCurrentHost();
                        currentHost.Host = value;
                    }
                    else if (til::equals_insensitive_ascii(key, SSH_CONFIG_HOSTNAME_KEY))
                    {
                        if (!currentHost.Host.empty())
                        {
                            currentHost.HostName = value;
                            hasHostName = true;
                        }
                    }
                    else if (til::equals_insensitive_ascii(key, SSH_CONFIG_PORT_KEY))
                    {
                        uint16_t parsedPort{};
                        if (!currentHost.Host.empty() && _tryParsePort(value, parsedPort))
                        {
                            currentHost.Port = parsedPort;
                        }
                    }
                }
            }

            finalizeCurrentHost();
        }
    }
    CATCH_LOG();
}

/*static*/ void SshHostGenerator::GetConfiguredHosts(std::vector<ConfiguredHost>& hosts) noexcept
{
    const auto originalCount = hosts.size();
    _getConfiguredHostsFromConfigFile(SSH_SYSTEM_CONFIG_PATH, hosts);
    _getConfiguredHostsFromConfigFile(SSH_USER_CONFIG_PATH, hosts);
    _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"configured-hosts total={} added={}"), hosts.size(), hosts.size() - originalCount));
}

/*static*/ bool SshHostGenerator::TryResolveCommandline(const std::wstring_view& commandline,
                                                        const std::vector<ConfiguredHost>& configuredHosts,
                                                        ConfiguredHost& resolvedHost) noexcept
{
    try
    {
        const std::wstring commandlineBuffer{ commandline };
        int argc = 0;
        wil::unique_hlocal_ptr<PWSTR[]> argv{ ::CommandLineToArgvW(commandlineBuffer.c_str(), &argc) };
        if (!argv || argc <= 0)
        {
            _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"resolve-cmd failed-empty cmd={}"), commandlineBuffer));
            return false;
        }

        for (auto i = 0; i < argc; ++i)
        {
            const std::wstring_view argument{ argv[i] };
            if (_isSshExecutable(argument))
            {
                const auto resolved = _tryResolveSshArguments(argv, argc, i, configuredHosts, resolvedHost);
                if (resolved)
                {
                    _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"resolve-cmd ok cmd={} alias={} host={} port={}"), commandlineBuffer, resolvedHost.Host, resolvedHost.HostName, resolvedHost.Port));
                }
                else
                {
                    _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"resolve-cmd failed-args cmd={}"), commandlineBuffer));
                }
                return resolved;
            }

            if (til::starts_with_insensitive_ascii(argument, L"ssh ") || til::starts_with_insensitive_ascii(argument, L"ssh.exe "))
            {
                return TryResolveCommandline(argument, configuredHosts, resolvedHost);
            }
        }

        _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"resolve-cmd not-ssh cmd={}"), commandlineBuffer));
        return false;
    }
    CATCH_LOG();

    return false;
}

std::wstring_view SshHostGenerator::GetNamespace() const noexcept
{
    return SshHostGeneratorNamespace;
}

std::wstring_view SshHostGenerator::GetDisplayName() const noexcept
{
    return RS_(L"SshHostGeneratorDisplayName");
}

std::wstring_view SshHostGenerator::GetIcon() const noexcept
{
    return GENERATOR_ICON_PATH;
}

// Method Description:
// - Generate a list of profiles for each detected OpenSSH host.
// Arguments:
// - <none>
// Return Value:
// - <A list of SSH host profiles.>
void SshHostGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    std::wstring sshExePath;
    if (_tryFindSshExePath(sshExePath))
    {
        std::vector<ConfiguredHost> hosts;
        GetConfiguredHosts(hosts);
        _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"generate-profiles sshExe={} hosts={}"), sshExePath, hosts.size()));

        for (const auto& host : hosts)
        {
            const auto profile{ CreateDynamicProfile(_getProfileName(host.Host)) };

            profile->Commandline(_getProfileCommandLine(sshExePath, host.Host));
            profile->Icon(winrt::hstring{ PROFILE_ICON_PATH });
            _logSshGeneratorDebug(fmt::format(FMT_COMPILE(L"generate-profile name={} cmd={}"), profile->Name(), profile->Commandline()));

            profiles.emplace_back(profile);
        }
    }
    else
    {
        _logSshGeneratorDebug(L"generate-profiles sshExe-not-found");
    }
}
