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
        TEST_METHOD(RelayProtocolMatchesTerminalServerFixture);
        TEST_METHOD(TerminalProtocolRejectsInvalidUtf8);
        TEST_METHOD(NodeRecorderAndRouteDispatcherPreserveCoreBoundary);
        TEST_METHOD(NodeRecorderFansLiveOutputToEveryRoute);
        TEST_METHOD(RouteCloseIsAValidNoopForMirrorCore);
        TEST_METHOD(NodeAgentOnlyFansOutputToOpenRoutes);
        TEST_METHOD(DeviceTunnelSessionMapsWebSocketBytesToCoreEffects);
        TEST_METHOD(DeviceTunnelSessionMaintainsServerHeartbeat);
        TEST_METHOD(DeviceTunnelClientConnectsToLocalTerminalServer);
        TEST_METHOD(DeviceTunnelClientAcceptsRouteFromLocalTerminalServer);
        TEST_METHOD(DeviceTunnelClientRelaysOutputToLocalBrowser);
        TEST_METHOD(DeviceEnrollmentRedeemsAndConnectsWithMtls);
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

    void MirrorCoreTests::RelayProtocolMatchesTerminalServerFixture()
    {
        workspace_core::WorkspaceMirrorRelayFrame frame;
        frame.Kind = workspace_core::WorkspaceMirrorRelayFrameKind::TerminalEffect;
        frame.Flags = 0x0042;
        for (uint8_t index{}; index < 16; ++index)
        {
            frame.RouteId[index] = index;
            frame.DeviceId[index] = static_cast<uint8_t>(0x10 + index);
            frame.NodeSessionId[index] = static_cast<uint8_t>(0x20 + index);
            frame.ClientId[index] = static_cast<uint8_t>(0x30 + index);
        }
        frame.Payload = { 0, 0xff, 'v', 't' };

        std::vector<uint8_t> encoded;
        VERIFY_IS_TRUE(workspace_core::EncodeWorkspaceMirrorRelayFrame(frame, encoded));
        const std::vector<uint8_t> expected{
            1, 5, 0, 0x42,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
            0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
            0, 0, 0, 4,
            0, 0xff, 'v', 't'
        };
        VERIFY_ARE_EQUAL(expected.size(), encoded.size());
        VERIFY_IS_TRUE(encoded == expected);
    }

    void MirrorCoreTests::TerminalProtocolRejectsInvalidUtf8()
    {
        // major, input kind, empty command/lease, sequence/size fields, a
        // two-byte malformed UTF-8 text field, and no terminal byte payload.
        const std::vector<uint8_t> malformed{
            1, 2, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 2, 0, 0, 0, 0,
            0xc3, 0x28
        };
        workspace_core::WorkspaceMirrorTerminalMessage decoded;
        VERIFY_IS_FALSE(workspace_core::DecodeWorkspaceMirrorTerminalMessage(malformed, decoded));
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

    void MirrorCoreTests::NodeRecorderFansLiveOutputToEveryRoute()
    {
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(recorder->BeginWindow(L"agent", 24, 80, 1));
        workspace_core::WorkspaceMirrorRouteDispatcher dispatcher{ recorder, {} };
        workspace_core::WorkspaceMirrorRelayFrame first{ .Kind = workspace_core::WorkspaceMirrorRelayFrameKind::RouteAccepted };
        first.RouteId[0] = 1;
        workspace_core::WorkspaceMirrorRelayFrame second{ .Kind = workspace_core::WorkspaceMirrorRelayFrameKind::RouteAccepted };
        second.RouteId[0] = 2;
        const std::array routes{ first, second };
        std::vector<workspace_core::WorkspaceMirrorRelayFrame> outbound;
        VERIFY_IS_TRUE(dispatcher.RecordOutputAndBuildEffects(L"agent", { 'o', 'k' }, 2, routes, outbound));
        VERIFY_ARE_EQUAL(2u, gsl::narrow<uint32_t>(outbound.size()));
        const std::vector<uint8_t> expectedBytes{ 'o', 'k' };
        for (const auto& frame : outbound)
        {
            VERIFY_IS_TRUE(frame.Kind == workspace_core::WorkspaceMirrorRelayFrameKind::TerminalEffect);
            workspace_core::WorkspaceMirrorTerminalMessage message;
            VERIFY_IS_TRUE(workspace_core::DecodeWorkspaceMirrorTerminalMessage(frame.Payload, message));
            VERIFY_IS_TRUE(message.Kind == workspace_core::WorkspaceMirrorTerminalMessageKind::Output);
            VERIFY_ARE_EQUAL(2ull, message.Sequence);
            VERIFY_IS_TRUE(message.Bytes == expectedBytes);
        }
        VERIFY_ARE_EQUAL(1, outbound[0].RouteId[0]);
        VERIFY_ARE_EQUAL(2, outbound[1].RouteId[0]);
    }

    void MirrorCoreTests::RouteCloseIsAValidNoopForMirrorCore()
    {
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        workspace_core::WorkspaceMirrorRouteDispatcher dispatcher{ recorder, {} };
        workspace_core::WorkspaceMirrorRelayFrame close{ .Kind = workspace_core::WorkspaceMirrorRelayFrameKind::RouteClose };
        std::vector<workspace_core::WorkspaceMirrorRelayFrame> outbound;
        VERIFY_IS_TRUE(dispatcher.HandleInbound(close, L"client", 1, outbound));
        VERIFY_IS_TRUE(outbound.empty());
    }

    void MirrorCoreTests::NodeAgentOnlyFansOutputToOpenRoutes()
    {
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(recorder->BeginWindow(L"agent", 24, 80, 1));
        auto dispatcher = std::make_shared<workspace_core::WorkspaceMirrorRouteDispatcher>(recorder, workspace_core::WorkspaceMirrorRouteDispatcher::InputEffectSink{});
        workspace_core::WorkspaceMirrorNodeAgent agent{ dispatcher };
        workspace_core::WorkspaceMirrorRelayFrame route{ .Kind = workspace_core::WorkspaceMirrorRelayFrameKind::RouteOpen };
        route.RouteId[0] = 7;
        std::vector<workspace_core::WorkspaceMirrorRelayFrame> outbound;
        VERIFY_IS_FALSE(agent.HandleServerFrame({ .Kind = workspace_core::WorkspaceMirrorRelayFrameKind::TerminalIntent }, L"client", 2, outbound));
        VERIFY_IS_TRUE(agent.HandleServerFrame(route, L"client", 2, outbound));
        VERIFY_ARE_EQUAL(1u, gsl::narrow<uint32_t>(agent.ActiveRouteCount()));
        outbound.clear();
        VERIFY_IS_TRUE(agent.PublishOutput(L"agent", { 'x' }, 3, outbound));
        VERIFY_ARE_EQUAL(1u, gsl::narrow<uint32_t>(outbound.size()));
        VERIFY_ARE_EQUAL(7, outbound[0].RouteId[0]);
        route.Kind = workspace_core::WorkspaceMirrorRelayFrameKind::RouteClose;
        outbound.clear();
        VERIFY_IS_TRUE(agent.HandleServerFrame(route, L"client", 4, outbound));
        VERIFY_ARE_EQUAL(0u, gsl::narrow<uint32_t>(agent.ActiveRouteCount()));
        VERIFY_IS_TRUE(agent.PublishOutput(L"agent", { 'y' }, 5, outbound));
        VERIFY_IS_TRUE(outbound.empty());
    }

    void MirrorCoreTests::DeviceTunnelSessionMapsWebSocketBytesToCoreEffects()
    {
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(recorder->BeginWindow(L"agent", 24, 80, 1));
        auto dispatcher = std::make_shared<workspace_core::WorkspaceMirrorRouteDispatcher>(recorder, workspace_core::WorkspaceMirrorRouteDispatcher::InputEffectSink{});
        auto agent = std::make_shared<workspace_core::WorkspaceMirrorNodeAgent>(dispatcher);
        workspace_core::WorkspaceMirrorDeviceTunnelSession tunnel{ agent };
        workspace_core::WorkspaceMirrorRelayFrame route{ .Kind = workspace_core::WorkspaceMirrorRelayFrameKind::RouteOpen };
        route.RouteId[0] = 9;
        std::vector<uint8_t> wire;
        VERIFY_IS_TRUE(workspace_core::EncodeWorkspaceMirrorRelayFrame(route, wire));
        std::vector<std::vector<uint8_t>> outbound;
        VERIFY_IS_TRUE(tunnel.HandleBinary(wire, 2, outbound));
        VERIFY_ARE_EQUAL(1u, gsl::narrow<uint32_t>(outbound.size()));
        workspace_core::WorkspaceMirrorRelayFrame accepted;
        VERIFY_IS_TRUE(workspace_core::DecodeWorkspaceMirrorRelayFrame(outbound[0], accepted));
        VERIFY_IS_TRUE(accepted.Kind == workspace_core::WorkspaceMirrorRelayFrameKind::RouteAccepted);
        outbound.clear();
        VERIFY_IS_TRUE(tunnel.PublishOutput(L"agent", { 'z' }, 3, outbound));
        VERIFY_ARE_EQUAL(1u, gsl::narrow<uint32_t>(outbound.size()));
        workspace_core::WorkspaceMirrorRelayFrame effect;
        workspace_core::WorkspaceMirrorTerminalMessage output;
        VERIFY_IS_TRUE(workspace_core::DecodeWorkspaceMirrorRelayFrame(outbound[0], effect));
        VERIFY_IS_TRUE(workspace_core::DecodeWorkspaceMirrorTerminalMessage(effect.Payload, output));
        VERIFY_IS_TRUE(output.Kind == workspace_core::WorkspaceMirrorTerminalMessageKind::Output);
        VERIFY_IS_TRUE(output.Bytes == std::vector<uint8_t>{ 'z' });
    }

    void MirrorCoreTests::DeviceTunnelSessionMaintainsServerHeartbeat()
    {
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        auto dispatcher = std::make_shared<workspace_core::WorkspaceMirrorRouteDispatcher>(recorder, workspace_core::WorkspaceMirrorRouteDispatcher::InputEffectSink{});
        auto agent = std::make_shared<workspace_core::WorkspaceMirrorNodeAgent>(dispatcher);
        workspace_core::WorkspaceMirrorDeviceTunnelSession tunnel{ agent };
        std::array<uint8_t, 16> deviceId{};
        deviceId[0] = 0x42;
        std::vector<uint8_t> wire;
        VERIFY_IS_TRUE(tunnel.BuildHeartbeat(deviceId, wire));
        workspace_core::WorkspaceMirrorRelayFrame ping;
        VERIFY_IS_TRUE(workspace_core::DecodeWorkspaceMirrorRelayFrame(wire, ping));
        VERIFY_IS_TRUE(ping.Kind == workspace_core::WorkspaceMirrorRelayFrameKind::Ping);
        VERIFY_IS_TRUE(ping.DeviceId == deviceId);

        ping.Kind = workspace_core::WorkspaceMirrorRelayFrameKind::Pong;
        VERIFY_IS_TRUE(workspace_core::EncodeWorkspaceMirrorRelayFrame(ping, wire));
        std::vector<std::vector<uint8_t>> outbound;
        VERIFY_IS_TRUE(tunnel.HandleBinary(wire, 4, outbound));
        VERIFY_IS_TRUE(outbound.empty());
    }

    void MirrorCoreTests::DeviceTunnelClientConnectsToLocalTerminalServer()
    {
        const auto environment = [](const wchar_t* name) {
            std::array<wchar_t, 256> value{};
            const auto length = GetEnvironmentVariableW(name, value.data(), gsl::narrow<DWORD>(value.size()));
            return length && length < value.size() ? std::optional<std::wstring>{ std::in_place, value.data(), length } : std::nullopt;
        };
        const auto organization = environment(L"TERMINAL_MIRROR_TEST_ORGANIZATION_ID");
        const auto device = environment(L"TERMINAL_MIRROR_TEST_DEVICE_ID");
        const auto serial = environment(L"TERMINAL_MIRROR_TEST_CERTIFICATE_SERIAL");
        const auto endpoint = environment(L"TERMINAL_MIRROR_TEST_TUNNEL_ENDPOINT").value_or(L"ws://127.0.0.1:8787/v1/device-tunnel");
        if (!organization || !device || !serial)
        {
            Log::Comment(L"Local terminal-server integration environment is not configured; skipping network assertion.");
            return;
        }
        std::array<uint8_t, 16> deviceId{};
        size_t output{};
        for (const auto character : *device)
        {
            if (character == '-') continue;
            const auto nibble = character >= L'0' && character <= L'9' ? character - L'0' :
                                character >= L'a' && character <= L'f' ? character - L'a' + 10 :
                                character >= L'A' && character <= L'F' ? character - L'A' + 10 : -1;
            VERIFY_IS_TRUE(nibble >= 0 && output < deviceId.size() * 2);
            if (output % 2 == 0) deviceId[output / 2] = gsl::narrow_cast<uint8_t>(nibble << 4);
            else deviceId[output / 2] |= gsl::narrow_cast<uint8_t>(nibble);
            ++output;
        }
        VERIFY_ARE_EQUAL(deviceId.size() * 2, output);

        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        auto dispatcher = std::make_shared<workspace_core::WorkspaceMirrorRouteDispatcher>(recorder, workspace_core::WorkspaceMirrorRouteDispatcher::InputEffectSink{});
        auto agent = std::make_shared<workspace_core::WorkspaceMirrorNodeAgent>(dispatcher);
        auto session = std::make_shared<workspace_core::WorkspaceMirrorDeviceTunnelSession>(agent);
        workspace_core::WorkspaceMirrorDeviceTunnelClient client{ session };
        workspace_core::WorkspaceMirrorDeviceTunnelConnection connection;
        connection.Endpoint = endpoint;
        connection.DeviceId = deviceId;
        connection.OrganizationId = *organization;
        connection.DeviceIdText = *device;
        connection.UseDevelopmentIdentityHeaders = true;
        connection.DevelopmentCertificateSerial = *serial;
        VERIFY_IS_TRUE(client.Connect(connection));
        VERIFY_IS_FALSE(client.ConnectionId().empty());
        workspace_core::WorkspaceMirrorDirectoryProjection projection;
        projection.WorkspaceId = L"workspace-a";
        projection.NodeId = L"window-42";
        projection.NodeSessionId = L"window-42:session-7";
        projection.DisplayName = L"\u5f00\u53d1\u7ec8\u7aef";
        projection.Available = true;
        projection.WindowCount = 1;
        projection.Capabilities = { L"view", L"control" };
        projection.ProjectionVersion = 1;
        VERIFY_IS_TRUE(client.ReportDirectory(projection));
        VERIFY_IS_TRUE(client.SendHeartbeat());
        VERIFY_IS_TRUE(client.ReceiveOnce(1));
    }

    void MirrorCoreTests::DeviceTunnelClientAcceptsRouteFromLocalTerminalServer()
    {
        const auto environment = [](const wchar_t* name) {
            std::array<wchar_t, 256> value{};
            const auto length = GetEnvironmentVariableW(name, value.data(), gsl::narrow<DWORD>(value.size()));
            return length && length < value.size() ? std::optional<std::wstring>{ std::in_place, value.data(), length } : std::nullopt;
        };
        const auto organization = environment(L"TERMINAL_MIRROR_TEST_ORGANIZATION_ID");
        const auto device = environment(L"TERMINAL_MIRROR_TEST_DEVICE_ID");
        const auto serial = environment(L"TERMINAL_MIRROR_TEST_CERTIFICATE_SERIAL");
        const auto expectRoute = environment(L"TERMINAL_MIRROR_TEST_EXPECT_ROUTE");
        const auto endpoint = environment(L"TERMINAL_MIRROR_TEST_TUNNEL_ENDPOINT").value_or(L"ws://127.0.0.1:8787/v1/device-tunnel");
        if (!organization || !device || !serial || !expectRoute)
        {
            Log::Comment(L"Local terminal-server route integration environment is not configured; skipping network assertion.");
            return;
        }
        std::array<uint8_t, 16> deviceId{};
        size_t output{};
        for (const auto character : *device)
        {
            if (character == '-') continue;
            const auto nibble = character >= L'0' && character <= L'9' ? character - L'0' :
                                character >= L'a' && character <= L'f' ? character - L'a' + 10 :
                                character >= L'A' && character <= L'F' ? character - L'A' + 10 : -1;
            VERIFY_IS_TRUE(nibble >= 0 && output < deviceId.size() * 2);
            if (output % 2 == 0) deviceId[output / 2] = gsl::narrow_cast<uint8_t>(nibble << 4);
            else deviceId[output / 2] |= gsl::narrow_cast<uint8_t>(nibble);
            ++output;
        }
        VERIFY_ARE_EQUAL(deviceId.size() * 2, output);
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        auto dispatcher = std::make_shared<workspace_core::WorkspaceMirrorRouteDispatcher>(recorder, workspace_core::WorkspaceMirrorRouteDispatcher::InputEffectSink{});
        auto agent = std::make_shared<workspace_core::WorkspaceMirrorNodeAgent>(dispatcher);
        auto session = std::make_shared<workspace_core::WorkspaceMirrorDeviceTunnelSession>(agent);
        workspace_core::WorkspaceMirrorDeviceTunnelClient client{ session };
        workspace_core::WorkspaceMirrorDeviceTunnelConnection connection;
        connection.Endpoint = endpoint;
        connection.DeviceId = deviceId;
        connection.OrganizationId = *organization;
        connection.DeviceIdText = *device;
        connection.UseDevelopmentIdentityHeaders = true;
        connection.DevelopmentCertificateSerial = *serial;
        VERIFY_IS_TRUE(client.Connect(connection));
        VERIFY_IS_TRUE(client.ReceiveOnce(2));
        VERIFY_ARE_EQUAL(1u, gsl::narrow<uint32_t>(agent->ActiveRouteCount()));
    }

    void MirrorCoreTests::DeviceTunnelClientRelaysOutputToLocalBrowser()
    {
        const auto environment = [](const wchar_t* name) {
            std::array<wchar_t, 256> value{};
            const auto length = GetEnvironmentVariableW(name, value.data(), gsl::narrow<DWORD>(value.size()));
            return length && length < value.size() ? std::optional<std::wstring>{ std::in_place, value.data(), length } : std::nullopt;
        };
        const auto organization = environment(L"TERMINAL_MIRROR_TEST_ORGANIZATION_ID");
        const auto device = environment(L"TERMINAL_MIRROR_TEST_DEVICE_ID");
        const auto serial = environment(L"TERMINAL_MIRROR_TEST_CERTIFICATE_SERIAL");
        const auto expectOutput = environment(L"TERMINAL_MIRROR_TEST_EXPECT_OUTPUT");
        const auto endpoint = environment(L"TERMINAL_MIRROR_TEST_TUNNEL_ENDPOINT").value_or(L"ws://127.0.0.1:8787/v1/device-tunnel");
        if (!organization || !device || !serial || !expectOutput)
        {
            Log::Comment(L"Local terminal-server output integration environment is not configured; skipping network assertion.");
            return;
        }
        std::array<uint8_t, 16> deviceId{};
        size_t output{};
        for (const auto character : *device)
        {
            if (character == '-') continue;
            const auto nibble = character >= L'0' && character <= L'9' ? character - L'0' :
                                character >= L'a' && character <= L'f' ? character - L'a' + 10 :
                                character >= L'A' && character <= L'F' ? character - L'A' + 10 : -1;
            VERIFY_IS_TRUE(nibble >= 0 && output < deviceId.size() * 2);
            if (output % 2 == 0) deviceId[output / 2] = gsl::narrow_cast<uint8_t>(nibble << 4);
            else deviceId[output / 2] |= gsl::narrow_cast<uint8_t>(nibble);
            ++output;
        }
        VERIFY_ARE_EQUAL(deviceId.size() * 2, output);
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        VERIFY_IS_TRUE(recorder->BeginWindow(L"agent", 24, 80, 1));
        auto dispatcher = std::make_shared<workspace_core::WorkspaceMirrorRouteDispatcher>(recorder, workspace_core::WorkspaceMirrorRouteDispatcher::InputEffectSink{});
        auto agent = std::make_shared<workspace_core::WorkspaceMirrorNodeAgent>(dispatcher);
        auto session = std::make_shared<workspace_core::WorkspaceMirrorDeviceTunnelSession>(agent);
        workspace_core::WorkspaceMirrorDeviceTunnelClient client{ session };
        workspace_core::WorkspaceMirrorDeviceTunnelConnection connection;
        connection.Endpoint = endpoint;
        connection.DeviceId = deviceId;
        connection.OrganizationId = *organization;
        connection.DeviceIdText = *device;
        connection.UseDevelopmentIdentityHeaders = true;
        connection.DevelopmentCertificateSerial = *serial;
        VERIFY_IS_TRUE(client.Connect(connection));
        VERIFY_IS_TRUE(client.ReceiveOnce(2));
        VERIFY_ARE_EQUAL(1u, gsl::narrow<uint32_t>(agent->ActiveRouteCount()));
        Sleep(1500); // The test driver binds the browser after attach receives RouteAccepted.
        VERIFY_IS_TRUE(client.SendOutput(L"agent", { 'o', 'k' }, 3));
    }

    void MirrorCoreTests::DeviceEnrollmentRedeemsAndConnectsWithMtls()
    {
        const auto environment = [](const wchar_t* name) {
            std::array<wchar_t, 256> value{};
            const auto length = GetEnvironmentVariableW(name, value.data(), gsl::narrow<DWORD>(value.size()));
            return length && length < value.size() ? std::optional<std::wstring>{ std::in_place, value.data(), length } : std::nullopt;
        };
        const auto organization = environment(L"TERMINAL_MIRROR_TEST_PAIRING_ORGANIZATION_ID");
        const auto requestId = environment(L"TERMINAL_MIRROR_TEST_PAIRING_REQUEST_ID");
        const auto pairingCode = environment(L"TERMINAL_MIRROR_TEST_PAIRING_CODE");
        const auto challenge = environment(L"TERMINAL_MIRROR_TEST_PAIRING_CHALLENGE");
        const auto device = environment(L"TERMINAL_MIRROR_TEST_PAIRING_DEVICE_ID");
        if (!organization || !requestId || !pairingCode || !challenge || !device)
        {
            Log::Comment(L"Local terminal-server mTLS pairing environment is not configured; skipping network assertion.");
            return;
        }
        std::array<uint8_t, 16> deviceId{};
        size_t output{};
        for (const auto character : *device)
        {
            if (character == '-') continue;
            const auto nibble = character >= L'0' && character <= L'9' ? character - L'0' :
                                character >= L'a' && character <= L'f' ? character - L'a' + 10 :
                                character >= L'A' && character <= L'F' ? character - L'A' + 10 : -1;
            VERIFY_IS_TRUE(nibble >= 0 && output < deviceId.size() * 2);
            if (output % 2 == 0) deviceId[output / 2] = gsl::narrow_cast<uint8_t>(nibble << 4);
            else deviceId[output / 2] |= gsl::narrow_cast<uint8_t>(nibble);
            ++output;
        }
        VERIFY_ARE_EQUAL(deviceId.size() * 2, output);
        workspace_core::WorkspaceMirrorDeviceKeyMaterial keyMaterial;
        VERIFY_IS_TRUE(workspace_core::CreateWorkspaceMirrorDeviceKeyRequest({ *device, L"mTLS integration device" }, keyMaterial));
        workspace_core::WorkspaceMirrorPairingRedemption redemption;
        redemption.Endpoint = environment(L"TERMINAL_MIRROR_TEST_PAIRING_ENDPOINT").value_or(L"https://localhost:9444");
        redemption.OrganizationId = *organization;
        redemption.RequestId = *requestId;
        redemption.PairingCode = *pairingCode;
        redemption.PublicKeyChallenge = *challenge;
        redemption.DeviceId = *device;
        redemption.DisplayName = L"mTLS integration device";
        redemption.KeyMaterial = keyMaterial;
        std::wstring certificateIdentifier;
        std::wstring expiresAt;
        VERIFY_IS_TRUE(workspace_core::RedeemWorkspaceMirrorDeviceCertificate(redemption, certificateIdentifier, expiresAt));
        VERIFY_IS_TRUE(certificateIdentifier.starts_with(L"sha256:"));
        VERIFY_IS_FALSE(expiresAt.empty());
        const auto certificate = workspace_core::OpenWorkspaceMirrorDeviceCertificate(certificateIdentifier);
        VERIFY_IS_NOT_NULL(certificate);
        workspace_core::WorkspaceNodeMirrorConfiguration config{ .Mode = workspace_core::WorkspaceNodeMirrorMode::NodeSession };
        auto recorder = std::make_shared<workspace_core::WorkspaceMirrorNodeRecorder>(config, L"workspace", L"node", L"session");
        auto dispatcher = std::make_shared<workspace_core::WorkspaceMirrorRouteDispatcher>(recorder, workspace_core::WorkspaceMirrorRouteDispatcher::InputEffectSink{});
        auto agent = std::make_shared<workspace_core::WorkspaceMirrorNodeAgent>(dispatcher);
        auto session = std::make_shared<workspace_core::WorkspaceMirrorDeviceTunnelSession>(agent);
        workspace_core::WorkspaceMirrorDeviceTunnelClient client{ session };
        workspace_core::WorkspaceMirrorDeviceTunnelConnection connection;
        connection.Endpoint = environment(L"TERMINAL_MIRROR_TEST_MTLS_TUNNEL_ENDPOINT").value_or(L"wss://localhost:9443/v1/device-tunnel");
        connection.DeviceId = deviceId;
        connection.OrganizationId = *organization;
        connection.DeviceIdText = *device;
        connection.ClientCertificateContext = certificate;
        VERIFY_IS_TRUE(client.Connect(connection));
        workspace_core::WorkspaceMirrorDirectoryProjection projection;
        projection.WorkspaceId = L"workspace-a";
        projection.NodeId = L"window-42";
        projection.NodeSessionId = L"window-42:session-7";
        projection.DisplayName = L"mTLS integration terminal";
        projection.Available = true;
        projection.WindowCount = 1;
        projection.Capabilities = { L"view", L"control" };
        projection.ProjectionVersion = 1;
        VERIFY_IS_TRUE(client.ReportDirectory(projection));
        VERIFY_IS_TRUE(client.SendHeartbeat());
        VERIFY_IS_TRUE(client.ReceiveOnce(1));
        workspace_core::CloseWorkspaceMirrorDeviceCertificate(certificate);
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
