// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

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

        TEST_METHOD(ParseWorkspaceYaml);
        TEST_METHOD(ParseWorkspaceYamlDefaultsToLocked);
        TEST_METHOD(BuildWorkspaceStartupActions);
        TEST_METHOD(SaveWorkspaceYamlRoundTrip);
        TEST_METHOD(SaveUnlockedWorkspaceYamlRoundTrip);
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

    void WorkspaceTests::ParseWorkspaceYaml()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path.parent_path());

        static constexpr std::string_view yaml{
            "version: 1\n"
            "workspaces:\n"
            "  - id: 'ws-dev'\n"
            "    name: 'Dev Workspace'\n"
            "    description: 'daily development'\n"
            "    backgroundColor: '#4682B4'\n"
            "    locked: true\n"
            "    nodes:\n"
            "      - id: 'node-1'\n"
            "        name: 'App 1'\n"
            "        connectionRef: 'peer-app'\n"
            "        profileGuid: '{00000000-0000-0000-0000-000000000101}'\n"
            "        startupDirectory: 'D:\\work\\app'\n"
            "        startupAction: '.\\bootstrap.ps1'\n"
        };

        {
            std::ofstream output{ temp.path, std::ios::binary | std::ios::trunc };
            output.write(yaml.data(), gsl::narrow_cast<std::streamsize>(yaml.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(manager.Workspaces().size()));

        const auto& workspace = manager.Workspaces().front();
        VERIFY_IS_TRUE(workspace.Id == L"ws-dev");
        VERIFY_IS_TRUE(workspace.Name == L"Dev Workspace");
        VERIFY_IS_TRUE(workspace.BackgroundColor == L"#4682B4");
        VERIFY_IS_TRUE(workspace.Locked);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(workspace.Nodes.size()));
        VERIFY_IS_TRUE(workspace.Nodes.front().Id == L"node-1");
        VERIFY_IS_TRUE(workspace.Nodes.front().StartupDirectory == L"D:\\work\\app");
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

        const auto sendInputArgs = actions.at(1).Args().try_as<SendInputArgs>();
        VERIFY_IS_NOT_NULL(sendInputArgs);
        VERIFY_IS_TRUE(std::wstring{ sendInputArgs.Input() } == L".\\bootstrap.ps1\r");
    }

    void WorkspaceTests::ParseWorkspaceYamlDefaultsToLocked()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path.parent_path());

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
            std::ofstream output{ temp.path, std::ios::binary | std::ios::trunc };
            output.write(yaml.data(), gsl::narrow_cast<std::streamsize>(yaml.size()));
        }

        const auto manager = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(manager.Workspaces().size()));
        VERIFY_IS_TRUE(manager.Workspaces().front().Locked);
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
        node.StartupDirectory = L"D:\\work\\app";
        node.StartupAction = L".\\bootstrap.ps1";
        workspace.Nodes.emplace_back(std::move(node));

        WorkspaceManager manager;
        manager.SetWorkspaces({ workspace });

        VERIFY_IS_TRUE(manager.SaveToPath(temp.path));

        const auto loaded = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loaded.Workspaces().size()));

        const auto& loadedWorkspace = loaded.Workspaces().front();
        VERIFY_IS_TRUE(loadedWorkspace.Id == L"ws-dev");
        VERIFY_IS_TRUE(loadedWorkspace.Name == L"Dev Workspace");
        VERIFY_IS_TRUE(loadedWorkspace.Description == L"daily development");
        VERIFY_IS_TRUE(loadedWorkspace.BackgroundColor == L"#4682B4");
        VERIFY_IS_TRUE(loadedWorkspace.Locked);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loadedWorkspace.Nodes.size()));

        const auto& loadedNode = loadedWorkspace.Nodes.front();
        VERIFY_IS_TRUE(loadedNode.Id == L"node-1");
        VERIFY_IS_TRUE(loadedNode.Name == L"App 1");
        VERIFY_IS_TRUE(loadedNode.ConnectionRef == L"peer-app");
        VERIFY_IS_TRUE(loadedNode.ProfileGuid == L"{00000000-0000-0000-0000-000000000101}");
        VERIFY_IS_TRUE(loadedNode.StartupDirectory == L"D:\\work\\app");
        VERIFY_IS_TRUE(loadedNode.StartupAction == L".\\bootstrap.ps1");
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

        const auto loaded = WorkspaceManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loaded.Workspaces().size()));
        VERIFY_IS_FALSE(loaded.Workspaces().front().Locked);
    }

    void WorkspaceTests::ParseWorkspaceWindowStateYaml()
    {
        TempPath temp;

        static constexpr std::string_view yaml{
            "version: 1\n"
            "windows:\n"
            "  - windowId: '7'\n"
            "    windowName: 'Dev Window'\n"
            "    workspaceId: 'ws-dev'\n"
        };

        {
            std::ofstream output{ temp.path, std::ios::binary | std::ios::trunc };
            output.write(yaml.data(), gsl::narrow_cast<std::streamsize>(yaml.size()));
        }

        auto state = WorkspaceStateManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(state.Windows().size()));
        VERIFY_ARE_EQUAL(7ull, state.Windows().front().WindowId);
        VERIFY_IS_TRUE(state.Windows().front().WindowName == L"Dev Window");
        VERIFY_IS_TRUE(state.Windows().front().WorkspaceId == L"ws-dev");
    }

    void WorkspaceTests::SaveWorkspaceWindowStateYamlRoundTrip()
    {
        TempPath temp;

        WorkspaceStateManager state;
        state.UpsertWindow(WorkspaceStateWindow{
            .WindowId = 7,
            .WindowName = L"Dev Window",
            .WorkspaceId = L"ws-dev",
        });

        VERIFY_IS_TRUE(state.SaveToPath(temp.path));

        auto loaded = WorkspaceStateManager::LoadFromPath(temp.path);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(loaded.Windows().size()));
        VERIFY_ARE_EQUAL(7ull, loaded.Windows().front().WindowId);
        VERIFY_IS_TRUE(loaded.Windows().front().WindowName == L"Dev Window");
        VERIFY_IS_TRUE(loaded.Windows().front().WorkspaceId == L"ws-dev");
    }

    void WorkspaceTests::PersistWorkspaceStateInApplicationState()
    {
        TempPath temp;
        std::filesystem::create_directories(temp.path);

        auto state = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::ApplicationState>(temp.path);
        state->LastOpenedWorkspaceId(L"ws-dev");
        state->OpenInNewWindow(false);
        state->EnqueuePendingWorkspaceLaunch(L"ws-a");
        state->EnqueuePendingWorkspaceLaunch(L"ws-b");
        state->RemovePendingWorkspaceLaunch(L"ws-a");
        VERIFY_IS_TRUE(state->ConsumePendingWorkspaceLaunch() == L"ws-b");
        state->EnqueuePendingWorkspaceLaunch(L"ws-c");
        state->Flush();

        auto loaded = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::ApplicationState>(temp.path);
        VERIFY_IS_TRUE(loaded->LastOpenedWorkspaceId() == L"ws-dev");
        VERIFY_IS_FALSE(loaded->OpenInNewWindow());
        VERIFY_ARE_EQUAL(1u, loaded->PendingWorkspaceLaunches().Size());
        VERIFY_IS_TRUE(loaded->PendingWorkspaceLaunches().GetAt(0) == L"ws-c");
    }
}
