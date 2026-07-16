// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "WorkspaceApi.h"

#include "../../../microsoft/src/types/inc/ColorFix.hpp"
#include "../../../microsoft/src/types/inc/utils.hpp"
#include "WorkspacePersistencePaths.h"

#include <shlobj.h>

using namespace Microsoft::Console;

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

        #include "WorkspacePersistenceSerialization.cpp"

    }

    std::filesystem::path WorkspaceManager::DefaultPath()
    {
        return terminal::workspacepaths::ResolveWorkspaceRootPath();
    }

    std::wstring SanitizeWorkspaceDirectoryName(const std::wstring_view value, const std::wstring_view fallback) noexcept
    {
        return terminal::workspacepaths::SanitizeWorkspaceDirectoryName(value, fallback);
    }

    std::filesystem::path WorkspaceStateManager::DefaultPath()
    {
        return terminal::workspacepaths::ResolveWorkspaceRootPath();
    }

    #include "WorkspacePersistenceManagers.cpp"

    #include "WorkspaceFacadeMethods.cpp"
}
