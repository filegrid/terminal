// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include <til/mutex.h>
#include <til/throttled_func.h>

#include "../TerminalSettingsModel/ApplicationState.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/Workspace.h"
#include "../../types/inc/utils.hpp"

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace WEX::Logging;

namespace SettingsModelUnitTests
{
    class WorkspaceTests
    {
        TEST_CLASS(WorkspaceTests);

        TEST_METHOD(ParseWorkspaceDirectoryYaml);
        TEST_METHOD(ParseWorkspaceDirectoryYamlIgnoresLegacyTabOrderField);
        TEST_METHOD(ParseWorkspaceDirectoryYamlUsesRootWorkspaceOrderFile);
        TEST_METHOD(ParseWorkspaceDirectoryYamlDefaultsToLocked);
        TEST_METHOD(IgnoreLegacyWorkspaceYamlFile);
        TEST_METHOD(IgnoreWorkspaceDirectoryWithoutWorkspaceYaml);
        TEST_METHOD(SanitizeWorkspaceDirectoryNames);
        TEST_METHOD(SanitizeInvalidWorkspaceDirectoryNamesOnSave);
        TEST_METHOD(PrepareWorkspaceEditorForSaveRenamesTabOrderEntries);
        TEST_METHOD(SaveWorkspaceYamlRoundTripPersistsRenamedWorkspaceOrder);
        TEST_METHOD(BuildWorkspaceStartupActions);
        TEST_METHOD(BuildWorkspaceStartupActionsForWindowsSshNode);
        TEST_METHOD(BuildWorkspaceStartupActionsForWindowsSshNodePreservesMultilineStartupAction);
        TEST_METHOD(BuildWorkspaceStartupActionsForWindowsPwshAliasNode);
        TEST_METHOD(BuildWorkspaceStartupActionsForLegacyWslNode);
        TEST_METHOD(BuildWorkspaceStartupActionsIgnoresStaleSshShellTypeForLocalProfile);
        TEST_METHOD(BuildWorkspaceStartupActionsResolvesSavedProfileNameWhenGuidMissing);
        TEST_METHOD(PrepareWorkspaceRuntimeLaunchStateInfersTransportAndMetadata);
        TEST_METHOD(PrepareWorkspaceRuntimeLaunchStateDefaultsToLinuxWithoutMetadata);
        TEST_METHOD(ResolveWorkspaceNodeLaunchResolutionPrefersObservedValues);
        TEST_METHOD(ResolveWorkspaceNodeLaunchResolutionDefaultsToLinuxWithoutMetadata);
        TEST_METHOD(ResolveTrackedWorkspaceDirectoryPrefersReportedPathForSsh);
        TEST_METHOD(IsWorkspaceDirtyUsesBaselineAndPersistedFallback);
        TEST_METHOD(PrepareWorkspaceEditorForSavePreservesSelectedIndex);
        TEST_METHOD(PrepareWorkspaceDefinitionRemovalResolvesEditorState);
        TEST_METHOD(ResolveWorkspaceOpenExecutionPlanMapsRuntimeSteps);
        TEST_METHOD(WorkspaceLiveTabHelpersCaptureAndResolveSnapshots);
        TEST_METHOD(BuildWorkspaceStartupActionsAssignsDistinctColorsForDuplicateProfiles);
        TEST_METHOD(BuildWorkspaceStartupActionsSkipsHiddenNodes);
        TEST_METHOD(ResolveWorkspaceStartupStateSkipsInvalidProfileNodes);
        TEST_METHOD(SaveWorkspaceYamlRoundTrip);
        TEST_METHOD(SaveUnlockedWorkspaceYamlRoundTrip);
        TEST_METHOD(ApplyVisibleWorkspaceNodeOrderPreservesNodeIdentity);
        TEST_METHOD(ReorderWorkspaceNodesPreservesNodeContent);
        TEST_METHOD(ParseWorkspaceWindowStateYaml);
        TEST_METHOD(SaveWorkspaceWindowStateYamlRoundTrip);
        TEST_METHOD(PersistWorkspaceStateInApplicationState);

    private:
        struct TempPath
        {
            TempPath()
            {
                GUID guid{};
                THROW_IF_FAILED(CoCreateGuid(&guid));
                path = std::filesystem::temp_directory_path() / (L"wt-workspace-test-" + Utils::GuidToString(guid));
            }

            ~TempPath()
            {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
            }

            std::filesystem::path path;
        };
    };

