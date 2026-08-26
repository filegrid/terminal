#include "../../core/chat/WorkspaceDiagnosticLog.h"

    // Glue owns the picker UI. TerminalPage owns only selection persistence and
    // diagnostics because those mutate its live workspace editor state.
    safe_void_coroutine TerminalPage::_ShowWorkspaceManagerWorkspaceIconPicker()
    {
        try
        {
            std::wstring initialIcon;
            if (const auto* current = _SelectedWorkspaceForEditing())
            {
                initialIcon = current->Icon;
            }

            const auto selectedIcon = co_await _workspaceExtension->PickWorkspaceManagerIcon(std::move(initialIcon), std::nullopt);
            const std::wstring iconValue{ selectedIcon.c_str() };
            if (!iconValue.empty())
            {
                _ApplyWorkspaceManagerWorkspaceIconSelection(iconValue);
            }
        }
        catch (...)
        {
            throw;
        }
    }

    safe_void_coroutine TerminalPage::_ShowWorkspaceManagerNodeIconPicker(const size_t nodeIndex)
    {
        Json::Value startPayload{ Json::objectValue };
        startPayload["nodeIndex"] = nodeIndex;
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_click", startPayload);

        try
        {
            std::wstring initialIcon;
            if (const auto* current = _SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
            {
                initialIcon = current->Nodes.at(nodeIndex).Icon;
            }

            const auto selectedIcon = co_await _workspaceExtension->PickWorkspaceManagerIcon(std::move(initialIcon), nodeIndex);
            const std::wstring iconValue{ selectedIcon.c_str() };
            Json::Value payload{ Json::objectValue };
            payload["nodeIndex"] = nodeIndex;
            std::wstring previousIcon;
            terminal::workspacechat::AddDiagnosticTextFields(payload, "selectedIcon", iconValue);
            if (const auto* currentBefore = _SelectedWorkspaceForEditing(); currentBefore && nodeIndex < currentBefore->Nodes.size())
            {
                previousIcon = currentBefore->Nodes.at(nodeIndex).Icon;
                terminal::workspacechat::AddDiagnosticTextFields(payload, "previousIcon", previousIcon);
            }
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_selected", payload);
            if (previousIcon == iconValue)
            {
                Json::Value noopPayload{ Json::objectValue };
                noopPayload["nodeIndex"] = nodeIndex;
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_noop_same_icon", noopPayload);
            }
            else if (!iconValue.empty())
            {
                _ApplyWorkspaceManagerNodeIconSelection(nodeIndex, iconValue);
            }
        }
        catch (const winrt::hresult_error& ex)
        {
            Json::Value payload{ Json::objectValue };
            payload["nodeIndex"] = nodeIndex;
            terminal::workspacechat::AppendExceptionDiagnostic(payload, ex);
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_exception", payload);
            throw;
        }
        catch (const std::exception& ex)
        {
            Json::Value payload{ Json::objectValue };
            payload["nodeIndex"] = nodeIndex;
            terminal::workspacechat::AppendExceptionDiagnostic(payload, ex);
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_exception", payload);
            throw;
        }
        catch (...)
        {
            Json::Value payload{ Json::objectValue };
            payload["nodeIndex"] = nodeIndex;
            terminal::workspacechat::AppendUnknownExceptionDiagnostic(payload);
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_exception", payload);
            throw;
        }
    }
