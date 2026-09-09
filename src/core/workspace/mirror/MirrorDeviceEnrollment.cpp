// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorDeviceEnrollment.h"

#include <Windows.h>
#include <array>
#include <certenroll.h>
#include <wincrypt.h>
#include <wrl/client.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ole32.lib")

// MirrorCore.cpp includes this implementation inside terminal::workspace.

namespace
{
    using Microsoft::WRL::ComPtr;

    bool _bstr(const BSTR value, std::wstring& target)
    {
        if (!value) return false;
        target.assign(value, SysStringLen(value));
        SysFreeString(value);
        return !target.empty();
    }

    bool _certificateDer(const std::wstring_view pem, std::vector<uint8_t>& der)
    {
        DWORD length{};
        if (!CryptStringToBinaryW(pem.data(), gsl::narrow<DWORD>(pem.size()), CRYPT_STRING_BASE64_ANY, nullptr, &length, nullptr, nullptr) || !length) return false;
        der.resize(length);
        return CryptStringToBinaryW(pem.data(), gsl::narrow<DWORD>(pem.size()), CRYPT_STRING_BASE64_ANY, der.data(), &length, nullptr, nullptr) != FALSE;
    }
}

bool CreateWorkspaceMirrorDeviceKeyRequest(const WorkspaceMirrorDeviceKeyRequest& request,
                                           WorkspaceMirrorDeviceKeyMaterial& material)
{
    if (request.DeviceId.empty() || request.DeviceId.size() > 64 || request.DisplayName.empty() || request.DisplayName.size() > 128) return false;
    const auto container = L"WindowsTerminalMirror-" + request.DeviceId;
    ComPtr<IX509PrivateKey> key;
    ComPtr<IX509CertificateRequestPkcs10> csr;
    ComPtr<IX500DistinguishedName> subject;
    if (FAILED(CoCreateInstance(__uuidof(CX509PrivateKey), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&key))) ||
        FAILED(key->put_ProviderName(SysAllocString(L"Microsoft Software Key Storage Provider"))) ||
        FAILED(key->put_ContainerName(SysAllocString(container.c_str()))) ||
        FAILED(key->put_MachineContext(VARIANT_FALSE)) ||
        FAILED(key->put_Length(2048)) ||
        FAILED(key->put_KeySpec(XCN_AT_SIGNATURE)) ||
        FAILED(key->put_ExportPolicy(XCN_NCRYPT_ALLOW_EXPORT_NONE)) ||
        FAILED(key->Create())) return false;
    if (FAILED(CoCreateInstance(__uuidof(CX509CertificateRequestPkcs10), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&csr))) ||
        FAILED(csr->InitializeFromPrivateKey(ContextUser, key.Get(), nullptr)) ||
        FAILED(CoCreateInstance(__uuidof(CX500DistinguishedName), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&subject)))) return false;
    const auto commonName = L"CN=terminal-device-" + request.DeviceId;
    if (FAILED(subject->Encode(SysAllocString(commonName.c_str()), XCN_CERT_NAME_STR_NONE)) ||
        FAILED(csr->put_Subject(subject.Get())) || FAILED(csr->Encode())) return false;
    BSTR encoded{};
    if (FAILED(csr->get_RawData(XCN_CRYPT_STRING_BASE64REQUESTHEADER, &encoded))) return false;
    std::wstring pem;
    if (!_bstr(encoded, pem)) return false;
    ComPtr<IX509PublicKey> publicKey;
    BSTR encodedPublicKey{};
    if (FAILED(csr->get_PublicKey(&publicKey)) ||
        FAILED(publicKey->get_EncodedKey(XCN_CRYPT_STRING_BASE64, &encodedPublicKey))) return false;
    std::wstring publicKeyBase64;
    if (!_bstr(encodedPublicKey, publicKeyBase64)) return false;
    // This is the encoded public-key bit string, never a private-key export.
    // It gives the inventory field its literal sha256(public-key) meaning.
    std::vector<uint8_t> bytes;
    if (!_certificateDer(publicKeyBase64, bytes)) return false;
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD objectLength{}, resultLength{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &resultLength, 0) < 0) return false;
    std::vector<uint8_t> object(objectLength), digest(32);
    const auto hashed = BCryptCreateHash(algorithm, &hash, object.data(), gsl::narrow<ULONG>(object.size()), nullptr, 0, 0) >= 0 &&
                        BCryptHashData(hash, bytes.data(), gsl::narrow<ULONG>(bytes.size()), 0) >= 0 &&
                        BCryptFinishHash(hash, digest.data(), gsl::narrow<ULONG>(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!hashed) return false;
    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring fingerprint{ L"sha256:" };
    for (const auto byte : digest) { fingerprint.push_back(hex[byte >> 4]); fingerprint.push_back(hex[byte & 0xf]); }
    material = { container, std::move(pem), std::move(fingerprint) };
    return true;
}

