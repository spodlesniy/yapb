//
// AiPB - objective approach navigation guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {

// Keep graph navigation active until an objective can be interacted with directly.
// Reaching the objective's target node falls back to the final direct approach.
constexpr bool shouldUseGraphObjectiveApproach(float distanceSq, float interactionDistanceSq,
                                               bool targetNodeExists, bool targetNodeReached) {
  return distanceSq >= interactionDistanceSq && targetNodeExists && !targetNodeReached;
}

// While carrying C4, side pickups must not take ownership away from the active
// demolition objective unless objectives are explicitly disabled.
constexpr bool blocksBombCarrierSidePickups(bool hasC4, bool objectivesEnabled) {
  return hasC4 && objectivesEnabled;
}

// Objective pickup may temporarily enter the navigation pause inserted between
// graph jump segments without giving up the semantic pickup lifecycle.
template <typename TaskType>
constexpr bool ownsObjectivePickupTask(TaskType currentTask, TaskType pickupTask, TaskType pauseTask,
                                       bool objectivePickupActive) {
  return currentTask == pickupTask || (objectivePickupActive && currentTask == pauseTask);
}

} // namespace ai
