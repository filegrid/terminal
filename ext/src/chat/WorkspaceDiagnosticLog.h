#pragma once

#include <Windows.h>
#include <json/json.h>

#include <string>
#include <string_view>

namespace terminal::workspacechat
{
    std::string DiagnosticUtf8(std::wstring_view value);
    void AddDiagnosticTextFields(Json::Value& object, std::string_view prefix, std::wstring_view value, size_t previewLimit = 160);
    void AddOptionalDiagnosticString(Json::Value& object, std::string_view key, std::wstring_view value);
    std::wstring WindowClassName(HWND hwnd);
    void AppendHwndDiagnostic(Json::Value& payload, std::string_view keyPrefix, HWND hwnd);
    void AppendGuiThreadFocusDiagnostics(Json::Value& payload, std::string_view keyPrefix);
    bool ActivateWindowForKeyboardInput(HWND hwnd);
    std::wstring NormalizeWorkspaceChatSubmitText(std::wstring_view text);
    bool AppendWorkspaceDiagnosticLog(std::wstring_view eventName, const Json::Value& payload);
}
