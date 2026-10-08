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


constexpr bool isReachablePlantedBombGraphApproach(bool directlyReachable,
                                                    bool graphDestinationValid,
                                                    float graphRouteDistance,
                                                    float unreachableDistance) {
  return directlyReachable
      || (graphDestinationValid && graphRouteDistance >= 0.0f
          && graphRouteDistance < unreachableDistance);
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


// Retain planted-bomb defense ownership during short legacy combat overrides.
 // The legacy task must still run; protection resumes when that task ends.
template <typename Task>
constexpr bool isTransientPlantedBombDefenseTask(Task currentTask, Task attackTask,
                                                 Task seekCoverTask, Task blindTask) {
  return currentTask == attackTask || currentTask == seekCoverTask || currentTask == blindTask;
}

// Prevent stale or interrupted movement from turning into camping far away
// from the actual planted-C4 defense waypoint.
constexpr bool hasReachedPlantedBombDefenseNode(int currentNode, int defenseNode,
                                                float distanceSq, float reachDistanceSq) {
  return defenseNode >= 0 && currentNode == defenseNode
      && distanceSq >= 0.0f && reachDistanceSq >= 0.0f
      && distanceSq <= reachDistanceSq;
}


constexpr float kPlantedBombReinforcementRadius = 768.0f;
constexpr float kPlantedBombReinforcementFarRoute = 1024.0f;
constexpr float kPlantedBombReinforcementUnreachableRoute = 32767.0f;
constexpr float kPlantedBombReinforcementSpeedFactor = 0.75f;
constexpr float kPlantedBombReinforcementArrivalReserve = 4.0f;
// Author-placed camp nodes can justify a small detour, but not a long delay.
constexpr float kPlantedBombReinforcementCampRouteAllowance = 192.0f;

// Evaluate actual graph distance rather than straight-line distance.
// An estimated arrival must leave time to hold the bomb before detonation.
constexpr bool canArriveAtPlantedBombDefenseInTime(float routeDistance, float maxSpeed,
                                                   float bombSecondsLeft) {
  return routeDistance >= 0.0f && routeDistance < kPlantedBombReinforcementUnreachableRoute
      && maxSpeed > 0.0f && maxSpeed < kPlantedBombReinforcementUnreachableRoute
      && bombSecondsLeft > kPlantedBombReinforcementArrivalReserve
      && routeDistance / (maxSpeed * kPlantedBombReinforcementSpeedFactor)
           + kPlantedBombReinforcementArrivalReserve < bombSecondsLeft;
}

// Rank eligible reinforcement positions by time to arrive. Prefer authored
// camp points only when the resulting detour is short.
constexpr bool isBetterPlantedBombReinforcementNode(float routeDistance, bool camp, int node,
                                                    float bestRouteDistance, bool bestCamp, int bestNode) {
  if (bestNode < 0) {
    return true;
  }
  const float adjustedDistance = routeDistance - (camp ? kPlantedBombReinforcementCampRouteAllowance : 0.0f);
  const float bestAdjustedDistance = bestRouteDistance
      - (bestCamp ? kPlantedBombReinforcementCampRouteAllowance : 0.0f);
  return adjustedDistance < bestAdjustedDistance
      || (adjustedDistance == bestAdjustedDistance && node < bestNode);
}

constexpr float kBombDefenseSeparationSq = 224.0f * 224.0f;
constexpr float kBombDefenseCrowdingCost = 512.0f;

constexpr float bombDefenseCrowdingCost(float distanceSq) {
  return distanceSq < kBombDefenseSeparationSq ? kBombDefenseCrowdingCost : 0.0f;
}

constexpr bool isBetterDistributedBombDefenseNode(float route, bool camp, float crowdCost, int node,
                                                  float bestRoute, bool bestCamp, float bestCrowdCost, int bestNode) {
  if (bestNode < 0) return true;
  const float score = route - (camp ? kPlantedBombReinforcementCampRouteAllowance : 0.0f) + crowdCost;
  const float bestScore = bestRoute - (bestCamp ? kPlantedBombReinforcementCampRouteAllowance : 0.0f) + bestCrowdCost;
  return score < bestScore || (score == bestScore && node < bestNode);
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
