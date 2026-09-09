// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <wincrypt.h>

#include <string>

namespace terminal::workspace
{
    // Current-user, non-exportable CNG key material used by a paired device.
    // Only the CSR leaves this machine; certificate responses are bound to the
    // named key container before entering the Windows certificate store.
    struct WorkspaceMirrorDeviceKeyRequest
    {
        std::wstring DeviceId;
        std::wstring DisplayName;
    };

    struct WorkspaceMirrorDeviceKeyMaterial
    {
        std::wstring KeyContainerName;
        std::wstring CertificateRequestPem;
        std::wstring PublicKeyFingerprint;
    };

    bool CreateWorkspaceMirrorDeviceKeyRequest(const WorkspaceMirrorDeviceKeyRequest& request,
                                               WorkspaceMirrorDeviceKeyMaterial& material);
    bool InstallWorkspaceMirrorDeviceCertificate(std::wstring_view keyContainerName,
                                                 std::wstring_view certificateChainPem);

    // Opens a duplicate certificate context from CurrentUser\\My. The caller
    // owns the returned context and must release it with the matching helper.
    // The context retains the Windows key-provider association; the private
    // key is never exported into the agent process or onto disk.
    PCCERT_CONTEXT OpenWorkspaceMirrorDeviceCertificate(std::wstring_view certificateIdentifier);
    void CloseWorkspaceMirrorDeviceCertificate(PCCERT_CONTEXT certificate) noexcept;

    struct WorkspaceMirrorPairingRedemption
    {
        std::wstring Endpoint;
        std::wstring OrganizationId;
        std::wstring RequestId;
        std::wstring PairingCode;
        std::wstring PublicKeyChallenge;
        std::wstring DeviceId;
        std::wstring DisplayName;
        WorkspaceMirrorDeviceKeyMaterial KeyMaterial;
    };

}
