// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "../core/workspace/ExtCoreApi.h"

#include <filesystem>

namespace terminal::extcore
{
    // Host-side client for stable Ext C ABI only. It deliberately has no Glue
    // include or link dependency.
    class RuntimeClient final
    {
    public:
        static const RuntimeClient& Shared();

        std::filesystem::path WorkspaceDefinitionsPath() const;
        void RemovePersistedWorkspaceWindowState(uint64_t windowId) const;

        RuntimeClient(const RuntimeClient&) = delete;
        RuntimeClient& operator=(const RuntimeClient&) = delete;

    private:
        RuntimeClient();
        ~RuntimeClient();

        HMODULE _module{ nullptr };
        terminal::workspace::GetExtWorkspaceDefinitionsPathFn _getWorkspaceDefinitionsPath{ nullptr };
        terminal::workspace::RemoveExtPersistedWorkspaceWindowStateFn _removePersistedWorkspaceWindowState{ nullptr };
    };
}
