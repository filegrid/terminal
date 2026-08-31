// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorRecoveryPlanner.h"

#include "MirrorEventStore.h"

#include <algorithm>

    WorkspaceMirrorRecoveryPlan PlanWorkspaceMirrorRecovery(const WorkspaceMirrorNodeSession& session,
                                                             const std::wstring_view commandId,
                                                             const std::optional<uint64_t> resumeSequence)
    {
        const auto* window = FindWorkspaceMirrorWindow(session, commandId);
        if (!window || !window->Started)
        {
            return {};
        }

        WorkspaceMirrorRecoveryPlan plan;
        plan.HeadSequence = window->HeadSequence;
        if (resumeSequence && !window->HasGap)
        {
            const auto canResume = *resumeSequence <= window->HeadSequence &&
                                   (window->Events.empty() || *resumeSequence + 1 >= window->Events.front().Sequence);
            if (canResume)
            {
                plan.Kind = WorkspaceMirrorRecoveryKind::Resume;
                plan.BaseSequence = *resumeSequence;
                plan.Events = WorkspaceMirrorEventsAfter(*window, *resumeSequence);
                return plan;
            }
        }

        if (!window->Checkpoints.empty())
        {
            const auto& checkpoint = window->Checkpoints.back();
            plan.Kind = WorkspaceMirrorRecoveryKind::Checkpoint;
            plan.BaseSequence = checkpoint.Sequence;
            plan.Checkpoint = checkpoint.State;
            plan.Events = WorkspaceMirrorEventsAfter(*window, checkpoint.Sequence);
            return plan;
        }

        const auto startsAtBeginning = !window->Events.empty() && window->Events.front().Kind == WorkspaceMirrorEventKind::Started;
        if (!window->HasGap && startsAtBeginning)
        {
            plan.Kind = WorkspaceMirrorRecoveryKind::ReplayFromStart;
            plan.BaseSequence = 0;
            plan.Events = window->Events;
            return plan;
        }
        return plan;
    }
