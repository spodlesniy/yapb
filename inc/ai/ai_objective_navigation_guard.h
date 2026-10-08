//
// AiPB - objective approach navigation guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_bomb_defense_guard.h>

namespace ai {


// Bomb acquisition and actual use are distinct stages. GoldSrc's interaction
// range is shorter than the 80-unit coarse pickup radius previously used.
// Use a conservative 60-unit 3D limit; after a failed USE, close to 42 units.
constexpr float kPlantedBombDefuseReadyDistance = 60.0f;
constexpr float kPlantedBombDefuseRetryDistance = 42.0f;
constexpr float kPlantedBombDefuseStanceRetrySeconds = 1.25f;
constexpr float kPlantedBombDefuseReapproachSeconds = 3.0f;

constexpr float plantedBombDefuseApproachRadius(bool tighterRetry) {
  return tighterRetry ? kPlantedBombDefuseRetryDistance : kPlantedBombDefuseReadyDistance;
}

constexpr bool canBeginPlantedBombUse(float distanceSq, float maxUseDistance, bool directReachable) {
  return distanceSq >= 0.0f && maxUseDistance > 0.0f
      && distanceSq < maxUseDistance * maxUseDistance && directReachable;
}

// A task/IN_USE alone is NOT evidence of defusing. Only the game's BarTime
// message confirms it. A failed attempt must return to the C4 approach.
constexpr bool shouldChangeUnconfirmedDefuseStance(float elapsedSeconds, bool hasProgressBar,
                                                    bool alreadyChanged) {
  return elapsedSeconds >= kPlantedBombDefuseStanceRetrySeconds
      && !hasProgressBar && !alreadyChanged;
}

constexpr bool shouldReapproachUnconfirmedDefuse(float elapsedSeconds, bool hasProgressBar) {
  return elapsedSeconds >= kPlantedBombDefuseReapproachSeconds && !hasProgressBar;
}

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


// Honor the normal camp restriction except for an actively AI-owned planted
// C4 defense. Knife-mode still blocks all camping as before. A generic T
// Camp task, an expired objective, or unrelated action cannot bypass the CVAR.
constexpr bool mayContinuePlantedBombDefenseCamp(bool campingAllowed, bool knifeMode,
                                                  bool terrorist, bool demolitionMap,
                                                  bool bombPlanted, bool protectObjectiveActive) {
  return !knifeMode && (campingAllowed
      || (terrorist && demolitionMap && bombPlanted && protectObjectiveActive));
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

// A Terrorist already at a valid bomb-defense waypoint should not abandon
// protection just because the remaining timer is shorter than the travel
// reserve used for distant reinforcements. Never override a defuse alarm.
constexpr bool canHoldPlantedBombDefenseLocally(bool defuseAlarm, bool validDefenseNode,
                                                 float bombSecondsLeft, float botToBombDistanceSq,
                                                 float nodeToBombDistanceSq, float botToNodeDistanceSq,
                                                 float reachDistanceSq) {
  constexpr float radiusSq = kPlantedBombReinforcementRadius * kPlantedBombReinforcementRadius;
  return !defuseAlarm && validDefenseNode && bombSecondsLeft > 0.0f
      && botToBombDistanceSq >= 0.0f && botToBombDistanceSq <= radiusSq
      && nodeToBombDistanceSq >= 0.0f && nodeToBombDistanceSq <= radiusSq
      && botToNodeDistanceSq >= 0.0f && reachDistanceSq >= 0.0f
      && botToNodeDistanceSq <= reachDistanceSq;
}
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
                                                  float bestRoute, bool bestCamp, float bestCrowdCost, int bestNode,
                                                  int connections = -1, int bestConnections = -1,
                                                  int sectors = -1, int bestSectors = -1) {
  if (bestNode < 0) return true;
  const float topologyCost = connections >= 0
      ? bombDefenseConnectionPenalty(connections, true) * kBombDefenseConnectionRouteCost : 0.0f;
  const float bestTopologyCost = bestConnections >= 0
      ? bombDefenseConnectionPenalty(bestConnections, true) * kBombDefenseConnectionRouteCost : 0.0f;
  const float directionCost = sectors >= 0
      ? bombDefenseDirectionPenalty(sectors, true) * kBombDefenseDirectionRouteCost : 0.0f;
  const float bestDirectionCost = bestSectors >= 0
      ? bombDefenseDirectionPenalty(bestSectors, true) * kBombDefenseDirectionRouteCost : 0.0f;
  const float score = route - (camp ? kPlantedBombReinforcementCampRouteAllowance : 0.0f)
      + crowdCost + topologyCost + directionCost;
  const float bestScore = bestRoute - (bestCamp ? kPlantedBombReinforcementCampRouteAllowance : 0.0f)
      + bestCrowdCost + bestTopologyCost + bestDirectionCost;
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
