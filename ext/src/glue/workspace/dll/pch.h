// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../../../../microsoft/src/cascadia/TerminalApp/pch.h"

#ifdef GetClassName
#undef GetClassName
#endif

#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
