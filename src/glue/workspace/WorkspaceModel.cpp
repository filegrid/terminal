// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "WorkspaceApi.h"

#include "../../../microsoft/src/types/inc/ColorFix.hpp"
#include "../../../microsoft/src/types/inc/utils.hpp"

using namespace Microsoft::Console;

// This TU owns the workspace core/facade include chain, open-state checks, and diagnostics.
#include "../../core/workspace/WorkspaceCore.cpp"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    namespace
    {
        #include "WorkspaceUtilityHelpers.cpp"

        #include "WorkspaceStartupColorLogic.cpp"

        #include "WorkspaceFacadeConversionHelpers.cpp"

    }
    #include "WorkspacePaletteHelpers.cpp"
    #include "WorkspaceFacadeEditorStateMethods.cpp"
    #include "WorkspaceFacadeStartupMethods.cpp"
    #include "WorkspaceFacadePersistenceMethods.cpp"
    #include "WorkspaceFacadeMethods.cpp"
}
