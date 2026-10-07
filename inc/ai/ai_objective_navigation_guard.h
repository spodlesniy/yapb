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

// If the short final line to the objective is physically blocked, keep using
// the already selected interaction-safe node as a geometry-aware staging point.
// Its center lies inside the interaction sphere, so reaching it is sufficient.
constexpr bool shouldFinishObjectiveApproachViaInteractionNode(float distanceSq, float interactionDistanceSq,
                                                              bool interactionNodeExists, bool interactionNodeReached,
                                                              bool directApproachReachable) {
  return distanceSq >= interactionDistanceSq
      && interactionNodeExists
      && interactionNodeReached
      && !directApproachReachable;
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

// Keep a firearm ready while a Counter-Terrorist is traversing toward a
// planted bomb with living enemies still in the round. The usual jump-speed
// knife optimization is safe again after the threat is gone or objectives are
// explicitly disabled.
constexpr bool preservesFirearmForPlantedBombApproach(bool counterTerrorist, bool demolitionMap,
                                                      bool bombPlanted, bool enemiesAlive,
                                                      bool objectivesEnabled) {
  return counterTerrorist && demolitionMap && bombPlanted && enemiesAlive && objectivesEnabled;
}

// A dropped-C4 defender must be able to act on the objective without abandoning
// immediate combat or another emergency traversal state.
constexpr bool isDroppedBombDefenderEligible(bool alive, bool counterTerrorist, bool seesBomb,
                                             bool seeingEnemy, bool onLadder, bool escaping) {
  return alive && counterTerrorist && seesBomb && !seeingEnemy && !onLadder && !escaping;
}

constexpr bool isBetterDroppedBombDefender(float candidateDistanceSq, int candidateIndex,
                                           float bestDistanceSq, int bestIndex) {
  return candidateDistanceSq < bestDistanceSq
      || (candidateDistanceSq == bestDistanceSq && (bestIndex < 0 || candidateIndex < bestIndex));
}

// Once planted-C4 pickup has handed control to DefuseBomb, pickup discovery
// must leave the preserved C4 entity binding alone until defuse ownership ends.
template <typename TaskType>
constexpr bool preservesPlantedBombPickupDuringDefuse(TaskType currentTask, TaskType defuseTask) {
  return currentTask == defuseTask;
}

// Objective pickup may temporarily enter the navigation pause inserted between
// graph jump segments without giving up the semantic pickup lifecycle.
template <typename TaskType>
constexpr bool ownsObjectivePickupTask(TaskType currentTask, TaskType pickupTask, TaskType pauseTask,
                                       bool objectivePickupActive) {
  return currentTask == pickupTask || (objectivePickupActive && currentTask == pauseTask);
}

} // namespace ai