    void WorkspaceTests::ParseWorkspaceDirectoryYaml()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path / L"Dev Workspace" / L"App 1");

        static constexpr std::string_view workspaceYaml{
            "version: 1\n"
            "description: 'daily development'\n"
            "backgroundColor: '#4682B4'\n"
            "locked: true\n"
            "tabOrder: |\n"
            "  App 1\n"
        };
        static constexpr std::string_view tabYaml{
            "version: 1\n"
            "connectionRef: 'peer-app'\n"
            "profileGuid: '{00000000-0000-0000-0000-000000000101}'\n"
            "profileName: 'Ubuntu'\n"
            "tabColor: '#445566'\n"
            "showTab: false\n"
            "startupDirectory: 'D:\\work\\app'\n"
            "startupAction: '.\\bootstrap.ps1'\n"
            "operatingSystem: 'windows'\n"
            "shellType: 'powershell'\n"
            "showInputPanel: true\n"
            "useNodeNameAsTabTitle: false\n"
        };

        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"workspace.yaml", std::ios::binary | std::ios::trunc };
            output.write(workspaceYaml.data(), gsl::narrow_cast<std::streamsize>(workspaceYaml.size()));
        }
        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"App 1" / L"tab.yaml", std::ios::binary | std::ios::trunc };
            output.write(tabYaml.data(), gsl::narrow_cast<std::streamsize>(tabYaml.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(manager.Workspaces().size()));

        const auto& workspace = manager.Workspaces().front();
        VERIFY_IS_TRUE(workspace.Id == L"Dev Workspace");
        VERIFY_IS_TRUE(workspace.Name == L"Dev Workspace");
        VERIFY_IS_TRUE(workspace.BackgroundColor == L"#4682B4");
        VERIFY_IS_TRUE(workspace.Locked);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(workspace.Nodes.size()));
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(workspace.TabOrder.size()));
        VERIFY_IS_TRUE(workspace.TabOrder.front() == L"App 1");
        VERIFY_IS_TRUE(workspace.Nodes.front().Id == L"App 1");
        VERIFY_IS_TRUE(workspace.Nodes.front().ProfileName == L"Ubuntu");
        VERIFY_IS_TRUE(workspace.Nodes.front().StartupDirectory == L"D:\\work\\app");
        VERIFY_IS_TRUE(workspace.Nodes.front().TabColor == L"#445566");
        VERIFY_IS_FALSE(workspace.Nodes.front().ShowTab);
        VERIFY_IS_TRUE(workspace.Nodes.front().OperatingSystem == L"windows");
        VERIFY_IS_TRUE(workspace.Nodes.front().ShellType == L"powershell");
        VERIFY_IS_TRUE(workspace.Nodes.front().ShowInputPanel);
        VERIFY_IS_FALSE(workspace.Nodes.front().UseNodeNameAsTabTitle);
    }

    void WorkspaceTests::ParseWorkspaceDirectoryYamlIgnoresLegacyTabOrderField()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path / L"Dev Workspace" / L"B Tab");
        std::filesystem::create_directories(temp.path / L"Dev Workspace" / L"A Tab");

        static constexpr std::string_view workspaceYaml{
            "version: 1\n"
        };
        static constexpr std::string_view firstTabYaml{
            "version: 1\n"
            "order: 99\n"
            "profileGuid: '{00000000-0000-0000-0000-000000000101}'\n"
        };
        static constexpr std::string_view secondTabYaml{
            "version: 1\n"
            "order: 0\n"
            "profileGuid: '{00000000-0000-0000-0000-000000000202}'\n"
        };

        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"workspace.yaml", std::ios::binary | std::ios::trunc };
            output.write(workspaceYaml.data(), gsl::narrow_cast<std::streamsize>(workspaceYaml.size()));
        }
        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"B Tab" / L"tab.yaml", std::ios::binary | std::ios::trunc };
            output.write(firstTabYaml.data(), gsl::narrow_cast<std::streamsize>(firstTabYaml.size()));
        }
        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"A Tab" / L"tab.yaml", std::ios::binary | std::ios::trunc };
            output.write(secondTabYaml.data(), gsl::narrow_cast<std::streamsize>(secondTabYaml.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(manager.Workspaces().size()));
        const auto& workspace = manager.Workspaces().front();
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(workspace.Nodes.size()));
        VERIFY_IS_TRUE(workspace.Nodes.at(0).Name == L"A Tab");
        VERIFY_IS_TRUE(workspace.Nodes.at(0).Id == L"A Tab");
        VERIFY_IS_TRUE(workspace.Nodes.at(1).Name == L"B Tab");
        VERIFY_IS_TRUE(workspace.Nodes.at(1).Id == L"B Tab");
    }

    void WorkspaceTests::ParseWorkspaceDirectoryYamlUsesRootWorkspaceOrderFile()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path / L"B Workspace" / L"Tab");
        std::filesystem::create_directories(temp.path / L"A Workspace" / L"Tab");

        static constexpr std::string_view workspaceYaml{
            "version: 1\n"
        };
        static constexpr std::string_view tabYaml{
            "version: 1\n"
            "profileGuid: '{00000000-0000-0000-0000-000000000101}'\n"
        };
        static constexpr std::string_view orderYaml{
            "version: 1\n"
            "workspaces: |\n"
            "  B Workspace\n"
            "  A Workspace\n"
        };

        for (const auto& workspaceName : { L"B Workspace", L"A Workspace" })
        {
            {
                std::ofstream output{ temp.path / workspaceName / L"workspace.yaml", std::ios::binary | std::ios::trunc };
                output.write(workspaceYaml.data(), gsl::narrow_cast<std::streamsize>(workspaceYaml.size()));
            }
            {
                std::ofstream output{ temp.path / workspaceName / L"Tab" / L"tab.yaml", std::ios::binary | std::ios::trunc };
                output.write(tabYaml.data(), gsl::narrow_cast<std::streamsize>(tabYaml.size()));
            }
        }
        {
            std::ofstream output{ temp.path / L"workspaces.yaml", std::ios::binary | std::ios::trunc };
            output.write(orderYaml.data(), gsl::narrow_cast<std::streamsize>(orderYaml.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(manager.Workspaces().size()));
        VERIFY_IS_TRUE(manager.Workspaces().at(0).Name == L"B Workspace");
        VERIFY_IS_TRUE(manager.Workspaces().at(1).Name == L"A Workspace");
    }

    void WorkspaceTests::BuildWorkspaceStartupActions()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"App 1";
        const auto expectedProfileGuid = Utils::GuidToString(profile.Guid());
        const auto expectedDirectory = std::wstring{ L"D:\\work\\app" };
        node.ProfileGuid = expectedProfileGuid;
        node.StartupDirectory = expectedDirectory;
        node.StartupAction = L".\\bootstrap.ps1";
        node.ShowInputPanel = true;
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto newTabArgs = actions.at(0).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(newTabArgs);
        const auto terminalArgs = newTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(terminalArgs);
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.Profile() } == expectedProfileGuid);
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.StartingDirectory() } == expectedDirectory);
        VERIFY_IS_TRUE(terminalArgs.SuppressApplicationTitle().Value());

        const auto sendInputArgs = actions.at(1).Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(sendInputArgs);
        VERIFY_IS_TRUE(std::wstring{ sendInputArgs.Input() } == L".\\bootstrap.ps1\r");
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsForWindowsSshNode()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "ssh dev@box"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-ssh";
        workspace.Name = L"SSH Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"Windows Remote";
        node.ProfileGuid = Utils::GuidToString(profile.Guid());
        node.StartupDirectory = L"E:\\tools";
        node.StartupAction = L"git status";
        node.OperatingSystem = L"windows";
        node.ShellType = L"powershell";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto newTabArgs = actions.at(0).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(newTabArgs);
        const auto terminalArgs = newTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(terminalArgs);
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.Profile() } == Utils::GuidToString(profile.Guid()));
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.StartingDirectory() }.empty());

        const auto sendInputArgs = actions.at(1).Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(sendInputArgs);
        VERIFY_IS_TRUE(std::wstring{ sendInputArgs.Input() } == L"Set-Location -LiteralPath 'E:\\tools'\rgit status\r");
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsForWindowsSshNodePreservesMultilineStartupAction()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "ssh -t dev@box"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-ssh";
        workspace.Name = L"SSH Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"Windows Remote";
        node.ProfileGuid = Utils::GuidToString(profile.Guid());
        node.StartupDirectory = L"E:\\tools";
        node.StartupAction = L"if ($true) {\n    'ok' >> .\\out.txt\n}";
        node.OperatingSystem = L"windows";
        node.ShellType = L"powershell";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto sendInputArgs = actions.at(1).Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(sendInputArgs);
        VERIFY_IS_TRUE(std::wstring{ sendInputArgs.Input() } == L"Set-Location -LiteralPath 'E:\\tools'\rif ($true) {\n    'ok' >> .\\out.txt\n}\r");
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsForLegacyWslNode()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "Ubuntu",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "C:\\Windows\\System32\\wsl.exe -d Ubuntu"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-wsl";
        workspace.Name = L"WSL Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"Ubuntu";
        node.ProfileGuid = Utils::GuidToString(profile.Guid());
        node.StartupDirectory = L"/app";
        node.StartupAction = L"pwd";
        node.OperatingSystem = L"linux";
        node.ShellType = L"ssh";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto newTabArgs = actions.at(0).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(newTabArgs);
        const auto terminalArgs = newTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(terminalArgs);
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.Profile() } == Utils::GuidToString(profile.Guid()));
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.StartingDirectory() }.empty());

        const auto sendInputArgs = actions.at(1).Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(sendInputArgs);
        VERIFY_IS_TRUE(std::wstring{ sendInputArgs.Input() } == L"cd \"/app\"\rpwd\r");
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsForWindowsPwshAliasNode()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "ssh dev@box"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-ssh";
        workspace.Name = L"SSH Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"Windows Remote";
        node.ProfileGuid = Utils::GuidToString(profile.Guid());
        node.StartupDirectory = L"D:\\workspace";
        node.StartupAction = L"copilot --allow-all";
        node.OperatingSystem = L"windows";
        node.ShellType = L"pwsh";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto sendInputArgs = actions.at(1).Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(sendInputArgs);
        VERIFY_IS_TRUE(std::wstring{ sendInputArgs.Input() } == L"Set-Location -LiteralPath 'D:\\workspace'\rcopilot --allow-all\r");
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsIgnoresStaleSshShellTypeForLocalProfile()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "PowerShell",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "pwsh.exe"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-local";
        workspace.Name = L"Local Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"t2";
        node.ProfileGuid = Utils::GuidToString(profile.Guid());
        node.StartupDirectory = L"E:\\";
        node.StartupAction = L"copilot --allow-all";
        node.OperatingSystem = L"windows";
        node.ShellType = L"ssh";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto newTabArgs = actions.at(0).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(newTabArgs);
        const auto terminalArgs = newTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(terminalArgs);
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.Profile() } == Utils::GuidToString(profile.Guid()));
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.StartingDirectory() } == L"E:\\");

        const auto sendInputArgs = actions.at(1).Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(sendInputArgs);
        VERIFY_IS_TRUE(std::wstring{ sendInputArgs.Input() } == L"copilot --allow-all\r");
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsResolvesSavedProfileNameWhenGuidMissing()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "Ubuntu",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "wsl.exe -d Ubuntu"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-linux";
        workspace.Name = L"Linux Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"Ubuntu";
        node.ProfileGuid = L"{00000000-0000-0000-0000-000000000000}";
        node.ProfileName = L"Ubuntu";
        node.StartupDirectory = L"/repo";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto newTabArgs = actions.at(0).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(newTabArgs);
        const auto terminalArgs = newTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(terminalArgs);
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.Profile() } == Utils::GuidToString(profile.Guid()));
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.StartingDirectory() } == L"/repo");
    }

    void WorkspaceTests::PrepareWorkspaceRuntimeLaunchStateInfersTransportAndMetadata()
    {
        const auto state = PrepareWorkspaceRuntimeLaunchState(L"D:\\repo",
                                                              L"Windows.Terminal.SSH",
                                                              L"ssh dev@box",
                                                              L"ssh -tt dev@box");

        VERIFY_IS_TRUE(state.IsSshTransport);
        VERIFY_IS_TRUE(state.HasSshTtyOption);
        VERIFY_IS_TRUE(state.ExplicitCommandline == L"ssh -tt dev@box");
        VERIFY_IS_TRUE(state.StartingDirectory.empty());
        VERIFY_IS_TRUE(state.ShellType == L"ssh");
        VERIFY_IS_TRUE(state.OperatingSystem == L"linux");
    }

    void WorkspaceTests::PrepareWorkspaceRuntimeLaunchStateDefaultsToLinuxWithoutMetadata()
    {
        const auto state = PrepareWorkspaceRuntimeLaunchState(L"D:\\repo",
                                                              L"",
                                                              L"",
                                                              L"custom-shell");

        VERIFY_IS_FALSE(state.IsSshTransport);
        VERIFY_IS_FALSE(state.HasSshTtyOption);
        VERIFY_IS_TRUE(state.StartingDirectory.empty());
        VERIFY_IS_TRUE(state.OperatingSystem == L"linux");
        VERIFY_IS_TRUE(state.ShellType.empty());
    }

    void WorkspaceTests::ResolveWorkspaceNodeLaunchResolutionPrefersObservedValues()
    {
        WorkspaceNode persistedNode;
        persistedNode.StartupAction = L"persisted-start";
        persistedNode.StartupDirectory = L"C:\\persisted";
        persistedNode.OperatingSystem = L"windows";
        persistedNode.ShellType = L"powershell";

        WorkspaceNodeLaunchResolutionInput input;
        input.PersistedNode = persistedNode;
        input.ObservedStartupAction = L"captured-start";
        input.ObservedWorkingDirectory = L"D:\\live";
        input.ObservedOperatingSystem = L"linux";
        input.ObservedShellType = L"ssh";
        input.RuntimeStartupAction = L"runtime-start";
        input.RuntimeExplicitCommandline = L"runtime-explicit";
        input.RuntimeStartingDirectory = L"E:\\runtime";
        input.RuntimeOperatingSystem = L"windows";
        input.RuntimeShellType = L"cmd";
        input.ProfileSource = L"";
        input.ProfileCommandline = L"pwsh.exe";
        input.TerminalCommandline = L"pwsh.exe";
        input.TerminalStartingDirectory = L"C:\\args";

        const auto resolution = ResolveWorkspaceNodeLaunchResolution(input);
        VERIFY_IS_TRUE(resolution.StartupAction == L"captured-start");
        VERIFY_IS_TRUE(resolution.StartingDirectory == L"D:\\live");
        VERIFY_IS_TRUE(resolution.OperatingSystem == L"linux");
        VERIFY_IS_TRUE(resolution.ShellType == L"ssh");
    }

    void WorkspaceTests::ResolveWorkspaceNodeLaunchResolutionDefaultsToLinuxWithoutMetadata()
    {
        WorkspaceNodeLaunchResolutionInput input;
        input.ProfileSource = L"";
        input.ProfileCommandline = L"custom-shell";
        input.TerminalCommandline = L"custom-shell";
        input.TerminalStartingDirectory = L"D:\\args";

        const auto resolution = ResolveWorkspaceNodeLaunchResolution(input);
        VERIFY_IS_TRUE(resolution.StartingDirectory.empty());
        VERIFY_IS_TRUE(resolution.OperatingSystem == L"linux");
        VERIFY_IS_TRUE(resolution.ShellType.empty());
    }

    void WorkspaceTests::ResolveTrackedWorkspaceDirectoryPrefersReportedPathForSsh()
    {
        WorkspaceTrackedDirectoryInput input;
        input.ReportedWorkingDirectory = L"/home/dev/project";
        input.ProcessWorkingDirectory = L"C:\\process";
        input.RuntimeStartingDirectory = L"C:\\start";
        input.RuntimeOperatingSystem = L"linux";
        input.RuntimeShellType = L"ssh";
        input.IsSshTransport = true;

        VERIFY_IS_TRUE(ResolveTrackedWorkspaceDirectory(input) == L"/home/dev/project");

        input.IsSshTransport = false;
        input.RuntimeOperatingSystem = L"windows";
        input.RuntimeShellType = L"powershell";
        VERIFY_IS_TRUE(ResolveTrackedWorkspaceDirectory(input) == L"C:\\process");
    }

    void WorkspaceTests::IsWorkspaceDirtyUsesBaselineAndPersistedFallback()
    {
        Workspace captured;
        captured.Id = L"ws";
        captured.Name = L"Workspace";
        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"Node 1";
        captured.Nodes.push_back(node);

        Workspace baseline = captured;
        VERIFY_IS_FALSE(IsWorkspaceDirty(captured, L"ws", baseline, std::nullopt));

        baseline.Nodes.front().StartupAction = L"changed";
        VERIFY_IS_TRUE(IsWorkspaceDirty(captured, L"ws", baseline, std::nullopt));

        Workspace persisted = captured;
        VERIFY_IS_FALSE(IsWorkspaceDirty(captured, L"ws", std::nullopt, persisted));

        persisted.Nodes.front().StartupDirectory = L"D:\\other";
        VERIFY_IS_TRUE(IsWorkspaceDirty(captured, L"ws", std::nullopt, persisted));
        VERIFY_IS_TRUE(IsWorkspaceDirty(captured, L"", std::nullopt, std::nullopt));
    }

    void WorkspaceTests::PrepareWorkspaceEditorForSavePreservesSelectedIndex()
    {
        Workspace persistedWorkspace;
        persistedWorkspace.Id = L"persisted";
        persistedWorkspace.Name = L"Persisted";
        WorkspaceManager persistedManager;
        persistedManager.SetWorkspaces({ persistedWorkspace });

        Workspace first;
        first.Id = L"one";
        first.Name = L"One";
        Workspace second;
        second.Id = L"two";
        second.Name = L"Two";
        WorkspaceManager editedManager;
        editedManager.SetWorkspaces({ first, second });

        const auto plan = PrepareWorkspaceEditorForSave(editedManager, persistedManager, L"", L"", 1);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(plan.SelectedWorkspaceIndex));
    }

    void WorkspaceTests::PrepareWorkspaceDefinitionRemovalResolvesEditorState()
    {
        Workspace one;
        one.Id = L"one";
        one.Name = L"One";

        Workspace two;
        two.Id = L"two";
        two.Name = L"Two";
        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"Node 1";
        two.Nodes.push_back(node);

        WorkspaceManager manager;
        manager.SetWorkspaces({ one, two });

        const auto plan = PrepareWorkspaceDefinitionRemoval(manager, L"one", L"one", L"two", 0, 1000, L"one");
        VERIFY_IS_TRUE(plan.has_value());
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(manager.Workspaces().size()));
        VERIFY_IS_TRUE(plan->LastOpenedWorkspaceExists == false);
        VERIFY_ARE_EQUAL(0u, gsl::narrow_cast<unsigned int>(plan->SelectedWorkspaceIndex));
        VERIFY_ARE_EQUAL(1000, plan->NavSelection);
    }

    void WorkspaceTests::ResolveWorkspaceOpenExecutionPlanMapsRuntimeSteps()
    {
        WorkspaceOpenPlan openPlan;
        openPlan.Disposition = WorkspaceOpenDisposition::ReplaceCurrentWindow;
        openPlan.ConfirmSaveCurrentWorkspace = true;

        const auto executionPlan = ResolveWorkspaceOpenExecutionPlan(openPlan, true, true);
        VERIFY_IS_TRUE(executionPlan.Disposition == WorkspaceOpenExecutionDisposition::ReplaceCurrentWindow);
        VERIFY_IS_TRUE(executionPlan.ConfirmSaveCurrentWorkspace);
        VERIFY_IS_TRUE(executionPlan.SetLastOpenedWorkspaceId);
        VERIFY_IS_TRUE(executionPlan.SetSaveBaseline);
        VERIFY_IS_TRUE(executionPlan.SetCurrentWorkspaceBeforeActions);
        VERIFY_IS_TRUE(executionPlan.ReplacePendingNodeQueues);
        VERIFY_IS_TRUE(executionPlan.FocusActiveContentAfterActions);
        VERIFY_IS_TRUE(executionPlan.RemoveCapturedTabsAfterActions);
        VERIFY_IS_TRUE(executionPlan.SetCurrentWorkspaceAfterActions);
    }

    void WorkspaceTests::WorkspaceLiveTabHelpersCaptureAndResolveSnapshots()
    {
        Workspace workspace;
        workspace.Id = L"ws";
        workspace.Name = L"Workspace";

        WorkspaceNode first;
        first.Id = L"node-1";
        first.Name = L"Node 1";
        workspace.Nodes.push_back(first);

        WorkspaceNode second;
        second.Id = L"node-2";
        second.Name = L"Node 2";
        workspace.Nodes.push_back(second);

        WorkspaceLiveTabCaptureState captureState;
        captureState.LiveTabTitle = L"Live";
        captureState.GeneratedNodeName = L"Generated";
        captureState.ProfileGuid = L"{00000000-0000-0000-0000-000000000001}";
        captureState.LaunchResolution.StartupAction = L"echo hi";
        captureState.LaunchResolution.StartingDirectory = L"D:\\repo";
        captureState.LaunchResolution.OperatingSystem = L"windows";
        captureState.LaunchResolution.ShellType = L"powershell";
        captureState.ShowInputPanel = true;
        captureState.TabColor = L"#123456";

        const auto capturedNode = BuildWorkspaceCapturedNode(captureState);
        VERIFY_IS_TRUE(capturedNode.Name == L"Live");
        VERIFY_IS_TRUE(capturedNode.Id == L"Live");
        VERIFY_IS_TRUE(capturedNode.StartupDirectory == L"D:\\repo");
        VERIFY_IS_TRUE(capturedNode.TabColor == L"#123456");

        std::vector<WorkspaceLiveTabSnapshot> tabs;
        tabs.push_back(WorkspaceLiveTabSnapshot{ .LoadsWorkspaceNode = true, .RuntimeNodeId = L"node-2" });
        tabs.push_back(WorkspaceLiveTabSnapshot{ .LoadsWorkspaceNode = true, .RuntimeNodeId = L"" });

        const auto firstNodeIndex = ResolveWorkspaceBackedTabIndex(workspace, tabs, 0);
        VERIFY_IS_TRUE(firstNodeIndex.has_value());
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(firstNodeIndex.value()));

        const auto secondNode = ResolveWorkspaceBackedTabNode(workspace, tabs, 1);
        VERIFY_IS_TRUE(secondNode.has_value());
        VERIFY_IS_TRUE(secondNode->Id == L"node-2");

        const auto tabIndex = FindWorkspaceBackedTabSnapshotIndex(workspace, tabs, 1);
        VERIFY_IS_TRUE(tabIndex.has_value());
        VERIFY_ARE_EQUAL(0u, gsl::narrow_cast<unsigned int>(tabIndex.value()));
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsAssignsDistinctColorsForDuplicateProfiles()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe",
                    "tabColor": "#445566"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";

        WorkspaceNode first;
        first.Id = L"node-1";
        first.Name = L"App 1";
        first.ProfileGuid = Utils::GuidToString(profile.Guid());
        workspace.Nodes.emplace_back(first);

        WorkspaceNode second;
        second.Id = L"node-2";
        second.Name = L"App 2";
        second.ProfileGuid = Utils::GuidToString(profile.Guid());
        workspace.Nodes.emplace_back(second);

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto firstTabArgs = actions.at(0).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(firstTabArgs);
        const auto firstTerminalArgs = firstTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(firstTerminalArgs);
        VERIFY_IS_NOT_NULL(firstTerminalArgs.TabColor());
        VERIFY_ARE_EQUAL(til::color{ 0x445566 }, til::color{ firstTerminalArgs.TabColor().Value() });

        const auto secondTabArgs = actions.at(1).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(secondTabArgs);
        const auto secondTerminalArgs = secondTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(secondTerminalArgs);
        VERIFY_IS_NOT_NULL(secondTerminalArgs.TabColor());
        VERIFY_IS_TRUE(til::color{ firstTerminalArgs.TabColor().Value() } != til::color{ secondTerminalArgs.TabColor().Value() });
    }

    void WorkspaceTests::BuildWorkspaceStartupActionsSkipsHiddenNodes()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";

        WorkspaceNode first;
        first.Id = L"node-1";
        first.Name = L"Shown";
        first.ProfileGuid = Utils::GuidToString(profile.Guid());
        first.ShowTab = true;
        workspace.Nodes.emplace_back(first);

        WorkspaceNode second;
        second.Id = L"node-2";
        second.Name = L"Hidden";
        second.ProfileGuid = Utils::GuidToString(profile.Guid());
        second.ShowTab = false;
        workspace.Nodes.emplace_back(second);

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        const auto actions = manager.BuildStartupActions(manager.Workspaces().front(), *settings);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(actions.size()));

        const auto newTabArgs = actions.at(0).Args().try_as<NewTabArgs>();
        VERIFY_IS_NOT_NULL(newTabArgs);
        const auto terminalArgs = newTabArgs.ContentArgs().try_as<NewTerminalArgs>();
        VERIFY_IS_NOT_NULL(terminalArgs);
        VERIFY_IS_TRUE(std::wstring{ terminalArgs.Profile() } == Utils::GuidToString(profile.Guid()));
    }

    void WorkspaceTests::ResolveWorkspaceStartupStateSkipsInvalidProfileNodes()
    {
        static constexpr std::string_view settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": { "list": [
                {
                    "name": "PowerShell",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                }
            ] }
        })" };

        const auto settings = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);

        Workspace workspace;
        workspace.Id = L"ws-startup";
        workspace.Name = L"Startup Workspace";

        WorkspaceNode validNode;
        validNode.Id = L"node-valid";
        validNode.Name = L"Valid";
        validNode.ProfileGuid = Utils::GuidToString(profile.Guid());
        validNode.ShowInputPanel = true;

        WorkspaceNode invalidNode;
        invalidNode.Id = L"node-invalid";
        invalidNode.Name = L"Invalid";
        invalidNode.ProfileGuid = L"{11111111-1111-1111-1111-111111111111}";
        invalidNode.ProfileName = L"Missing Profile";
        invalidNode.ShowInputPanel = false;

        workspace.Nodes = { validNode, invalidNode };
        workspace.TabOrder = { invalidNode.Id, validNode.Id };

        const auto startupState = ResolveWorkspaceStartupState(workspace, *settings);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(startupState.PendingNodeIds.size()));
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(startupState.PendingNodeInputVisibility.size()));
        VERIFY_IS_TRUE(startupState.PendingNodeIds.at(0) == validNode.Id);
        VERIFY_IS_TRUE(startupState.PendingNodeInputVisibility.at(0));
    }

    void WorkspaceTests::ParseWorkspaceDirectoryYamlDefaultsToLocked()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path / L"Dev Workspace" / L"node-1");

        static constexpr std::string_view workspaceYaml{
            "version: 1\n"
        };
        static constexpr std::string_view tabYaml{
            "version: 1\n"
            "id: 'node-1'\n"
            "profileGuid: '{00000000-0000-0000-0000-000000000101}'\n"
        };

        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"workspace.yaml", std::ios::binary | std::ios::trunc };
            output.write(workspaceYaml.data(), gsl::narrow_cast<std::streamsize>(workspaceYaml.size()));
        }
        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"node-1" / L"tab.yaml", std::ios::binary | std::ios::trunc };
            output.write(tabYaml.data(), gsl::narrow_cast<std::streamsize>(tabYaml.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(manager.Workspaces().size()));
        VERIFY_IS_TRUE(manager.Workspaces().front().Locked);
    }

    void WorkspaceTests::IgnoreLegacyWorkspaceYamlFile()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path);

        static constexpr std::string_view yaml{
            "version: 1\n"
            "workspaces:\n"
            "  - id: 'ws-dev'\n"
            "    name: 'Dev Workspace'\n"
            "    nodes:\n"
            "      - id: 'node-1'\n"
            "        profileGuid: '{00000000-0000-0000-0000-000000000101}'\n"
        };

        {
            std::ofstream output{ temp.path / L"workspaces.yaml", std::ios::binary | std::ios::trunc };
            output.write(yaml.data(), gsl::narrow_cast<std::streamsize>(yaml.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_IS_TRUE(manager.Workspaces().empty());
    }

    void WorkspaceTests::IgnoreWorkspaceDirectoryWithoutWorkspaceYaml()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path / L"ceshi" / L"GitHub Copilot" / L"terminal");

        {
            std::ofstream output{ temp.path / L"ceshi" / L"GitHub Copilot" / L"terminal" / L"2026-07-16.jsonl", std::ios::binary | std::ios::trunc };
            static constexpr std::string_view diagnostics{ "{\"event\":\"workspace_chat\"}\n" };
            output.write(diagnostics.data(), gsl::narrow_cast<std::streamsize>(diagnostics.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_IS_TRUE(manager.Workspaces().empty());
    }

    void WorkspaceTests::SanitizeWorkspaceDirectoryNames()
    {
        VERIFY_ARE_EQUAL(std::wstring{ L"Dev Workspace" }, SanitizeWorkspaceDirectoryName(L"Dev Workspace"));
        VERIFY_ARE_EQUAL(std::wstring{ L"bad_name" }, SanitizeWorkspaceDirectoryName(L"bad:name"));
        VERIFY_ARE_EQUAL(std::wstring{ L"CON_" }, SanitizeWorkspaceDirectoryName(L"CON"));
        VERIFY_ARE_EQUAL(std::wstring{ L"node" }, SanitizeWorkspaceDirectoryName(L"node."));
        VERIFY_ARE_EQUAL(std::wstring{ L"node" }, SanitizeWorkspaceDirectoryName(L" node", L"node"));
    }

    void WorkspaceTests::SanitizeInvalidWorkspaceDirectoryNamesOnSave()
    {
        TempPath temp;

        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"bad:name";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"App 1";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });
        VERIFY_IS_TRUE(manager.SaveToPath(temp.path));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"bad_name" / L"workspace.yaml"));

        workspace.Name = L"Dev Workspace";
        workspace.Nodes.front().Name = L"CON";
        manager.SetWorkspaces({ workspace });
        VERIFY_IS_TRUE(manager.SaveToPath(temp.path));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"Dev Workspace" / L"CON_" / L"tab.yaml"));
    }

    void WorkspaceTests::PrepareWorkspaceEditorForSaveRenamesTabOrderEntries()
    {
        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"bad:name";
        workspace.Nodes.emplace_back(std::move(node));
        workspace.TabOrder = { L"bad:name" };

        WorkspaceEditorState editor;
        editor.Workspace = workspace;

        const auto prepared = PrepareWorkspaceEditorForSave(editor);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(prepared.Workspace.TabOrder.size()));
        VERIFY_IS_TRUE(prepared.Workspace.TabOrder.front() == L"bad_name");
        VERIFY_IS_TRUE(prepared.Workspace.Nodes.front().Name == L"bad_name");
        VERIFY_IS_TRUE(prepared.Workspace.Nodes.front().Id == L"bad_name");
    }

    void WorkspaceTests::SaveWorkspaceYamlRoundTripPersistsRenamedWorkspaceOrder()
    {
        TempPath temp;

        Workspace first;
        first.Id = L"workspace-a";
        first.Name = L"bad:name";
        WorkspaceNode firstNode;
        firstNode.Id = L"node-1";
        firstNode.ProfileGuid = L"{00000000-0000-0000-0000-000000000101}";
        first.Nodes.emplace_back(std::move(firstNode));

        Workspace second;
        second.Id = L"workspace-b";
        second.Name = L"Second";
        WorkspaceNode secondNode;
        secondNode.Id = L"node-2";
        secondNode.ProfileGuid = L"{00000000-0000-0000-0000-000000000202}";
        second.Nodes.emplace_back(std::move(secondNode));

        WorkspaceManager manager;
        manager.SetWorkspaces({ first, second });

        VERIFY_IS_TRUE(manager.SaveToPath(temp.path));
        {
            std::ifstream input{ temp.path / L"workspaces.yaml", std::ios::binary };
            const std::string content{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
            VERIFY_IS_TRUE(content.find("bad_name") != std::string::npos);
            VERIFY_IS_TRUE(content.find("bad:name") == std::string::npos);
            VERIFY_IS_TRUE(content.find("Second") != std::string::npos);
        }
    }

    void WorkspaceTests::SaveWorkspaceYamlRoundTrip()
    {
        TempPath temp;

        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";
        workspace.Description = L"daily development";
        workspace.BackgroundColor = L"#4682B4";
        workspace.Locked = true;

        WorkspaceNode node;
        node.Id = L"node-1";
        node.Name = L"App 1";
        node.ConnectionRef = L"peer-app";
        node.ProfileGuid = L"{00000000-0000-0000-0000-000000000101}";
        node.ProfileName = L"Ubuntu";
        node.TabColor = L"#445566";
        node.ShowTab = false;
        node.StartupDirectory = L"D:\\work\\app";
        node.StartupAction = L"if ($true) {\n    'ok' >> .\\bootstrap.log\n}";
        node.OperatingSystem = L"windows";
        node.ShellType = L"powershell";
        node.ShowInputPanel = true;
        node.UseNodeNameAsTabTitle = false;
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });
        manager.Workspaces().front().TabOrder = { L"App 1" };

        VERIFY_IS_TRUE(manager.SaveToPath(temp.path));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"Dev Workspace" / L"workspace.yaml"));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"Dev Workspace" / L"App 1" / L"tab.yaml"));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"workspaces.yaml"));
        {
            std::ifstream input{ temp.path / L"Dev Workspace" / L"workspace.yaml", std::ios::binary };
            const std::string content{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
            VERIFY_IS_TRUE(content.rfind("order:", 0) != 0 && content.find("\norder:") == std::string::npos);
            VERIFY_IS_TRUE(content.find("name:") == std::string::npos);
            VERIFY_IS_TRUE(content.rfind("id:", 0) != 0 && content.find("\nid:") == std::string::npos);
            VERIFY_IS_TRUE(content.find("tabOrder:") != std::string::npos);
        }
        {
            std::ifstream input{ temp.path / L"workspaces.yaml", std::ios::binary };
            const std::string content{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
            VERIFY_IS_TRUE(content.find("workspaces:") != std::string::npos);
            VERIFY_IS_TRUE(content.find("Dev Workspace") != std::string::npos);
        }
        {
            std::ifstream input{ temp.path / L"Dev Workspace" / L"App 1" / L"tab.yaml", std::ios::binary };
            const std::string content{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
            VERIFY_IS_TRUE(content.rfind("order:", 0) != 0 && content.find("\norder:") == std::string::npos);
            VERIFY_IS_TRUE(content.find("name:") == std::string::npos);
            VERIFY_IS_TRUE(content.rfind("id:", 0) != 0 && content.find("\nid:") == std::string::npos);
            VERIFY_IS_TRUE(content.find("profileName: 'Ubuntu'") != std::string::npos);
        }

        const auto loaded = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loaded.Workspaces().size()));

        const auto& loadedWorkspace = loaded.Workspaces().front();
        VERIFY_IS_TRUE(loadedWorkspace.Id == L"Dev Workspace");
        VERIFY_IS_TRUE(loadedWorkspace.Name == L"Dev Workspace");
        VERIFY_IS_TRUE(loadedWorkspace.Description == L"daily development");
        VERIFY_IS_TRUE(loadedWorkspace.BackgroundColor == L"#4682B4");
        VERIFY_IS_TRUE(loadedWorkspace.Locked);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loadedWorkspace.TabOrder.size()));
        VERIFY_IS_TRUE(loadedWorkspace.TabOrder.front() == L"App 1");
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loadedWorkspace.Nodes.size()));

        const auto& loadedNode = loadedWorkspace.Nodes.front();
        VERIFY_IS_TRUE(loadedNode.Id == L"App 1");
        VERIFY_IS_TRUE(loadedNode.Name == L"App 1");
        VERIFY_IS_TRUE(loadedNode.ConnectionRef == L"peer-app");
        VERIFY_IS_TRUE(loadedNode.ProfileGuid == L"{00000000-0000-0000-0000-000000000101}");
        VERIFY_IS_TRUE(loadedNode.ProfileName == L"Ubuntu");
        VERIFY_IS_TRUE(loadedNode.TabColor == L"#445566");
        VERIFY_IS_FALSE(loadedNode.ShowTab);
        VERIFY_IS_TRUE(loadedNode.StartupDirectory == L"D:\\work\\app");
        VERIFY_IS_TRUE(loadedNode.StartupAction == L"if ($true) {\n    'ok' >> .\\bootstrap.log\n}");
        VERIFY_IS_TRUE(loadedNode.OperatingSystem == L"windows");
        VERIFY_IS_TRUE(loadedNode.ShellType == L"powershell");
        VERIFY_IS_TRUE(loadedNode.ShowInputPanel);
        VERIFY_IS_FALSE(loadedNode.UseNodeNameAsTabTitle);
    }

    void WorkspaceTests::SaveUnlockedWorkspaceYamlRoundTrip()
    {
        TempPath temp;

        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";
        workspace.Locked = false;

        WorkspaceNode node;
        node.Id = L"node-1";
        node.ProfileGuid = L"{00000000-0000-0000-0000-000000000101}";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        VERIFY_IS_TRUE(manager.SaveToPath(temp.path));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"Dev Workspace" / L"workspace.yaml"));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"Dev Workspace" / L"node-1" / L"tab.yaml"));

        const auto loaded = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loaded.Workspaces().size()));
        VERIFY_IS_FALSE(loaded.Workspaces().front().Locked);
    }

    void WorkspaceTests::ApplyVisibleWorkspaceNodeOrderPreservesNodeIdentity()
    {
        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";

        WorkspaceNode hidden;
        hidden.Id = L"node-0";
        hidden.Name = L"Hidden";
        hidden.ConnectionRef = L"hidden-ref";
        hidden.ShowTab = false;
        workspace.Nodes.emplace_back(hidden);

        WorkspaceNode first;
        first.Id = L"node-1";
        first.Name = L"App 1";
        first.ConnectionRef = L"peer-app-1";
        first.ProfileGuid = L"{00000000-0000-0000-0000-000000000101}";
        first.StartupDirectory = L"D:\\work\\app1";
        workspace.Nodes.emplace_back(first);

        WorkspaceNode second;
        second.Id = L"node-2";
        second.Name = L"App 2";
        second.ConnectionRef = L"peer-app-2";
        second.ProfileGuid = L"{00000000-0000-0000-0000-000000000202}";
        second.StartupDirectory = L"D:\\work\\app2";
        workspace.Nodes.emplace_back(second);

        WorkspaceNode reorderedSecond = second;
        reorderedSecond.Name = L"App 2 Updated";
        reorderedSecond.StartupDirectory = L"E:\\tools\\app2";

        WorkspaceNode reorderedFirst = first;
        reorderedFirst.Name = L"App 1 Updated";
        reorderedFirst.StartupDirectory = L"E:\\tools\\app1";

        VERIFY_IS_TRUE(ApplyVisibleWorkspaceNodeOrder(workspace, { reorderedSecond, reorderedFirst }));
        VERIFY_ARE_EQUAL(3u, gsl::narrow_cast<unsigned int>(workspace.Nodes.size()));

        VERIFY_IS_TRUE(workspace.Nodes.at(0).Id == L"node-0");
        VERIFY_IS_TRUE(workspace.Nodes.at(0).ConnectionRef == L"hidden-ref");
        VERIFY_IS_FALSE(workspace.Nodes.at(0).ShowTab);

        VERIFY_IS_TRUE(workspace.Nodes.at(1).Id == L"node-2");
        VERIFY_IS_TRUE(workspace.Nodes.at(1).Name == L"App 2 Updated");
        VERIFY_IS_TRUE(workspace.Nodes.at(1).ConnectionRef == L"peer-app-2");
        VERIFY_IS_TRUE(workspace.Nodes.at(1).StartupDirectory == L"E:\\tools\\app2");

        VERIFY_IS_TRUE(workspace.Nodes.at(2).Id == L"node-1");
        VERIFY_IS_TRUE(workspace.Nodes.at(2).Name == L"App 1 Updated");
        VERIFY_IS_TRUE(workspace.Nodes.at(2).ConnectionRef == L"peer-app-1");
        VERIFY_IS_TRUE(workspace.Nodes.at(2).StartupDirectory == L"E:\\tools\\app1");
    }

    void WorkspaceTests::ReorderWorkspaceNodesPreservesNodeContent()
    {
        Workspace workspace;
        workspace.Id = L"ws-dev";
        workspace.Name = L"Dev Workspace";

        WorkspaceNode first;
        first.Id = L"node-1";
        first.Name = L"App 1";
        first.ConnectionRef = L"peer-app-1";
        first.ProfileGuid = L"{00000000-0000-0000-0000-000000000101}";
        first.TabColor = L"#112233";
        first.ShowTab = false;
        first.StartupDirectory = L"D:\\work\\app1";
        first.StartupAction = L".\\bootstrap-1.ps1";
        first.OperatingSystem = L"windows";
        first.ShellType = L"powershell";
        first.ShowInputPanel = true;
        first.UseNodeNameAsTabTitle = false;
        workspace.Nodes.emplace_back(first);

        WorkspaceNode second;
        second.Id = L"node-2";
        second.Name = L"App 2";
        second.ConnectionRef = L"peer-app-2";
        second.ProfileGuid = L"{00000000-0000-0000-0000-000000000202}";
        second.TabColor = L"#334455";
        second.ShowTab = true;
        second.StartupDirectory = L"/app2";
        second.StartupAction = L"./bootstrap-2.sh";
        second.OperatingSystem = L"linux";
        second.ShellType = L"ssh";
        second.ShowInputPanel = false;
        second.UseNodeNameAsTabTitle = true;
        workspace.Nodes.emplace_back(second);

        WorkspaceNode third;
        third.Id = L"node-3";
        third.Name = L"App 3";
        third.ConnectionRef = L"peer-app-3";
        third.ProfileGuid = L"{00000000-0000-0000-0000-000000000303}";
        third.TabColor = L"#556677";
        third.ShowTab = true;
        third.StartupDirectory = L"D:\\work\\app3";
        third.StartupAction = L".\\bootstrap-3.ps1";
        third.OperatingSystem = L"windows";
        third.ShellType = L"powershell";
        third.ShowInputPanel = true;
        third.UseNodeNameAsTabTitle = false;
        workspace.Nodes.emplace_back(third);

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        VERIFY_IS_TRUE(manager.ReorderWorkspaceNodes(L"ws-dev", { L"node-3", L"node-2" }));

        const auto* reorderedWorkspace = manager.FindById(L"ws-dev");
        VERIFY_IS_NOT_NULL(reorderedWorkspace);
        VERIFY_ARE_EQUAL(3u, gsl::narrow_cast<unsigned int>(reorderedWorkspace->Nodes.size()));
        VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<unsigned int>(reorderedWorkspace->TabOrder.size()));
        VERIFY_IS_TRUE(reorderedWorkspace->TabOrder.at(0) == L"node-3");
        VERIFY_IS_TRUE(reorderedWorkspace->TabOrder.at(1) == L"node-2");

        const auto& reorderedFirst = reorderedWorkspace->Nodes.at(0);
        VERIFY_IS_TRUE(reorderedFirst.Id == L"node-1");
        VERIFY_IS_TRUE(reorderedFirst.Name == L"App 1");
        VERIFY_IS_TRUE(reorderedFirst.ConnectionRef == L"peer-app-1");
        VERIFY_IS_TRUE(reorderedFirst.ProfileGuid == L"{00000000-0000-0000-0000-000000000101}");
        VERIFY_IS_TRUE(reorderedFirst.TabColor == L"#112233");
        VERIFY_IS_FALSE(reorderedFirst.ShowTab);
        VERIFY_IS_TRUE(reorderedFirst.StartupDirectory == L"D:\\work\\app1");
        VERIFY_IS_TRUE(reorderedFirst.StartupAction == L".\\bootstrap-1.ps1");
        VERIFY_IS_TRUE(reorderedFirst.OperatingSystem == L"windows");
        VERIFY_IS_TRUE(reorderedFirst.ShellType == L"powershell");
        VERIFY_IS_TRUE(reorderedFirst.ShowInputPanel);
        VERIFY_IS_FALSE(reorderedFirst.UseNodeNameAsTabTitle);

        const auto& reorderedSecond = reorderedWorkspace->Nodes.at(1);
        VERIFY_IS_TRUE(reorderedSecond.Id == L"node-2");
        VERIFY_IS_TRUE(reorderedSecond.Name == L"App 2");
        VERIFY_IS_TRUE(reorderedSecond.ConnectionRef == L"peer-app-2");
        VERIFY_IS_TRUE(reorderedSecond.ProfileGuid == L"{00000000-0000-0000-0000-000000000202}");
        VERIFY_IS_TRUE(reorderedSecond.TabColor == L"#334455");
        VERIFY_IS_TRUE(reorderedSecond.ShowTab);
        VERIFY_IS_TRUE(reorderedSecond.StartupDirectory == L"/app2");
        VERIFY_IS_TRUE(reorderedSecond.StartupAction == L"./bootstrap-2.sh");
        VERIFY_IS_TRUE(reorderedSecond.OperatingSystem == L"linux");
        VERIFY_IS_TRUE(reorderedSecond.ShellType == L"ssh");
        VERIFY_IS_FALSE(reorderedSecond.ShowInputPanel);
        VERIFY_IS_TRUE(reorderedSecond.UseNodeNameAsTabTitle);

        const auto& reorderedThird = reorderedWorkspace->Nodes.at(2);
        VERIFY_IS_TRUE(reorderedThird.Id == L"node-3");
        VERIFY_IS_TRUE(reorderedThird.Name == L"App 3");
        VERIFY_IS_TRUE(reorderedThird.ConnectionRef == L"peer-app-3");
        VERIFY_IS_TRUE(reorderedThird.ProfileGuid == L"{00000000-0000-0000-0000-000000000303}");
        VERIFY_IS_TRUE(reorderedThird.TabColor == L"#556677");
        VERIFY_IS_TRUE(reorderedThird.ShowTab);
        VERIFY_IS_TRUE(reorderedThird.StartupDirectory == L"D:\\work\\app3");
        VERIFY_IS_TRUE(reorderedThird.StartupAction == L".\\bootstrap-3.ps1");
        VERIFY_IS_TRUE(reorderedThird.OperatingSystem == L"windows");
        VERIFY_IS_TRUE(reorderedThird.ShellType == L"powershell");
        VERIFY_IS_TRUE(reorderedThird.ShowInputPanel);
        VERIFY_IS_FALSE(reorderedThird.UseNodeNameAsTabTitle);
    }

    void WorkspaceTests::ParseWorkspaceWindowStateYaml()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path / L"Dev Workspace");

        static constexpr std::string_view workspaceYaml{
            "version: 1\n"
        };

        static constexpr std::string_view yaml{
            "version: 1\n"
            "windows:\n"
            "  - windowId: '7'\n"
            "    windowName: 'Dev Window'\n"
        };

        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"workspace.yaml", std::ios::binary | std::ios::trunc };
            output.write(workspaceYaml.data(), gsl::narrow_cast<std::streamsize>(workspaceYaml.size()));
        }
        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"state.yaml", std::ios::binary | std::ios::trunc };
            output.write(yaml.data(), gsl::narrow_cast<std::streamsize>(yaml.size()));
        }

        auto state = WorkspaceStateManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(state.Windows().size()));
        VERIFY_ARE_EQUAL(7ull, state.Windows().front().WindowId);
        VERIFY_IS_TRUE(state.Windows().front().WindowName == L"Dev Window");
        VERIFY_IS_TRUE(state.Windows().front().WorkspaceId == L"Dev Workspace");
    }

    void WorkspaceTests::SaveWorkspaceWindowStateYamlRoundTrip()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path / L"Dev Workspace");

        static constexpr std::string_view workspaceYaml{
            "version: 1\n"
        };

        {
            std::ofstream output{ temp.path / L"Dev Workspace" / L"workspace.yaml", std::ios::binary | std::ios::trunc };
            output.write(workspaceYaml.data(), gsl::narrow_cast<std::streamsize>(workspaceYaml.size()));
        }
        {
            static constexpr std::string_view staleYaml{
                "version: 1\n"
                "windows:\n"
                "  - windowId: '99'\n"
            };
            std::ofstream output{ temp.path / L"workspace-window-state-WindowsTerminal-2826c24d03430e2b.yaml", std::ios::binary | std::ios::trunc };
            output.write(staleYaml.data(), gsl::narrow_cast<std::streamsize>(staleYaml.size()));
        }

        WorkspaceStateManager state;
        state.UpsertWindow(WorkspaceStateWindow{
            .WindowId = 7,
            .WindowName = L"Dev Window",
            .WorkspaceId = L"Dev Workspace",
        });

        VERIFY_IS_TRUE(state.SaveToPath(temp.path));
        VERIFY_IS_TRUE(std::filesystem::exists(temp.path / L"Dev Workspace" / L"state.yaml"));
        VERIFY_IS_FALSE(std::filesystem::exists(temp.path / L"workspace-window-state-WindowsTerminal-2826c24d03430e2b.yaml"));

        auto loaded = WorkspaceStateManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loaded.Windows().size()));
        VERIFY_ARE_EQUAL(7ull, loaded.Windows().front().WindowId);
        VERIFY_IS_TRUE(loaded.Windows().front().WindowName == L"Dev Window");
        VERIFY_IS_TRUE(loaded.Windows().front().WorkspaceId == L"Dev Workspace");
    }

    void WorkspaceTests::PersistWorkspaceStateInApplicationState()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path);

        auto state = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::ApplicationState>(temp.path);
        state->LastOpenedWorkspaceId(L"Dev Workspace");
        state->OpenInNewWindow(false);
        state->EnqueuePendingWorkspaceLaunch(L"Alpha Workspace");
        state->EnqueuePendingWorkspaceLaunch(L"Beta Workspace");
        state->RemovePendingWorkspaceLaunch(L"Alpha Workspace");
        VERIFY_IS_TRUE(state->ConsumePendingWorkspaceLaunch() == L"Beta Workspace");
        state->EnqueuePendingWorkspaceLaunch(L"Gamma Workspace");
        state->Flush();

        auto loaded = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::ApplicationState>(temp.path);
        VERIFY_IS_TRUE(loaded->LastOpenedWorkspaceId() == L"Dev Workspace");
        VERIFY_IS_FALSE(loaded->OpenInNewWindow());
        VERIFY_ARE_EQUAL(1u, loaded->PendingWorkspaceLaunches().Size());
        VERIFY_IS_TRUE(loaded->PendingWorkspaceLaunches().GetAt(0) == L"Gamma Workspace");
    }
}
