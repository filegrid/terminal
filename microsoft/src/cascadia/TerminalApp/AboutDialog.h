// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AboutDialog.g.h"

namespace winrt::TerminalApp::implementation
{
    struct AboutDialog : AboutDialogT<AboutDialog>
    {
    public:
        AboutDialog();

        winrt::hstring ApplicationDisplayName();
        winrt::hstring ApplicationVersion();

    private:
        friend struct AboutDialogT<AboutDialog>; // for Xaml to bind events
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AboutDialog);
}
