// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorDeviceTunnelClient.h"

#include <Windows.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>

#pragma comment(lib, "winhttp.lib")

// MirrorCore.cpp includes this implementation inside terminal::workspace.

namespace
{
    constexpr size_t welcomeLimit = 4096;

    bool _isLoopbackHost(const std::wstring_view host)
    {
        return host == L"localhost" || host == L"127.0.0.1" || host == L"[::1]";
    }

    bool _parseEndpoint(const std::wstring_view endpoint,
                        bool& secure,
                        std::wstring& host,
                        INTERNET_PORT& port,
                        std::wstring& path)
    {
        constexpr std::wstring_view wss{ L"wss://" };
        constexpr std::wstring_view ws{ L"ws://" };
        size_t authorityOffset{};
        if (endpoint.starts_with(wss))
        {
            secure = true;
            authorityOffset = wss.size();
        }
        else if (endpoint.starts_with(ws))
        {
            secure = false;
            authorityOffset = ws.size();
        }
        else
        {
            return false;
        }
        const auto pathOffset = endpoint.find(L'/', authorityOffset);
        const auto authority = endpoint.substr(authorityOffset, pathOffset == std::wstring_view::npos ? endpoint.size() - authorityOffset : pathOffset - authorityOffset);
        if (authority.empty()) return false;
        path = pathOffset == std::wstring_view::npos ? L"/" : std::wstring{ endpoint.substr(pathOffset) };
        host = std::wstring{ authority };
        port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        if (authority.front() == L'[')
        {
            const auto close = authority.find(L']');
            if (close == std::wstring_view::npos) return false;
            host = std::wstring{ authority.substr(0, close + 1) };
            if (close + 1 < authority.size())
            {
                if (authority[close + 1] != L':') return false;
                const auto text = authority.substr(close + 2);
                if (text.empty()) return false;
                try { port = gsl::narrow<INTERNET_PORT>(std::stoul(std::wstring{ text })); }
                catch (...) { return false; }
            }
            return true;
        }
        const auto separator = authority.rfind(L':');
        if (separator != std::wstring_view::npos)
        {
            const auto text = authority.substr(separator + 1);
            if (text.empty()) return false;
            host = std::wstring{ authority.substr(0, separator) };
            try { port = gsl::narrow<INTERNET_PORT>(std::stoul(std::wstring{ text })); }
            catch (...) { return false; }
        }
        return !host.empty();
    }

    HINTERNET _handle(void* value) noexcept
    {
        return static_cast<HINTERNET>(value);
    }
}

WorkspaceMirrorDeviceTunnelClient::WorkspaceMirrorDeviceTunnelClient(std::shared_ptr<WorkspaceMirrorDeviceTunnelSession> session) :
    _session{ std::move(session) }
{
}

WorkspaceMirrorDeviceTunnelClient::~WorkspaceMirrorDeviceTunnelClient()
{
    Close();
}

bool WorkspaceMirrorDeviceTunnelClient::Connect(const WorkspaceMirrorDeviceTunnelConnection& connection)
{
    Close();
    if (!_session || connection.Endpoint.empty()) return false;
    bool secure{};
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port{};
    if (!_parseEndpoint(connection.Endpoint, secure, host, port, path)) return false;
    if (connection.OrganizationId.empty() || connection.DeviceIdText.empty()) return false;
    if (connection.UseDevelopmentIdentityHeaders)
    {
        if (secure || !_isLoopbackHost(host) || connection.DevelopmentCertificateSerial.empty()) return false;
    }
    else if (!secure || !connection.ClientCertificateContext)
    {
        return false;
    }

    const auto session = WinHttpOpen(L"WindowsTerminalMirror/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!session) return false;
    const auto connectionHandle = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connectionHandle)
    {
        WinHttpCloseHandle(session);
        return false;
    }
    const auto request = WinHttpOpenRequest(connectionHandle, L"GET", path.c_str(), nullptr, nullptr, nullptr, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request)
    {
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(session);
        return false;
    }
    bool valid = WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) != FALSE;
    if (valid && !connection.UseDevelopmentIdentityHeaders)
    {
        valid = WinHttpSetOption(request, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, const_cast<void*>(connection.ClientCertificateContext), sizeof(CERT_CONTEXT)) != FALSE;
    }
    if (valid)
    {
        auto headers = L"x-terminal-organization-id: " + connection.OrganizationId + L"\r\n" +
                       L"x-terminal-device-id: " + connection.DeviceIdText;
        if (connection.UseDevelopmentIdentityHeaders)
        {
            headers += L"\r\nx-terminal-client-cert-serial: " + connection.DevelopmentCertificateSerial;
        }
        valid = WinHttpAddRequestHeaders(request, headers.c_str(), gsl::narrow<DWORD>(headers.size()), WINHTTP_ADDREQ_FLAG_ADD) != FALSE;
    }
    if (valid) valid = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) != FALSE;
    if (valid) valid = WinHttpReceiveResponse(request, nullptr) != FALSE;
    const auto socket = valid ? WinHttpWebSocketCompleteUpgrade(request, 0) : nullptr;
    WinHttpCloseHandle(request);
    if (!socket)
    {
        WinHttpCloseHandle(connectionHandle);
        WinHttpCloseHandle(session);
        return false;
    }
    _sessionHandle = session;
    _connectionHandle = connectionHandle;
    _webSocket = socket;
    _deviceId = connection.DeviceId;
    _connectionSettings = connection;
    if (_receiveWelcome()) return true;
    Close();
    return false;
}

