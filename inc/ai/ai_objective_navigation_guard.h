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

// Once an interaction-safe objective node is reached, finish only the remaining
// distance to the objective interaction sphere. Do not drive back to the center
// of a waypoint whose navigation radius has already been satisfied.
constexpr bool shouldFinishObjectiveApproachDirectly(float distanceSq, float interactionDistanceSq,
                                                     bool interactionNodeExists, bool interactionNodeReached) {
  return distanceSq >= interactionDistanceSq && interactionNodeExists && interactionNodeReached;
}

// Objective interaction range is a full 3D sphere. A node that is close in XY
// but too far above or below the objective must not be treated as interaction-safe.
constexpr bool isWithinObjectiveInteractionRange(float deltaX, float deltaY, float deltaZ,
                                                 float interactionDistance) {
  return deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ
      < interactionDistance * interactionDistance;
}

constexpr bool isBetterObjectiveApproachNode(float candidateRouteDistance, int candidateNode,
                                             float bestRouteDistance, int bestNode) {
  return candidateRouteDistance < bestRouteDistance
      || (candidateRouteDistance == bestRouteDistance && (bestNode < 0 || candidateNode < bestNode));
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
