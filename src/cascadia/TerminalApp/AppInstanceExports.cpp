#include "pch.h"

#include "App.h"

extern "C" __declspec(dllexport) HRESULT __cdecl CreateTerminalAppAppInstance(void** instance) noexcept
{
    if (instance == nullptr)
    {
        return E_POINTER;
    }

    *instance = nullptr;

    try
    {
        auto app = winrt::make<winrt::TerminalApp::implementation::App>();
        *instance = winrt::detach_abi(app);
        return S_OK;
    }
    CATCH_RETURN();
}