bool WorkspaceMirrorDeviceTunnelClient::_receiveWelcome()
{
    std::array<char, welcomeLimit> buffer{};
    DWORD read{};
    WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
    if (WinHttpWebSocketReceive(_handle(_webSocket), buffer.data(), gsl::narrow<DWORD>(buffer.size()), &read, &type) != NO_ERROR) return false;
    if (type != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) return false;
    const std::string_view welcome{ buffer.data(), read };
    constexpr std::string_view prefix{ "\"connectionId\":\"" };
    const auto position = welcome.find(prefix);
    if (welcome.find("\"kind\":\"welcome\"") == std::string_view::npos ||
        welcome.find("\"heartbeatIntervalSeconds\":") == std::string_view::npos || position == std::string_view::npos)
    {
        return false;
    }
    const auto begin = position + prefix.size();
    const auto end = welcome.find('"', begin);
    if (end == std::string_view::npos || end == begin || end - begin > 64) return false;
    _connectionId.clear();
    _connectionId.reserve(end - begin);
    for (auto index = begin; index < end; ++index)
    {
        _connectionId.push_back(static_cast<wchar_t>(welcome[index]));
    }
    return true;
}

bool WorkspaceMirrorDeviceTunnelClient::_receiveBinary(std::vector<uint8_t>& bytes)
{
    bytes.clear();
    std::array<uint8_t, 16 * 1024> buffer{};
    WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
    do
    {
        DWORD read{};
        if (WinHttpWebSocketReceive(_handle(_webSocket), buffer.data(), gsl::narrow<DWORD>(buffer.size()), &read, &type) != NO_ERROR) return false;
        if (type != WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE && type != WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) return false;
        if (bytes.size() + read > WorkspaceMirrorRelayHeaderLength + WorkspaceMirrorMaximumTerminalPayload) return false;
        bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + read);
    } while (type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE);
    return true;
}

bool WorkspaceMirrorDeviceTunnelClient::_send(const std::span<const uint8_t> bytes)
{
    return _webSocket && !bytes.empty() && WinHttpWebSocketSend(_handle(_webSocket), WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, const_cast<uint8_t*>(bytes.data()), gsl::narrow<DWORD>(bytes.size())) == NO_ERROR;
}

bool WorkspaceMirrorDeviceTunnelClient::_sendAll(const std::vector<std::vector<uint8_t>>& frames)
{
    return std::all_of(frames.cbegin(), frames.cend(), [this](const auto& frame) { return _send(frame); });
}

bool WorkspaceMirrorDeviceTunnelClient::ReceiveOnce(const uint64_t nowMilliseconds)
{
    std::vector<uint8_t> inbound;
    std::vector<std::vector<uint8_t>> outbound;
    return Connected() && _receiveBinary(inbound) && _session->HandleBinary(inbound, nowMilliseconds, outbound) && _sendAll(outbound);
}

bool WorkspaceMirrorDeviceTunnelClient::SendHeartbeat()
{
    std::vector<uint8_t> ping;
    return Connected() && _session->BuildHeartbeat(_deviceId, ping) && _send(ping);
}

bool WorkspaceMirrorDeviceTunnelClient::SendOutput(const std::wstring_view commandId,
                                                    std::vector<uint8_t> bytes,
                                                    const uint64_t timestampMilliseconds)
{
    std::vector<std::vector<uint8_t>> outbound;
    return Connected() && _session->PublishOutput(commandId, std::move(bytes), timestampMilliseconds, outbound) && _sendAll(outbound);
}

