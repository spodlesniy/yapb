//
// AiPB - planted C4 CT path choice guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>.
// SPDX-License-Identifier: MIT
//
#pragma once

namespace ai {

// This is only an ETA estimate, not an assumption that the bomb can be
// reached or defused. Defuse always takes priority over route diversity.

constexpr int kCtBombMaxRouteAllies = 4;
constexpr int kCtBombMaxRouteCongestion = 2;
constexpr float kCtBombAllyGoalNearDistanceSq = 320.0f * 320.0f;
constexpr float kCtBombRouteTrafficCostPerNode = 160.0f;

// D187: common, explicitly selected bombsite goals are observable teammate
// intentions even before a plant. Keep real defusing and combat unaltered.
constexpr bool shouldDiversifyCtSiteApproach(bool counterTerrorist, bool demolition,
                                             bool bombPlanted, bool siteGoal,
                                             bool supportedPathType, bool defusing,
                                             bool enemyVisible) {
  return counterTerrorist && demolition && (bombPlanted || siteGoal)
      && supportedPathType && !defusing && !enemyVisible;
}

// Capture ally movement intentions on the game thread before asynchronous
// pathfinding. The worker must never read another bot's mutable PathWalk.
struct CtBombAllyRouteIntent {
  int currentNode { -1 };
  int goalNode { -1 };
};

struct CtBombAllyRouteSnapshot {
  CtBombAllyRouteIntent allies[kCtBombMaxRouteAllies] {};
  int count {};
};

constexpr bool isRelevantCtBombRouteAlly(bool teammateAlive, bool localizedC4,
                                         float allyGoalDistanceSq, bool validNodes) {
  return teammateAlive && localizedC4 && validNodes
      && allyGoalDistanceSq <= kCtBombAllyGoalNearDistanceSq;
}

// Only intermediate path nodes count: sharing the C4 endpoint is unavoidable.
constexpr bool shouldReserveCtBombApproachNode(int node, int src, int dest) {
  return node >= 0 && node != src && node != dest;
}

constexpr float ctBombRouteTrafficPenalty(int sharedAllyCount) {
  return sharedAllyCount <= 0 ? 0.0f
       : (sharedAllyCount > kCtBombMaxRouteCongestion
           ? kCtBombMaxRouteCongestion : sharedAllyCount) * kCtBombRouteTrafficCostPerNode;
}

constexpr float kCtBombRouteUnreachable = 32767.0f;
constexpr float kCtBombRouteEffectiveSpeed = 0.75f;
constexpr float kCtBombRouteArrivalReserve = 4.0f;
constexpr float kCtBombRouteMaxDetourFactor = 1.35f;
constexpr float kCtBombRouteMaxDetourAllowance = 96.0f;

// Do not force long pre-plant detours to escape a single narrow waypoint link.
constexpr bool canAffordSharedCtSiteRoute(float shortestDistance, float candidateDistance) {
  return shortestDistance > 0.0f && shortestDistance < kCtBombRouteUnreachable
      && candidateDistance > 0.0f && candidateDistance < kCtBombRouteUnreachable
      && candidateDistance <= shortestDistance * kCtBombRouteMaxDetourFactor
                                + kCtBombRouteMaxDetourAllowance;
}

constexpr bool canAffordRiskAwareCtBombRoute(float shortestDistance, float candidateDistance,
                                              float maxSpeed, float bombTimeLeft, bool hasKit) {
  if (shortestDistance <= 0.0f || shortestDistance >= kCtBombRouteUnreachable
      || candidateDistance <= 0.0f || candidateDistance >= kCtBombRouteUnreachable
      || maxSpeed <= 0.0f || bombTimeLeft <= 0.0f) {
    return false;
  }
  const float maxDetour = shortestDistance * kCtBombRouteMaxDetourFactor
      + kCtBombRouteMaxDetourAllowance;
  const float estimatedSeconds = candidateDistance / (maxSpeed * kCtBombRouteEffectiveSpeed);
  const float defuseSeconds = hasKit ? 5.0f : 10.0f;
  return candidateDistance <= maxDetour
      && estimatedSeconds + defuseSeconds + kCtBombRouteArrivalReserve < bombTimeLeft;
}

// Some CTs keep the fastest approach while the others can take a safer
// alternative. A known C4 (heard or visibly acquired) is essential; an
// unlocalized site search still follows its shortest path.
constexpr bool shouldTryRiskAwareCtBombRoute(bool bombLocalized, int botId,
                                              float shortestDistance, float maxSpeed,
                                              float bombTimeLeft, bool hasKit) {
  return bombLocalized && botId > 0 && (botId % 2) == 0
      && canAffordRiskAwareCtBombRoute(
          shortestDistance, shortestDistance, maxSpeed, bombTimeLeft, hasKit);
}

// TODO(D193): replace this temporary Training ablation with a single persistent
// CT defuser owner managed at team scope, not independent bot pickup scans.
constexpr bool shouldIsolateTrainingCtDefuse(bool training, bool ct, bool demolition, bool planted) {
  return training && ct && demolition && planted;
}

// YaPB reserves extra task time beyond the 5/10 second engine defuse.
// Never start another Training CT pickup/defuse cycle when that allowance
// already exceeds the remaining bomb timer.
constexpr float plannedPlantedBombDefuseSeconds(bool hasKit) {
  return hasKit ? 7.0f : 12.0f;
}

constexpr bool shouldAbandonLateTrainingCtBombApproach(bool isolatedTrainingCt,
    bool activeProgress, float bombSecondsLeft, bool hasKit) {
  return isolatedTrainingCt && !activeProgress && bombSecondsLeft > 0.0f
      && bombSecondsLeft < plannedPlantedBombDefuseSeconds(hasKit);
}

// D192.14: recognize live teammates approaching planted C4 without
// confusing another bot's claim with confirmed BarTime.
constexpr bool isCtBombDefuseTeammateEngaged(bool alive, bool sameTeam, bool self,
    bool escaping, float distanceSq, bool activeDefuse, bool pickupClaim,
    bool movingTowardBomb) {
  constexpr float kActiveRangeSq = 165.0f * 165.0f;
  constexpr float kApproachRangeSq = 512.0f * 512.0f;
  return alive && sameTeam && !self && !escaping && distanceSq >= 0.0f
      && ((distanceSq < kActiveRangeSq && activeDefuse)
          || (distanceSq < kApproachRangeSq && (activeDefuse || pickupClaim || movingTowardBomb)));
}

// A physically ready contender can supersede an unfinished remote pickup
// reservation; this does not supersede confirmed USE/BarTime.
constexpr bool canCtBombReadyContenderChallenge(bool isolatedTrainingCt,
    bool physicallyUseReady, bool teammateBarTime) {
  return isolatedTrainingCt && physicallyUseReady && !teammateBarTime;
}

// A C4 pickup reservation is not a defuse in progress. A teammate already
// able to USE the bomb can supersede an owner still approaching its waypoint.
// Do not interrupt an owner at USE/BarTime or another physically ready owner.
constexpr bool shouldTransferTrainingCtBombPickup(bool isolatedTrainingCt,
    bool contenderUseReady, bool ownerDefusing, bool ownerUseReady) {
  return isolatedTrainingCt && contenderUseReady && !ownerDefusing && !ownerUseReady;
}

// Confirmed BarTime, not another bot's provisional PickupItem, makes a CT
// stop pursuing C4 so that ordinary combat/cover behavior can resume.
// A confirmed displaced defuser may keep its current C4 pickup across a
// brief combat-sensing scanner veto; no new pickup or USE is authorized.
constexpr float kCtDefuseRecoveryPickupGrace = 2.0f;
constexpr bool shouldPreserveDisplacedCtBombPickup(bool trainingCt, bool bombPlanted,
    bool hasPlantedC4Pickup, bool escapeActive, float now, float recoveryUntil) {
  return trainingCt && bombPlanted && hasPlantedC4Pickup && !escapeActive
      && recoveryUntil > now && now >= recoveryUntil - kCtDefuseRecoveryPickupGrace;
}

constexpr bool shouldYieldTrainingCtBombPickupToDefuser(bool isolatedTrainingCt,
    bool teammateBarTime, bool ownBarTime) {
  return isolatedTrainingCt && teammateBarTime && !ownBarTime;
}

// D184: elect a single CT with a credible route to localized C4.
// Actual BarTime/DefuseBomb ownership outranks every travel-time estimate.
constexpr float kCtBombDefuseIncumbentBias = 1.0f;
constexpr float kCtBombCoverSeparationSq = 192.0f * 192.0f;
constexpr float kCtBombCoverMinDistanceSq = 96.0f * 96.0f;

constexpr bool isCtBombDefuserCandidate(bool alive, bool localized, bool escaping,
                                       bool enemyVisible, bool activeDefuse, bool reachable) {
  return alive && localized && !escaping
      && (activeDefuse || (!enemyVisible && reachable));
}

constexpr float ctBombDefuserEstimatedCompletion(float routeDistance, float maxSpeed,
                                                  bool hasKit, bool ownsPickup, bool activeDefuse) {
  return activeDefuse ? -1000.0f
       : routeDistance / (maxSpeed * kCtBombRouteEffectiveSpeed)
           + (hasKit ? 5.0f : 10.0f)
           - (ownsPickup ? kCtBombDefuseIncumbentBias : 0.0f);
}

constexpr bool isBetterCtBombDefuser(float estimate, int botId, float bestEstimate, int bestBotId) {
  return bestBotId < 0 || estimate < bestEstimate
      || (estimate == bestEstimate && botId < bestBotId);
}

constexpr bool conflictsWithCtBombCoverGoal(bool allyCoverActive, float goalDistanceSq) {
  return allyCoverActive && goalDistanceSq >= 0.0f
      && goalDistanceSq < kCtBombCoverSeparationSq;
}

constexpr bool shouldHoldCtBombCover(bool bombPlanted, bool ownerAlive, bool bombDefusing) {
  return bombPlanted && (ownerAlive || bombDefusing);
}

} // namespace ai