bool InstallWorkspaceMirrorDeviceCertificate(const std::wstring_view keyContainerName,
                                             const std::wstring_view certificateChainPem)
{
    std::vector<uint8_t> der;
    if (keyContainerName.empty() || !_certificateDer(certificateChainPem, der)) return false;
    const auto certificate = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, der.data(), gsl::narrow<DWORD>(der.size()));
    if (!certificate) return false;
    auto freeCertificate = wil::scope_exit([&] { CertFreeCertificateContext(certificate); });
    CRYPT_KEY_PROV_INFO keyInfo{};
    keyInfo.pwszContainerName = const_cast<wchar_t*>(keyContainerName.data());
    keyInfo.pwszProvName = const_cast<wchar_t*>(L"Microsoft Software Key Storage Provider");
    keyInfo.dwProvType = 0;
    keyInfo.dwKeySpec = AT_SIGNATURE;
    if (!CertSetCertificateContextProperty(certificate, CERT_KEY_PROV_INFO_PROP_ID, 0, &keyInfo)) return false;
    const auto store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, static_cast<HCRYPTPROV_LEGACY>(0), CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    if (!store) return false;
    auto closeStore = wil::scope_exit([&] { CertCloseStore(store, 0); });
    return CertAddCertificateContextToStore(store, certificate, CERT_STORE_ADD_REPLACE_EXISTING, nullptr) != FALSE;
}

PCCERT_CONTEXT OpenWorkspaceMirrorDeviceCertificate(const std::wstring_view certificateIdentifier)
{
    constexpr std::wstring_view prefix{ L"sha256:" };
    if (!certificateIdentifier.starts_with(prefix) || certificateIdentifier.size() != prefix.size() + 64) return nullptr;
    const auto store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, static_cast<HCRYPTPROV_LEGACY>(0), CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    if (!store) return nullptr;
    auto closeStore = wil::scope_exit([&] { CertCloseStore(store, 0); });
    for (PCCERT_CONTEXT certificate{}; (certificate = CertEnumCertificatesInStore(store, certificate)) != nullptr;)
    {
        std::array<uint8_t, 32> digest{};
        DWORD length = gsl::narrow<DWORD>(digest.size());
        if (!CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr, certificate->pbCertEncoded, certificate->cbCertEncoded, digest.data(), &length) || length != digest.size()) continue;
        constexpr wchar_t hex[] = L"0123456789abcdef";
        bool matches = true;
        for (size_t index = 0; index < digest.size(); ++index)
        {
            const auto expected = certificateIdentifier[prefix.size() + index * 2];
            const auto expectedLow = certificateIdentifier[prefix.size() + index * 2 + 1];
            if (expected != hex[digest[index] >> 4] || expectedLow != hex[digest[index] & 0xf])
            {
                matches = false;
                break;
            }
        }
        if (matches) return CertDuplicateCertificateContext(certificate);
    }
    return nullptr;
}

void CloseWorkspaceMirrorDeviceCertificate(const PCCERT_CONTEXT certificate) noexcept
{
    if (certificate) CertFreeCertificateContext(certificate);
}
