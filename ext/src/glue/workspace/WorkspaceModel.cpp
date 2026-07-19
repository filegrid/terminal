// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "WorkspaceApi.h"

#include "../../../microsoft/src/types/inc/ColorFix.hpp"
#include "../../../microsoft/src/types/inc/utils.hpp"

using namespace Microsoft::Console;

#include "../../core/workspace/WorkspaceCore.cpp"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    namespace
    {
        static constexpr std::array<std::wstring_view, 12> _workspaceNodeColorPalette{
            L"#C50F1F",
            L"#0063B1",
            L"#0F7B0F",
            L"#CA5010",
            L"#8E562E",
            L"#744DA9",
            L"#038387",
            L"#881798",
            L"#498205",
            L"#515C6B",
            L"#567C73",
            L"#7A7574",
        };

        #include "WorkspaceUtilityHelpers.cpp"

        #include "WorkspaceStartupColorLogic.cpp"

    }
    #include "WorkspaceFacadeMethods.cpp"
}
