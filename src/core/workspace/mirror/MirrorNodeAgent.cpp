// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorNodeAgent.h"

#include <algorithm>

// MirrorCore.cpp includes this implementation inside terminal::workspace.
    WorkspaceMirrorNodeAgent::WorkspaceMirrorNodeAgent(std::shared_ptr<WorkspaceMirrorRouteDispatcher> dispatcher) :
        _dispatcher{ std::move(dispatcher) }
    {
    }

    bool WorkspaceMirrorNodeAgent::_hasRoute(const WorkspaceMirrorRelayFrame& frame) const noexcept
    {
        return std::any_of(_routes.cbegin(), _routes.cend(), [&frame](const auto& route) {
            return route.RouteId == frame.RouteId;
        });
    }

    void WorkspaceMirrorNodeAgent::_removeRoute(const WorkspaceMirrorRelayFrame& frame)
    {
        std::erase_if(_routes, [&frame](const auto& route) {
            return route.RouteId == frame.RouteId;
        });
    }

    bool WorkspaceMirrorNodeAgent::HandleServerFrame(const WorkspaceMirrorRelayFrame& inbound,
                                                      const std::wstring_view clientId,
                                                      const uint64_t nowMilliseconds,
                                                      std::vector<WorkspaceMirrorRelayFrame>& outbound)
    {
        if (!_dispatcher)
        {
            return false;
        }
        if (inbound.Kind == WorkspaceMirrorRelayFrameKind::RouteOpen)
        {
            if (_hasRoute(inbound) || !_dispatcher->HandleInbound(inbound, clientId, nowMilliseconds, outbound))
            {
                return false;
            }
            _routes.emplace_back(inbound);
            return true;
        }
        if (inbound.Kind == WorkspaceMirrorRelayFrameKind::RouteClose)
        {
            const auto handled = _dispatcher->HandleInbound(inbound, clientId, nowMilliseconds, outbound);
            _removeRoute(inbound);
            return handled;
        }
        // A Pong is the acknowledgement for the device-originated heartbeat.
        // It is not associated with a route and has no Core-side effect.
        if (inbound.Kind == WorkspaceMirrorRelayFrameKind::Pong)
        {
            return true;
        }
        if (inbound.Kind == WorkspaceMirrorRelayFrameKind::TerminalIntent && !_hasRoute(inbound))
        {
            return false;
        }
        return _dispatcher->HandleInbound(inbound, clientId, nowMilliseconds, outbound);
    }

    bool WorkspaceMirrorNodeAgent::PublishOutput(const std::wstring_view commandId,
                                                  std::vector<uint8_t> bytes,
                                                  const uint64_t timestampMilliseconds,
                                                  std::vector<WorkspaceMirrorRelayFrame>& outbound)
    {
        return _dispatcher && _dispatcher->RecordOutputAndBuildEffects(commandId, std::move(bytes), timestampMilliseconds, _routes, outbound);
    }

    size_t WorkspaceMirrorNodeAgent::ActiveRouteCount() const noexcept
    {
        return _routes.size();
    }
