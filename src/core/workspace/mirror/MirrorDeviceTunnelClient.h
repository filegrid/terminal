// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorDeviceTunnelSession.h"
#include "MirrorDirectoryProjection.h"
#include "MirrorDeviceEnrollment.h"

namespace terminal::workspace
{
    // Connection details deliberately keep development identity separate from
    // client-certificate authentication. Development headers are accepted only
    // for a loopback ws endpoint; production always uses wss and mTLS.
    struct WorkspaceMirrorDeviceTunnelConnection
    {
        std::wstring Endpoint;
        std::array<uint8_t, 16> DeviceId{};
        std::wstring OrganizationId;
        std::wstring DeviceIdText;
        const void* ClientCertificateContext{};
        bool UseDevelopmentIdentityHeaders{};
        std::wstring DevelopmentCertificateSerial;
    };

    // Synchronous WinHTTP owner for terminal-server's device tunnel. Callers
    // run ReceiveOnce from their connection worker and call SendOutput from the
    // terminal output callback. It does not interpret terminal payloads.
    class WorkspaceMirrorDeviceTunnelClient
    {
    public:
        explicit WorkspaceMirrorDeviceTunnelClient(std::shared_ptr<WorkspaceMirrorDeviceTunnelSession> session);
        ~WorkspaceMirrorDeviceTunnelClient();
        WorkspaceMirrorDeviceTunnelClient(const WorkspaceMirrorDeviceTunnelClient&) = delete;
        WorkspaceMirrorDeviceTunnelClient& operator=(const WorkspaceMirrorDeviceTunnelClient&) = delete;

        bool Connect(const WorkspaceMirrorDeviceTunnelConnection& connection);
        bool ReceiveOnce(uint64_t nowMilliseconds);
        bool SendHeartbeat();
        bool SendOutput(std::wstring_view commandId, std::vector<uint8_t> bytes, uint64_t timestampMilliseconds);
        bool ReportDirectory(const WorkspaceMirrorDirectoryProjection& projection);
        void Close() noexcept;
        bool Connected() const noexcept;
        const std::wstring& ConnectionId() const noexcept;

    private:
        bool _send(std::span<const uint8_t> bytes);
        bool _sendAll(const std::vector<std::vector<uint8_t>>& frames);
        bool _receiveWelcome();
        bool _receiveBinary(std::vector<uint8_t>& bytes);

        std::shared_ptr<WorkspaceMirrorDeviceTunnelSession> _session;
        std::array<uint8_t, 16> _deviceId{};
        void* _sessionHandle{};
        void* _connectionHandle{};
        void* _webSocket{};
        std::wstring _connectionId;
        WorkspaceMirrorDeviceTunnelConnection _connectionSettings;
    };

    // HTTPS-only one-time redemption; implemented with the same WinHTTP
    // transport boundary as the device tunnel, not with a Core WinRT API.
    bool RedeemWorkspaceMirrorDeviceCertificate(const WorkspaceMirrorPairingRedemption& redemption,
                                                std::wstring& certificateIdentifier,
                                                std::wstring& expiresAt);
}
