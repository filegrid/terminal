// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "WorkspaceManagerProfilePicker.h"

#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml::Controls;

namespace terminal::workspace
{
    ComboBox CreateWorkspaceManagerProfilePicker(const std::vector<WorkspaceProfileOption>& profiles,
                                                 std::wstring selectedProfileGuid,
                                                 const bool enabled)
    {
        auto picker = ComboBox{};
        picker.IsEnabled(enabled);

        int32_t selectedIndex = -1;
        for (uint32_t index = 0; index < profiles.size(); ++index)
        {
            const auto& profile = profiles.at(index);
            auto item = ComboBoxItem{};
            item.Content(box_value(profile.DisplayName.empty() ? profile.Guid : profile.DisplayName));
            item.Tag(box_value(profile.Guid));
            picker.Items().Append(item);
            if (!selectedProfileGuid.empty() && _wcsicmp(profile.Guid.c_str(), selectedProfileGuid.c_str()) == 0)
            {
                selectedIndex = gsl::narrow_cast<int32_t>(index);
            }
        }

        picker.SelectedIndex(selectedIndex);
        return picker;
    }
}
