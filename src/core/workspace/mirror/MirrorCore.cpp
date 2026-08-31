// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

// WorkspaceCore.cpp is the repository's intentional aggregation translation
// unit. Keep every Mirror implementation physically in this directory and
// include its split implementation fragments only from this aggregator.
#include "MirrorEventStore.cpp"
#include "MirrorCheckpoint.cpp"
#include "MirrorRecoveryPlanner.cpp"
#include "MirrorControlLease.cpp"
#include "MirrorNodeSession.cpp"
#include "MirrorNodeRecorder.cpp"
#include "MirrorAuthorization.cpp"
#include "MirrorReducer.cpp"
#include "MirrorRegistry.cpp"
#include "MirrorDirectoryProjection.cpp"
#include "MirrorConfiguration.cpp"
#include "MirrorRelayProtocol.cpp"
#include "MirrorTerminalProtocol.cpp"
#include "MirrorRouteDispatcher.cpp"
