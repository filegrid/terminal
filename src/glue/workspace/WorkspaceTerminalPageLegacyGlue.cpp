// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "TerminalPage.h"
#include "../contracts/GluePageHostContract.h"

#include <TerminalCore/ControlKeyStates.hpp>
#include <TerminalThemeHelpers.h>
#include <winternl.h>
#include <til/hash.h>
#include <til/unicode.h>
#include <Utils.h>

#include "../../types/inc/ColorFix.hpp"
#include "../../types/inc/utils.hpp"
#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"
#include "DebugTapConnection.h"
#include "MarkdownPaneContent.h"
#include "Remoting.h"
#include "ScratchpadContent.h"
#include "SettingsPaneContent.h"
#include "SnippetsPaneContent.h"
#include "TabRowControl.h"
#include "TerminalSettingsCache.h"

#include "../../../../microsoft/src/cascadia/TerminalApp/WorkspacePageLegacyCppIncludes.h"

using namespace winrt;
using namespace winrt::Microsoft::Management::Deployment;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace ::TerminalApp;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Terminal::Core;
using namespace std::chrono_literals;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
    using VirtualKeyModifiers = Windows::System::VirtualKeyModifiers;
}

namespace
{
    #include "../../../../microsoft/src/cascadia/TerminalApp/WorkspacePageLegacyPrelude.inc"
}

namespace winrt::TerminalApp::implementation
{
    // Keep the legacy glue bodies intact for now, but redirect their workspace-owned
    // state references into the extension so ext-side changes stop re-expanding
    // storage back onto TerminalPage itself.
    #define _currentWorkspaceId (_workspaceExtension->CurrentWorkspaceIdState())
    #define _lastWorkspaceId (_workspaceExtension->LastWorkspaceIdState())
    #define _currentWorkspaceSaveBaseline (_workspaceExtension->CurrentWorkspaceSaveBaseline())
    #define _startupWorkspaceId (_workspaceExtension->StartupWorkspaceIdState())
    #define _workspaceChatOutputCaptureTimer (_workspaceExtension->WorkspaceChatOutputCaptureTimer())
    #define _workspaceSaverLayoutUpdatedRevoker (_workspaceExtension->WorkspaceSaverLayoutUpdatedRevoker())
    #define _workspaceSaverLayoutCount (_workspaceExtension->WorkspaceSaverLayoutCount())
    #define _workspaceSaverPressedEnter (_workspaceExtension->WorkspaceSaverPressedEnter())
    #define _workspaceEditorManager (_workspaceExtension->WorkspaceEditorManager())
    #define _workspaceEditorSelectedIndex (_workspaceExtension->WorkspaceEditorSelectedIndex())
    #define _workspaceManagerNavSelection (_workspaceExtension->WorkspaceManagerNavSelection())
    #define _workspaceEditorEditMode (_workspaceExtension->WorkspaceEditorEditMode())
    #define _workspaceDefinitionsDirty (_workspaceExtension->WorkspaceDefinitionsDirty())
    #include "../../../../microsoft/src/cascadia/TerminalApp/WorkspacePageLegacyGlue.inc"
    #undef _workspaceChatOutputCaptureTimer
    #undef _workspaceSaverPressedEnter
    #undef _workspaceSaverLayoutCount
    #undef _workspaceSaverLayoutUpdatedRevoker
    #undef _startupWorkspaceId
    #undef _currentWorkspaceSaveBaseline
    #undef _lastWorkspaceId
    #undef _currentWorkspaceId
    #undef _workspaceDefinitionsDirty
    #undef _workspaceEditorEditMode
    #undef _workspaceManagerNavSelection
    #undef _workspaceEditorSelectedIndex
    #undef _workspaceEditorManager
}
