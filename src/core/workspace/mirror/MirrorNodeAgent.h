// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "MirrorRouteDispatcher.h"

namespace terminal::workspace
{
    // Transport-independent PC-side half of the device tunnel. A WinHTTP
    // WebSocket loop feeds it decoded relay frames and sends its outbound
    // frames unchanged. Keeping this state here means terminal-server never
    // needs to inspect terminal payloads.
    class WorkspaceMirrorNodeAgent
    {
    public:
        explicit WorkspaceMirrorNodeAgent(std::shared_ptr<WorkspaceMirrorRouteDispatcher> dispatcher);

        bool HandleServerFrame(const WorkspaceMirrorRelayFrame& inbound,
                               std::wstring_view clientId,
                               uint64_t nowMilliseconds,
                               std::vector<WorkspaceMirrorRelayFrame>& outbound);
        bool PublishOutput(std::wstring_view commandId,
                           std::vector<uint8_t> bytes,
                           uint64_t timestampMilliseconds,
                           std::vector<WorkspaceMirrorRelayFrame>& outbound);
        size_t ActiveRouteCount() const noexcept;

    private:
        bool _hasRoute(const WorkspaceMirrorRelayFrame& frame) const noexcept;
        void _removeRoute(const WorkspaceMirrorRelayFrame& frame);

        std::shared_ptr<WorkspaceMirrorRouteDispatcher> _dispatcher;
        std::vector<WorkspaceMirrorRelayFrame> _routes;
    };
}
