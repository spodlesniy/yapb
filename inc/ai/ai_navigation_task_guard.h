//
// AiPB - AI navigation task ownership guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {

// AI navigation may only own the bot while the legacy task stack is in a
// navigation-neutral state. The task type itself remains engine-owned; this
// generic helper keeps task and objective ownership testable without the GoldSrc runtime.
template <typename TaskType>
constexpr bool allowsNavigationOverride(TaskType currentTask, TaskType normalTask, TaskType moveToPositionTask,
                                        bool legacyObjectiveActive = false) {
  return !legacyObjectiveActive && (currentTask == normalTask || currentTask == moveToPositionTask);
}

// The legacy task selector runs BEFORE the AI executor each frame. Once AI
// Retreat has selected a cover waypoint, another legacy SeekCover selection
// would replace its MoveToPosition task and immediately restart the retreat.
// Suppress ONLY the duplicate cover desire; Attack and Blind remain eligible.
constexpr bool shouldDeferLegacySeekCoverToAiRetreat(bool aiControlEnabled,
                                                     bool aiActionActive, bool retreatOwned) {
  return aiControlEnabled && aiActionActive && retreatOwned;
}

// External legacy tasks can momentarily own execution without invalidating
// the retreat's destination. Let combat and blindness run uninterrupted;
// a leftover SeekCover may also finish instead of causing a cancel/restart loop.
template <typename TaskType>
constexpr bool isTemporaryRetreatTaskOverride(TaskType task, TaskType attackTask,
                                               TaskType blindTask, TaskType coverTask) {
  return task == attackTask || task == blindTask || task == coverTask;
}

// Physical clearance can require crouching before reaching a crouch-flagged
// waypoint. A standing-height forward obstruction alone is insufficient:
// the duck-height probe must be free as well.
constexpr bool shouldDuckForLowCeiling(bool moving, bool grounded, bool onLadder,
                                       bool standingHeadBlocked, bool duckPathClear) {
  return moving && grounded && !onLadder && standingHeadBlocked && duckPathClear;
}

} // namespace ai
