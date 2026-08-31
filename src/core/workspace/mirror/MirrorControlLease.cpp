// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorControlLease.h"

    bool GrantWorkspaceMirrorControl(WorkspaceMirrorNodeSession& session,
                                     std::wstring clientId,
                                     std::wstring leaseId,
                                     const uint64_t nowMilliseconds,
                                     const uint64_t leaseDurationMilliseconds)
    {
        if (session.Closed || clientId.empty() || leaseId.empty() || leaseDurationMilliseconds == 0)
        {
            return false;
        }
        if (session.ControlLease && session.ControlLease->ExpiresAtMilliseconds > nowMilliseconds && session.ControlLease->ClientId != clientId)
        {
            return false;
        }
        session.ControlLease = { std::move(clientId), std::move(leaseId), nowMilliseconds + leaseDurationMilliseconds };
        return true;
    }

    bool RevokeWorkspaceMirrorControl(WorkspaceMirrorNodeSession& session, const std::wstring_view clientId)
    {
        if (!session.ControlLease || (!clientId.empty() && session.ControlLease->ClientId != clientId))
        {
            return false;
        }
        session.ControlLease.reset();
        return true;
    }

    bool HasWorkspaceMirrorControl(const WorkspaceMirrorNodeSession& session,
                                   const std::wstring_view clientId,
                                   const std::wstring_view leaseId,
                                   const uint64_t nowMilliseconds)
    {
        return !session.Closed && session.ControlLease && session.ControlLease->ExpiresAtMilliseconds > nowMilliseconds &&
               session.ControlLease->ClientId == clientId && session.ControlLease->LeaseId == leaseId;
    }
