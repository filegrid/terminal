// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"

#include "ActionMap.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    // Ext deliberately has no resource/XAML dependency. Workspace core only
    // needs ActionAndArgs to be link-complete when it builds startup actions;
    // localized action labels are a Glue/UI concern.
    hstring ActionArgFactory::GetNameForAction(const ShortcutAction& action)
    {
        return winrt::to_hstring(static_cast<int32_t>(action));
    }

    hstring ActionArgFactory::GetNameForAction(const ShortcutAction& action,
                                               const Windows::ApplicationModel::Resources::Core::ResourceContext&)
    {
        return GetNameForAction(action);
    }
}