bool WorkspaceMirrorDeviceTunnelClient::ReportDirectory(const WorkspaceMirrorDirectoryProjection& projection)
{
    if (!Connected() || _connectionId.empty() || projection.WorkspaceId.empty() || projection.NodeId.empty() ||
        projection.NodeSessionId.empty() || projection.DisplayName.empty() ||
        projection.WindowCount == 0 || projection.ProjectionVersion == 0 || projection.Capabilities.empty()) return false;
    bool secure{};
    std::wstring host;
    std::wstring ignoredPath;
    INTERNET_PORT port{};
    if (!_parseEndpoint(_connectionSettings.Endpoint, secure, host, port, ignoredPath)) return false;
    const auto session = WinHttpOpen(L"WindowsTerminalMirror/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    const auto connection = session ? WinHttpConnect(session, host.c_str(), port, 0) : nullptr;
    const auto request = connection ? WinHttpOpenRequest(connection, L"POST", L"/v1/device-tunnel/directory", nullptr, nullptr, nullptr, secure ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    auto close = wil::scope_exit([&] {
        if (request) WinHttpCloseHandle(request);
        if (connection) WinHttpCloseHandle(connection);
        if (session) WinHttpCloseHandle(session);
    });
    if (!request) return false;
    bool valid = true;
    if (!_connectionSettings.UseDevelopmentIdentityHeaders)
    {
        valid = WinHttpSetOption(request, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, const_cast<void*>(_connectionSettings.ClientCertificateContext), sizeof(CERT_CONTEXT)) != FALSE;
    }
    auto headers = L"content-type: application/json\r\nx-terminal-organization-id: " + _connectionSettings.OrganizationId +
                   L"\r\nx-terminal-device-id: " + _connectionSettings.DeviceIdText + L"\r\nx-terminal-connection-id: " + _connectionId;
    if (_connectionSettings.UseDevelopmentIdentityHeaders) headers += L"\r\nx-terminal-client-cert-serial: " + _connectionSettings.DevelopmentCertificateSerial;
    if (valid) valid = WinHttpAddRequestHeaders(request, headers.c_str(), gsl::narrow<DWORD>(headers.size()), WINHTTP_ADDREQ_FLAG_ADD) != FALSE;
    const auto quote = [](const std::wstring_view value, std::string& output) {
        output.push_back('"');
        for (const auto character : value)
        {
            if (character == L'"' || character == L'\\') { output.push_back('\\'); output.push_back(gsl::narrow_cast<char>(character)); }
            else if (character < 0x20) return false;
            else
            {
                const auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, &character, 1, nullptr, 0, nullptr, nullptr);
                if (length <= 0) return false;
                const auto begin = output.size(); output.resize(begin + gsl::narrow<size_t>(length));
                if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, &character, 1, output.data() + begin, length, nullptr, nullptr) != length) return false;
            }
        }
        output.push_back('"'); return true;
    };
    std::string body{"{"};
    const auto field = [&](const char* name, const std::wstring_view value) { body += '"'; body += name; body += "\":"; return quote(value, body); };
    if (!field("resourceKey", projection.WorkspaceId + L":" + projection.NodeId)) return false; body += ',';
    if (!field("workspaceKey", projection.WorkspaceId)) return false; body += ',';
    if (!field("nodeKey", projection.NodeId)) return false; body += ',';
    if (!field("nodeSessionKey", projection.NodeSessionId)) return false; body += ',';
    if (!field("displayName", projection.DisplayName)) return false;
    body += projection.Available ? ",\"availability\":\"online\",\"windowCount\":" : ",\"availability\":\"offline\",\"windowCount\":";
    body += std::to_string(projection.WindowCount) + ",\"capabilities\":[";
    for (size_t index{}; index < projection.Capabilities.size(); ++index) { if (index) body += ','; if (!quote(projection.Capabilities[index], body)) return false; }
    body += "],\"projectionVersion\":" + std::to_string(projection.ProjectionVersion) + '}';
    if (valid) valid = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body.data(), gsl::narrow<DWORD>(body.size()), gsl::narrow<DWORD>(body.size()), 0) != FALSE;
    if (valid) valid = WinHttpReceiveResponse(request, nullptr) != FALSE;
    DWORD status{}; DWORD length{ sizeof(status) };
    return valid && WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &length, nullptr) != FALSE && status == 204;
}

void WorkspaceMirrorDeviceTunnelClient::Close() noexcept
{
    if (_webSocket) { WinHttpWebSocketClose(_handle(_webSocket), WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0); WinHttpCloseHandle(_handle(_webSocket)); }
    if (_connectionHandle) WinHttpCloseHandle(_handle(_connectionHandle));
    if (_sessionHandle) WinHttpCloseHandle(_handle(_sessionHandle));
    _webSocket = nullptr;
    _connectionHandle = nullptr;
    _sessionHandle = nullptr;
    _connectionId.clear();
    _connectionSettings = {};
}

bool WorkspaceMirrorDeviceTunnelClient::Connected() const noexcept
{
    return _webSocket != nullptr;
}

const std::wstring& WorkspaceMirrorDeviceTunnelClient::ConnectionId() const noexcept
{
    return _connectionId;
}

