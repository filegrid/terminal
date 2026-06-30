// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/NewTabMenuEntry.h"
#include "../TerminalSettingsModel/FolderEntry.h"
#include "../TerminalSettingsModel/ProfileCollectionEntry.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/SshHostGenerator.h"
#include "../TerminalSettingsModel/resource.h"
#include "../types/inc/colorTable.hpp"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class NewTabMenuTests : public JsonTestClass
    {
        TEST_CLASS(NewTabMenuTests);

        TEST_METHOD(DefaultsToRemainingProfiles);
        TEST_METHOD(ParseEmptyFolder);
        TEST_METHOD(ResolveSshAliasCommandline);
        TEST_METHOD(ResolveSshDirectHostAndPortCommandline);
        TEST_METHOD(ShowsConfigOnlySshHostInSshFolder);
    };

    namespace
    {
        struct ScopedEnvironmentVariable
        {
            ScopedEnvironmentVariable(const wchar_t* name, const std::wstring& value) :
                _name{ name }
            {
                const auto required = GetEnvironmentVariableW(name, nullptr, 0);
                if (required != 0)
                {
                    _hadOriginal = true;
                    _original.resize(required - 1);
                    GetEnvironmentVariableW(name, _original.data(), required);
                }

                VERIFY_WIN32_BOOL_SUCCEEDED(SetEnvironmentVariableW(name, value.c_str()));
            }

            ~ScopedEnvironmentVariable()
            {
                if (_hadOriginal)
                {
                    VERIFY_WIN32_BOOL_SUCCEEDED(SetEnvironmentVariableW(_name.c_str(), _original.c_str()));
                }
                else
                {
                    VERIFY_WIN32_BOOL_SUCCEEDED(SetEnvironmentVariableW(_name.c_str(), nullptr));
                }
            }

        private:
            std::wstring _name;
            std::wstring _original;
            bool _hadOriginal{ false };
        };
    }

    void NewTabMenuTests::DefaultsToRemainingProfiles()
    {
        Log::Comment(L"If the user doesn't customize the menu, put one entry for each profile");

        static constexpr std::string_view settingsString{ R"json({
        })json" };

        try
        {
            const auto settings{ winrt::make_self<implementation::CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };

            VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());

            const auto& entries = settings->GlobalSettings().NewTabMenu();
            VERIFY_ARE_EQUAL(1u, entries.Size());
            VERIFY_ARE_EQUAL(winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntryType::RemainingProfiles, entries.GetAt(0).Type());
        }
        catch (const SettingsException& ex)
        {
            auto loadError = ex.Error();
            loadError;
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            throw e;
        }
    }

    void NewTabMenuTests::ParseEmptyFolder()
    {
        Log::Comment(L"GH #14557 - An empty folder entry shouldn't crash");

        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder" }
            ]
        })json" };

        try
        {
            const auto settings{ winrt::make_self<implementation::CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };

            VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());

            const auto& entries = settings->GlobalSettings().NewTabMenu();
            VERIFY_ARE_EQUAL(1u, entries.Size());
        }
        catch (const SettingsException& ex)
        {
            auto loadError = ex.Error();
            loadError;
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            throw e;
        }
    }

    void NewTabMenuTests::ResolveSshAliasCommandline()
    {
        Log::Comment(L"SSH commandlines that use a configured Host alias should resolve to that host's target");

        std::vector<SshHostGenerator::ConfiguredHost> configuredHosts{
            { L"devbox", L"devbox.contoso.com", 2222 },
        };

        SshHostGenerator::ConfiguredHost resolvedHost;
        VERIFY_IS_TRUE(SshHostGenerator::TryResolveCommandline(LR"(ssh devbox)", configuredHosts, resolvedHost));
        VERIFY_ARE_EQUAL(L"devbox", resolvedHost.Host.c_str());
        VERIFY_ARE_EQUAL(L"devbox.contoso.com", resolvedHost.HostName.c_str());
        VERIFY_ARE_EQUAL(2222u, static_cast<unsigned int>(resolvedHost.Port));
    }

    void NewTabMenuTests::ResolveSshDirectHostAndPortCommandline()
    {
        Log::Comment(L"SSH commandlines that directly target a host and port should resolve to that exact target");

        std::vector<SshHostGenerator::ConfiguredHost> configuredHosts{
            { L"devbox", L"devbox.contoso.com", 2222 },
        };

        SshHostGenerator::ConfiguredHost resolvedHost;
        VERIFY_IS_TRUE(SshHostGenerator::TryResolveCommandline(LR"(ssh user@devbox.contoso.com -p 2222)", configuredHosts, resolvedHost));
        VERIFY_ARE_EQUAL(L"devbox.contoso.com", resolvedHost.Host.c_str());
        VERIFY_ARE_EQUAL(L"devbox.contoso.com", resolvedHost.HostName.c_str());
        VERIFY_ARE_EQUAL(2222u, static_cast<unsigned int>(resolvedHost.Port));
    }

    void NewTabMenuTests::ShowsConfigOnlySshHostInSshFolder()
    {
        Log::Comment(L"A host coming only from .ssh/config should still appear inside the SSH submenu");

        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "remainingProfiles" },
                {
                    "type": "folder",
                    "name": "SSH",
                    "inline": "never",
                    "allowEmpty": false,
                    "entries": [
                        {
                            "type": "matchProfiles",
                            "source": "Windows.Terminal.SSH"
                        }
                    ]
                }
            ]
        })json" };

        const auto tempRoot = std::filesystem::temp_directory_path() / "wt-ssh-folder-test";
        std::filesystem::remove_all(tempRoot);
        std::filesystem::create_directories(tempRoot / ".ssh");
        const auto cleanup = wil::scope_exit([&]() {
            std::filesystem::remove_all(tempRoot);
        });

        {
            std::ofstream configFile{ tempRoot / ".ssh" / "config", std::ios::binary };
            configFile << R"(Host *
  ServerAliveInterval 30
  ServerAliveCountMax 3
Host win36501
  HostName dev.iotop.xyz
  Port 36501
  User Administrator
  IdentityFile C:\Users\Administrator\.ssh\id_iotop_win
  IdentitiesOnly yes
)";
        }

        const ScopedEnvironmentVariable userProfile{ L"USERPROFILE", tempRoot.native() };
        const ScopedEnvironmentVariable userProfileLower{ L"UserProfile", tempRoot.native() };

        const auto settings{ winrt::make_self<implementation::CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };

        auto sshProfileCount = 0u;
        for (const auto& profile : settings->ActiveProfiles())
        {
            Log::Comment(NoThrowString().Format(L"active profile name=%s source=%s hidden=%d cmd=%s",
                                                profile.Name().c_str(),
                                                profile.Source().c_str(),
                                                profile.Hidden(),
                                                profile.Commandline().c_str()));
            if (profile.Source() == L"Windows.Terminal.SSH")
            {
                ++sshProfileCount;
            }
        }

        const auto entries = settings->GlobalSettings().NewTabMenu();
        VERIFY_ARE_EQUAL(2u, entries.Size());

        auto foundSshFolder = false;
        auto matchedSshProfiles = 0u;
        for (const auto& entry : entries)
        {
            if (entry.Type() != NewTabMenuEntryType::Folder)
            {
                continue;
            }

            const auto folder = entry.as<FolderEntry>();
            if (folder.Name() != L"SSH")
            {
                continue;
            }

            foundSshFolder = true;
            Log::Comment(NoThrowString().Format(L"ssh folder entries=%u inline=%d", folder.Entries().Size(), static_cast<int>(folder.Inlining())));

            VERIFY_ARE_EQUAL(1u, folder.Entries().Size());
            const auto profileCollection = folder.Entries().GetAt(0).as<ProfileCollectionEntry>();
            matchedSshProfiles = profileCollection.Profiles().Size();
            Log::Comment(NoThrowString().Format(L"ssh match profiles=%u", matchedSshProfiles));

            for (const auto& pair : profileCollection.Profiles())
            {
                const auto profile = pair.Value();
                Log::Comment(NoThrowString().Format(L"ssh menu profile name=%s source=%s cmd=%s",
                                                    profile.Name().c_str(),
                                                    profile.Source().c_str(),
                                                    profile.Commandline().c_str()));
            }
        }

        VERIFY_IS_TRUE(foundSshFolder);
        VERIFY_ARE_EQUAL(1u, sshProfileCount);
        VERIFY_ARE_EQUAL(1u, matchedSshProfiles);
        auto foundSshProfile{ false };
        for (const auto& profile : settings->AllProfiles())
        {
            if (profile.Name() == L"SSH - win36501")
            {
                foundSshProfile = true;
                break;
            }
        }
        VERIFY_IS_TRUE(foundSshProfile);
    }
}
