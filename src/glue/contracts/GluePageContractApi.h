// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once // Shared loader/host contract export macro.

#if defined(WT_WORKSPACE_HOST_BUILD)
#define WT_WORKSPACE_EXT_API
#elif defined(WT_WORKSPACE_EXTENSION_EXPORTS)
#define WT_WORKSPACE_EXT_API __declspec(dllexport)
#else
#define WT_WORKSPACE_EXT_API __declspec(dllimport)
#endif
