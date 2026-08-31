// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

// This file deliberately lives outside both src/ and microsoft/. It reuses the
// existing TAEF harness only as a runner; all test cases belong to Mirror Core.
#include "../../../../microsoft/src/cascadia/UnitTests_SettingsModel/pch.h"
#include "../../../../src/core/workspace/WorkspaceCore.h"

using namespace WEX::Logging;
namespace workspace_core = terminal::workspace;

namespace MirrorCoreTests
{
    class MirrorCoreTests
    {
        TEST_CLASS(MirrorCoreTests);

        TEST_METHOD(RecoveryUsesCheckpointAndNeverCrossesGap);
        TEST_METHOD(ControlLeaseAndReducerRejectUnauthorizedInput);
        TEST_METHOD(RelayAndTerminalProtocolsRoundTrip);
        TEST_METHOD(NodeRecorderAndRouteDispatcherPreserveCoreBoundary);
        TEST_METHOD(OutputEvictionRequiresCheckpoint);
        TEST_METHOD(DirectoryProjectionNeverContainsTerminalContent);
        TEST_METHOD(CapabilityRegistryEnforcesPolicyVersion);
    };

    void MirrorCoreTests::RecoveryUsesCheckpointAndNeverCrossesGap()
    {
        auto session = workspace_core::CreateWorkspaceMirrorNodeSession(L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(workspace_core::BeginWorkspaceMirrorWindow(session, L"agent", 24, 80, 1, 3));
        VERIFY_IS_TRUE(workspace_core::RecordWorkspaceMirrorOutput(session, L"agent", { 'a' }, 2, 3));
        VERIFY_IS_TRUE(workspace_core::AddWorkspaceMirrorCheckpoint(session, L"agent", { 24, 80, { 's' } }, 2));
        VERIFY_IS_TRUE(workspace_core::RecordWorkspaceMirrorOutput(session, L"agent", { 'b' }, 3, 3));
        const auto checkpoint = workspace_core::PlanWorkspaceMirrorRecovery(session, L"agent", std::nullopt);
        VERIFY_IS_TRUE(checkpoint.Kind == workspace_core::WorkspaceMirrorRecoveryKind::Checkpoint);
        VERIFY_IS_TRUE(checkpoint.Checkpoint.has_value());
        VERIFY_IS_TRUE(workspace_core::RecordWorkspaceMirrorOutput(session, L"agent", { 'c' }, 4, 3));
        VERIFY_IS_TRUE(workspace_core::PlanWorkspaceMirrorRecovery(session, L"agent", 1).Kind == workspace_core::WorkspaceMirrorRecoveryKind::Checkpoint);
    }

    void MirrorCoreTests::ControlLeaseAndReducerRejectUnauthorizedInput()
    {
        auto session = workspace_core::CreateWorkspaceMirrorNodeSession(L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(workspace_core::BeginWorkspaceMirrorWindow(session, L"agent", 24, 80, 1));
        VERIFY_IS_TRUE(workspace_core::GrantWorkspaceMirrorControl(session, L"client", L"lease", 10, 20));
        workspace_core::WorkspaceMirrorEffect effect;
        VERIFY_IS_FALSE(workspace_core::TryCreateWorkspaceMirrorInputEffect(session, L"agent", L"other", L"lease", L"bad", 11, effect));
        VERIFY_IS_TRUE(workspace_core::TryCreateWorkspaceMirrorInputEffect(session, L"agent", L"client", L"lease", L"ok", 11, effect));
        VERIFY_IS_TRUE(effect.Type == workspace_core::WorkspaceMirrorEffect::Kind::WriteInput);
        VERIFY_IS_FALSE(workspace_core::HasWorkspaceMirrorControl(session, L"client", L"lease", 30));
    }

    void MirrorCoreTests::RelayAndTerminalProtocolsRoundTrip()
    {
        workspace_core::WorkspaceMirrorRelayFrame relay;
        relay.Kind = workspace_core::WorkspaceMirrorRelayFrameKind::TerminalEffect;
        relay.Payload = { 0, 0xff, 'v', 't' };
        std::vector<uint8_t> bytes;
        VERIFY_IS_TRUE(workspace_core::EncodeWorkspaceMirrorRelayFrame(relay, bytes));
        workspace_core::WorkspaceMirrorRelayFrame decodedRelay;
        VERIFY_IS_TRUE(workspace_core::DecodeWorkspaceMirrorRelayFrame(bytes, decodedRelay));
        workspace_core::WorkspaceMirrorTerminalMessage message{ .Kind = workspace_core::WorkspaceMirrorTerminalMessageKind::Input, .CommandId = L"agent", .LeaseId = L"lease", .Text = L"\u4f60\u597d\r" };
        VERIFY_IS_TRUE(workspace_core::EncodeWorkspaceMirrorTerminalMessage(message, bytes));
        workspace_core::WorkspaceMirrorTerminalMessage decodedMessage;
        VERIFY_IS_TRUE(workspace_core::DecodeWorkspaceMirrorTerminalMessage(bytes, decodedMessage));
        VERIFY_IS_TRUE(decodedMessage.Text == message.Text);
    }

    void MirrorCoreTests::NodeRecorderAndRouteDispatcherPreserveCoreBoundary()
    {
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(recorder->BeginWindow(L"agent", 24, 80, 1));
        VERIFY_IS_TRUE(recorder->GrantControl(L"client", L"lease", 10, 30));
        workspace_core::WorkspaceMirrorEffect effect;
        workspace_core::WorkspaceMirrorRouteDispatcher dispatcher{ recorder, [&effect](const auto& value) { effect = value; } };
        workspace_core::WorkspaceMirrorTerminalMessage input{ .Kind = workspace_core::WorkspaceMirrorTerminalMessageKind::Input, .CommandId = L"agent", .LeaseId = L"lease", .Text = L"ls\r" };
        workspace_core::WorkspaceMirrorRelayFrame frame{ .Kind = workspace_core::WorkspaceMirrorRelayFrameKind::TerminalIntent };
        VERIFY_IS_TRUE(workspace_core::EncodeWorkspaceMirrorTerminalMessage(input, frame.Payload));
        std::vector<workspace_core::WorkspaceMirrorRelayFrame> outbound;
        VERIFY_IS_TRUE(dispatcher.HandleInbound(frame, L"client", 20, outbound));
        VERIFY_IS_TRUE(effect.Text == L"ls\r");
    }

    void MirrorCoreTests::OutputEvictionRequiresCheckpoint()
    {
        auto session = workspace_core::CreateWorkspaceMirrorNodeSession(L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(workspace_core::BeginWorkspaceMirrorWindow(session, L"agent", 24, 80, 1, 2));
        std::vector<uint8_t> output(workspace_core::WorkspaceMirrorMaximumOutputChunkBytes + 1, 'x');
        VERIFY_IS_TRUE(workspace_core::RecordWorkspaceMirrorOutput(session, L"agent", std::move(output), 2, 2));
        const auto* window = workspace_core::FindWorkspaceMirrorWindow(session, L"agent");
        VERIFY_IS_TRUE(window->HasGap);
        VERIFY_IS_TRUE(workspace_core::PlanWorkspaceMirrorRecovery(session, L"agent", 1).Kind == workspace_core::WorkspaceMirrorRecoveryKind::Unavailable);
    }

    void MirrorCoreTests::DirectoryProjectionNeverContainsTerminalContent()
    {
        auto session = workspace_core::CreateWorkspaceMirrorNodeSession(L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(workspace_core::BeginWorkspaceMirrorWindow(session, L"agent", 24, 80, 1));
        VERIFY_IS_TRUE(workspace_core::RecordWorkspaceMirrorOutput(session, L"agent", { 's', 'e', 'c', 'r', 'e', 't' }, 2));
        const auto projection = workspace_core::BuildWorkspaceMirrorDirectoryProjection(session, L"Agent", 7);
        VERIFY_IS_TRUE(projection.DisplayName == L"Agent");
        VERIFY_IS_TRUE(projection.Available);
        VERIFY_ARE_EQUAL(1u, projection.WindowCount);
        VERIFY_IS_TRUE(std::find(projection.Capabilities.cbegin(), projection.Capabilities.cend(), L"view") != projection.Capabilities.cend());
    }

    void MirrorCoreTests::CapabilityRegistryEnforcesPolicyVersion()
    {
        auto session = workspace_core::CreateWorkspaceMirrorNodeSession(L"workspace", L"node", L"session");
        workspace_core::WorkspaceMirrorCapability capability{ .UserId = L"user", .DeviceId = L"device", .NodeSessionId = L"session", .Permission = workspace_core::WorkspaceMirrorPermission::Control, .ExpiresAtMilliseconds = 100, .PolicyVersion = 7 };
        VERIFY_IS_TRUE(workspace_core::IsWorkspaceMirrorCapabilityValid(capability, session, L"device", workspace_core::WorkspaceMirrorPermission::View, 99, 7));
        VERIFY_IS_FALSE(workspace_core::IsWorkspaceMirrorCapabilityValid(capability, session, L"device", workspace_core::WorkspaceMirrorPermission::Control, 100, 7));
        workspace_core::WorkspaceMirrorRegistry registry;
        VERIFY_IS_TRUE(registry.Add(session));
        VERIFY_IS_FALSE(registry.Add(session));
    }
}