bool RedeemWorkspaceMirrorDeviceCertificate(const WorkspaceMirrorPairingRedemption& redemption,
                                            std::wstring& certificateIdentifier,
                                            std::wstring& expiresAt)
{
    certificateIdentifier.clear();
    expiresAt.clear();
    if (!redemption.Endpoint.starts_with(L"https://") || redemption.OrganizationId.empty() || redemption.RequestId.empty() ||
        redemption.PairingCode.empty() || redemption.PublicKeyChallenge.empty() || redemption.DeviceId.empty() ||
        redemption.DisplayName.empty() || redemption.KeyMaterial.KeyContainerName.empty() || redemption.KeyMaterial.CertificateRequestPem.empty()) return false;
    wchar_t host[256]{};
    URL_COMPONENTS url{ .dwStructSize = sizeof(url), .lpszHostName = host, .dwHostNameLength = ARRAYSIZE(host) - 1 };
    if (!WinHttpCrackUrl(redemption.Endpoint.c_str(), gsl::narrow<DWORD>(redemption.Endpoint.size()), 0, &url) || url.nScheme != INTERNET_SCHEME_HTTPS) return false;
    const auto toUtf8 = [](const std::wstring_view value, std::string& result) {
        const auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), gsl::narrow<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (length < 0) return false; result.resize(gsl::narrow<size_t>(length));
        return !length || WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), gsl::narrow<int>(value.size()), result.data(), length, nullptr, nullptr) == length;
    };
    const auto quote = [&](const std::wstring_view value, std::string& result) {
        std::string utf8; if (!toUtf8(value, utf8)) return false; result.push_back('"');
        for (const auto c : utf8) { if (c == '"' || c == '\\') result.push_back('\\'); if (static_cast<unsigned char>(c) < 0x20) return false; result.push_back(c); }
        result.push_back('"'); return true;
    };
    std::string body{"{"};
    const auto field = [&](const char* name, const std::wstring_view value) { body += '"'; body += name; body += "\":"; return quote(value, body); };
    if (!field("pairingCode", redemption.PairingCode)) return false; body += ',';
    if (!field("publicKeyChallenge", redemption.PublicKeyChallenge)) return false; body += ',';
    if (!field("deviceId", redemption.DeviceId)) return false; body += ',';
    if (!field("displayName", redemption.DisplayName)) return false; body += ',';
    if (!field("publicKeyFingerprint", redemption.KeyMaterial.PublicKeyFingerprint)) return false; body += ',';
    if (!field("certificateRequestPem", redemption.KeyMaterial.CertificateRequestPem)) return false; body += '}';
    const auto path = L"/api/v1/pairing-requests/" + redemption.OrganizationId + L"/" + redemption.RequestId + L"/redeem";
    const auto session = WinHttpOpen(L"WindowsTerminalMirror/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    const auto connection = session ? WinHttpConnect(session, host, url.nPort, 0) : nullptr;
    const auto request = connection ? WinHttpOpenRequest(connection, L"POST", path.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
    auto close = wil::scope_exit([&] { if (request) WinHttpCloseHandle(request); if (connection) WinHttpCloseHandle(connection); if (session) WinHttpCloseHandle(session); });
    if (!request || !WinHttpAddRequestHeaders(request, L"content-type: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD) ||
        !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body.data(), gsl::narrow<DWORD>(body.size()), gsl::narrow<DWORD>(body.size()), 0) || !WinHttpReceiveResponse(request, nullptr)) return false;
    DWORD status{}, size{ sizeof(status) };
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr) || status != 201) return false;
    std::string response; DWORD available{};
    while (WinHttpQueryDataAvailable(request, &available) && available) { if (response.size() + available > 64 * 1024) return false; const auto begin = response.size(); response.resize(begin + available); DWORD read{}; if (!WinHttpReadData(request, response.data() + begin, available, &read)) return false; response.resize(begin + read); }
    const auto value = [&](const std::string_view name, std::wstring& output) {
        const auto prefix = std::string{"\""} + std::string{name} + "\":\""; const auto begin = response.find(prefix); if (begin == std::string::npos) return false; const auto start = begin + prefix.size(); const auto end = response.find('"', start); if (end == std::string::npos) return false;
        std::string text = response.substr(start, end - start); size_t offset{}; while ((offset = text.find("\\n", offset)) != std::string::npos) { text.replace(offset, 2, "\n"); ++offset; }
        const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), gsl::narrow<int>(text.size()), nullptr, 0); if (length <= 0) return false; output.resize(gsl::narrow<size_t>(length)); return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), gsl::narrow<int>(text.size()), output.data(), length) == length;
    };
    std::wstring chain;
    if (!value("certificateChainPem", chain) || !value("certificateIdentifier", certificateIdentifier) || !value("expiresAt", expiresAt) || !certificateIdentifier.starts_with(L"sha256:")) return false;
    return InstallWorkspaceMirrorDeviceCertificate(redemption.KeyMaterial.KeyContainerName, chain);
}
