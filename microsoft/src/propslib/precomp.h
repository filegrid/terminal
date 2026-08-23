// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#define DEFINE_CONSOLEV2_PROPERTIES
#define INC_OLE2

#define NOMINMAX

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>

#pragma warning(push)
#pragma warning(disable : 4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

#ifdef EXTERNAL_BUILD
#include <ShlObj.h>
#else
#include <shlobj_core.h>
#endif

#include <strsafe.h>
#include <sal.h>

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include <winconp.h>
#include "../host/settings.hpp"
#include <pathcch.h>

#include "conpropsp.hpp"
