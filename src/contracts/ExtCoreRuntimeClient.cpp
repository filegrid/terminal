// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "ExtCoreRuntimeClient.h"

#include <array>
#include <string>
#include <winrt/base.h>

namespace terminal::extcore
{
    const RuntimeClient& RuntimeClient::Shared()
    {
        static const RuntimeClient client;
        return client;
    }

    RuntimeClient::RuntimeClient()
    {
        std::array<wchar_t, 32768> executablePath{};
        const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length >= executablePath.size())
        {
            throw winrt::hresult_error{ HRESULT_FROM_WIN32(GetLastError()) };
        }

        auto modulePath = std::filesystem::path{ executablePath.data() };
        modulePath.replace_filename(L"Ext.dll");
        _module = LoadLibraryExW(modulePath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!_module)
        {
            throw winrt::hresult_error{ HRESULT_FROM_WIN32(GetLastError()) };
        }

        const auto getApiVersion = reinterpret_cast<terminal::workspace::GetExtCoreApiVersionFn>(
            GetProcAddress(_module, terminal::workspace::GetExtCoreApiVersionSymbol));
        _getWorkspaceDefinitionsPath = reinterpret_cast<terminal::workspace::GetExtWorkspaceDefinitionsPathFn>(
            GetProcAddress(_module, terminal::workspace::GetExtWorkspaceDefinitionsPathSymbol));
        _removePersistedWorkspaceWindowState = reinterpret_cast<terminal::workspace::RemoveExtPersistedWorkspaceWindowStateFn>(
            GetProcAddress(_module, terminal::workspace::RemoveExtPersistedWorkspaceWindowStateSymbol));
        if (!getApiVersion || getApiVersion() != terminal::workspace::ExtCoreApiVersion ||
            !_getWorkspaceDefinitionsPath || !_removePersistedWorkspaceWindowState)
        {
            FreeLibrary(_module);
            _module = nullptr;
            throw winrt::hresult_error{ E_NOINTERFACE };
        }
    }

    RuntimeClient::~RuntimeClient()
    {
        if (_module)
        {
            FreeLibrary(_module);
        }
    }

    std::filesystem::path RuntimeClient::WorkspaceDefinitionsPath() const
    {
        uint32_t length{};
        const auto initialResult = _getWorkspaceDefinitionsPath(nullptr, 0, &length);
        if (initialResult != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
        {
            throw winrt::hresult_error{ initialResult };
        }

        std::wstring value(length, L'\0');
        value.push_back(L'\0');
        if (const auto result = _getWorkspaceDefinitionsPath(value.data(), static_cast<uint32_t>(value.size()), &length); FAILED(result))
        {
            throw winrt::hresult_error{ result };
        }
        value.resize(length);
        return std::filesystem::path{ value };
    }

    void RuntimeClient::RemovePersistedWorkspaceWindowState(const uint64_t windowId) const
    {
        if (const auto result = _removePersistedWorkspaceWindowState(windowId); FAILED(result))
        {
            throw winrt::hresult_error{ result };
        }
    }
}
