// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "../../../microsoft/src/cascadia/UnitTests_SettingsModel/pch.h"
#include "../../../src/core/workspace/WorkspaceCore.h"

namespace workspace_core = terminal::workspace;

namespace WorkspaceCoreTests
{
    class WorkspaceCoreTests
    {
        TEST_CLASS(WorkspaceCoreTests);

        TEST_METHOD(StartupStateUsesOnlyVisibleNodes);
        TEST_METHOD(MirrorNodePolicyParserValidatesBounds);
    };

    void WorkspaceCoreTests::StartupStateUsesOnlyVisibleNodes()
    {
        workspace_core::Workspace visible{ .Id = L"workspace" };
        visible.Nodes = { { .Id = L"visible", .ShowTab = true }, { .Id = L"hidden", .ShowTab = false } };
        workspace_core::WorkspaceManager manager;
        manager.SetWorkspaces({ visible });
        const auto state = workspace_core::PrepareWorkspaceStartupState(L"workspace", manager);
        VERIFY_ARE_EQUAL(1u, gsl::narrow_cast<unsigned int>(state.PendingNodeIds.size()));
        VERIFY_IS_TRUE(state.PendingNodeIds.front() == L"visible");
    }

    void WorkspaceCoreTests::MirrorNodePolicyParserValidatesBounds()
    {
        workspace_core::WorkspaceNodeMirrorConfiguration policy;
        VERIFY_IS_TRUE(workspace_core::ApplyWorkspaceMirrorConfigurationField(policy, L"mirrorMode", L"node-session"));
        VERIFY_IS_TRUE(workspace_core::ApplyWorkspaceMirrorConfigurationField(policy, L"mirrorMaximumEvents", L"8192"));
        VERIFY_IS_TRUE(workspace_core::ApplyWorkspaceMirrorConfigurationField(policy, L"mirrorMaximumCheckpoints", L"6"));
        VERIFY_IS_FALSE(workspace_core::ApplyWorkspaceMirrorConfigurationField(policy, L"mirrorMaximumEvents", L"0"));
        VERIFY_IS_TRUE(policy.Mode == workspace_core::WorkspaceNodeMirrorMode::NodeSession);
        VERIFY_ARE_EQUAL(8192u, policy.MaximumEvents);
        VERIFY_ARE_EQUAL(6u, policy.MaximumCheckpoints);
    }
}
