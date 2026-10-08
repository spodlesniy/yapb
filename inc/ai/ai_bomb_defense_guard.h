//
// AiPB - planted-bomb defense node guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {

constexpr bool isBombDefenseNodeEligibleForPass(bool requireCamp, bool isCamp, bool isLadder) {
  return (!requireCamp || isCamp) && !isLadder;
}

// Count ordinary outgoing connections. The caller validates graph nodes and
// rejects jump and ladder links. YaPB graph creation already rejects duplicates.
template <typename Links, typename IsWalkable>
int countWalkableBombDefenseConnections(const Links &links, IsWalkable isWalkable) {
  int count = 0;
  for (const auto &link : links) {
    if (isWalkable(link)) ++count;
  }
  return count;
}

// Quantize outgoing WALKABLE links into eight horizontal approach directions.
// Multiple links down the same corridor count as one direction; vertical-only
// links carry no useful horizontal direction. This is only a topology heuristic.
constexpr float kBombDefenseOctantEdge = 2.41421356f; // tan(67.5 degrees)
constexpr int bombDefenseExitSector(float dx, float dy) {
  if (dx == 0.0f && dy == 0.0f) return -1;
  const float ax = dx < 0.0f ? -dx : dx;
  const float ay = dy < 0.0f ? -dy : dy;
  if (ax >= ay * kBombDefenseOctantEdge) return dx > 0.0f ? 0 : 4;
  if (ay >= ax * kBombDefenseOctantEdge) return dy > 0.0f ? 2 : 6;
  return dx > 0.0f ? (dy > 0.0f ? 1 : 7) : (dy > 0.0f ? 3 : 5);
}

template <typename Links, typename IsWalkable, typename DeltaX, typename DeltaY>
int countBombDefenseExitSectors(const Links &links, IsWalkable isWalkable,
                                DeltaX deltaX, DeltaY deltaY) {
  unsigned int mask = 0;
  for (const auto &link : links) {
    if (!isWalkable(link)) continue;
    const int sector = bombDefenseExitSector(deltaX(link), deltaY(link));
    if (sector >= 0) mask |= 1u << sector;
  }
  int count = 0;
  for (int i = 0; i < 8; ++i) {
    if (mask & (1u << i)) ++count;
  }
  return count;
}

// A T needs at least two useful directions to reposition; a CT watching a
// dropped bomb can exploit a one-direction recess. Busy crossings are costly.
constexpr int bombDefenseDirectionPenalty(int sectors, bool plantedDefense) {
  if (sectors <= 0) return 3;
  if (plantedDefense && sectors == 1) return 1;
  return sectors <= 2 ? 0 : sectors == 3 ? 1 : 2;
}
constexpr float kBombDefenseDirectionRouteCost = 80.0f;

// This is a weak topology preference, not a replacement for world visibility.
// CT can defend from a recess; T preferably retain two ways to reposition.
constexpr int bombDefenseConnectionPenalty(int count, bool plantedDefense) {
  if (count <= 0) return 3;
  if (plantedDefense) return count == 2 ? 0 : count == 3 ? 1 : 2;
  return count <= 2 ? 0 : count == 3 ? 1 : 2;
}
constexpr int kBombDefenseExposureNearTie = 4;
constexpr int kBombDefenseDamageNearTie = 8;
constexpr float kBombDefenseConnectionRouteCost = 64.0f;

// Dropped-C4 defense should prefer positions that expose the defender to fewer
// graph locations. Historical damage and route cost are deterministic tie-breaks.
constexpr bool isBetterBombDefenseCover(int candidateExposure, int candidateDamage,
                                        float candidateRouteDistance, int candidateNode,
                                        int bestExposure, int bestDamage,
                                        float bestRouteDistance, int bestNode,
                                        int candidateConnections = -1, int bestConnections = -1,
                                        bool plantedDefense = false,
                                        int candidateSectors = -1, int bestSectors = -1) {
  if (bestNode < 0) {
    return true;
  }
  // Only near-equivalent exposure/damage may be traded for better topology.
  // Calls without link information preserve the exact historical ordering.
  if (candidateConnections >= 0 && bestConnections >= 0) {
    if (candidateExposure + kBombDefenseExposureNearTie < bestExposure) return true;
    if (bestExposure + kBombDefenseExposureNearTie < candidateExposure) return false;
    if (candidateDamage + kBombDefenseDamageNearTie < bestDamage) return true;
    if (bestDamage + kBombDefenseDamageNearTie < candidateDamage) return false;
    const int candidatePenalty = bombDefenseConnectionPenalty(candidateConnections, plantedDefense);
    const int bestPenalty = bombDefenseConnectionPenalty(bestConnections, plantedDefense);
    const int candidateDirectionPenalty = candidateSectors >= 0
        ? bombDefenseDirectionPenalty(candidateSectors, plantedDefense) : 0;
    const int bestDirectionPenalty = bestSectors >= 0
        ? bombDefenseDirectionPenalty(bestSectors, plantedDefense) : 0;
    const int candidateTotal = candidatePenalty + candidateDirectionPenalty;
    const int bestTotal = bestPenalty + bestDirectionPenalty;
    if (candidateTotal != bestTotal) return candidateTotal < bestTotal;
  }
  if (candidateExposure != bestExposure) {
    return candidateExposure < bestExposure;
  }
  if (candidateDamage != bestDamage) {
    return candidateDamage < bestDamage;
  }
  if (candidateRouteDistance != bestRouteDistance) {
    return candidateRouteDistance < bestRouteDistance;
  }
  return candidateNode < bestNode;
}

constexpr float kDefuseSafetyMargin = 2.0f;
constexpr float kDefuseTimeWithKit = 7.0f;
constexpr float kDefuseTimeWithoutKit = 12.0f;

constexpr float kDefuseKitPickupAllowance = 1.0f;
constexpr float kDefuseInteractionAllowance = 1.0f;
// Near-equal completion margins are compared by route risk. Avoid changing an
// already owned route until its competitor wins by more than this time margin.
constexpr float kDefuseRouteTieWindow = 1.0f;
constexpr float kDefuseRouteSwitchHysteresis = 1.0f;

// Floyd-Warshall stores an unreachable distance as SHRT_MAX. Rejecting this
// bound also conservatively rejects overlong Dijkstra routes.
constexpr float kDefuseUnreachableDistance = 32767.0f;

struct DefuseRouteLeg {
  bool reachable;
  float distance;
};

enum class DefuseRouteChoice {
  None,
  Direct,
  ViaKit
};

struct DefuseRouteEstimate {
  bool feasible;
  float slackSeconds;
};

struct DefuseRouteEvaluation {
  DefuseRouteChoice choice;
  DefuseRouteEstimate direct;
  DefuseRouteEstimate viaKit;
};

constexpr bool isUsableDefuseRouteLeg(DefuseRouteLeg leg) {
  return leg.reachable && leg.distance >= 0.0f && leg.distance < kDefuseUnreachableDistance;
}

// Distances must be measured on the shortest waypoint graph route, not as
// Euclidean distances. The caller must validate route connectivity and supply
// a conservative estimated travel speed (not necessarily pev->maxspeed).
constexpr DefuseRouteEstimate estimateDefuseRoute(float bombSecondsLeft, float routeDistance,
                                                 float estimatedSpeed, float defuseSeconds,
                                                 float extraSeconds) {
  if (!(bombSecondsLeft > 0.0f && bombSecondsLeft < 1000000.0f)
      || !(estimatedSpeed > 0.0f && estimatedSpeed < kDefuseUnreachableDistance)
      || !(routeDistance >= 0.0f && routeDistance < 2.0f * kDefuseUnreachableDistance)
      || !(extraSeconds >= 0.0f)) {
    return { false, -1.0f };
  }
  const float needed = routeDistance / estimatedSpeed + defuseSeconds + extraSeconds + kDefuseSafetyMargin;
  const float slack = bombSecondsLeft - needed;
  return { slack > 0.0f, slack };
}

// Compare the direct CT -> C4 route with CT -> kit -> C4. Risk inputs are
// nonnegative route costs when known; a negative value means risk is unknown.
// Route preference is only a recommendation: the pickup scanner will integrate
// it separately, including cancellation of stale kit targets.
constexpr DefuseRouteEvaluation chooseDefuseRoute(bool alreadyHasKit, float bombSecondsLeft,
                                                   float estimatedSpeed, DefuseRouteLeg toBomb,
                                                   DefuseRouteLeg toKit, DefuseRouteLeg kitToBomb,
                                                   int directRisk, int viaKitRisk,
                                                   DefuseRouteChoice currentChoice = DefuseRouteChoice::None) {
  const auto direct = isUsableDefuseRouteLeg(toBomb)
    ? estimateDefuseRoute(bombSecondsLeft, toBomb.distance, estimatedSpeed,
                          alreadyHasKit ? kDefuseTimeWithKit : kDefuseTimeWithoutKit,
                          kDefuseInteractionAllowance)
    : DefuseRouteEstimate { false, -1.0f };
  const auto viaKit = !alreadyHasKit && isUsableDefuseRouteLeg(toKit) && isUsableDefuseRouteLeg(kitToBomb)
    ? estimateDefuseRoute(bombSecondsLeft, toKit.distance + kitToBomb.distance, estimatedSpeed,
                          kDefuseTimeWithKit, kDefuseKitPickupAllowance + kDefuseInteractionAllowance)
    : DefuseRouteEstimate { false, -1.0f };

  if (!direct.feasible && !viaKit.feasible) {
    return { DefuseRouteChoice::None, direct, viaKit };
  }
  if (!direct.feasible) {
    return { DefuseRouteChoice::ViaKit, direct, viaKit };
  }
  if (!viaKit.feasible) {
    return { DefuseRouteChoice::Direct, direct, viaKit };
  }

  DefuseRouteChoice preferred = DefuseRouteChoice::Direct;
  if (viaKit.slackSeconds > direct.slackSeconds + kDefuseRouteTieWindow) {
    preferred = DefuseRouteChoice::ViaKit;
  }
  else if (direct.slackSeconds <= viaKit.slackSeconds + kDefuseRouteTieWindow) {
    if (directRisk >= 0 && viaKitRisk >= 0 && directRisk != viaKitRisk) {
      preferred = viaKitRisk < directRisk ? DefuseRouteChoice::ViaKit : DefuseRouteChoice::Direct;
    }
    else if (viaKit.slackSeconds > direct.slackSeconds) {
      preferred = DefuseRouteChoice::ViaKit;
    }
  }

  // A route that has become infeasible must be abandoned immediately. For two
  // feasible routes, a small time/risk fluctuation must not create route churn.
  if (currentChoice == DefuseRouteChoice::Direct && preferred == DefuseRouteChoice::ViaKit
      && viaKit.slackSeconds <= direct.slackSeconds + kDefuseRouteSwitchHysteresis) {
    preferred = DefuseRouteChoice::Direct;
  }
  else if (currentChoice == DefuseRouteChoice::ViaKit && preferred == DefuseRouteChoice::Direct
           && direct.slackSeconds <= viaKit.slackSeconds + kDefuseRouteSwitchHysteresis) {
    preferred = DefuseRouteChoice::ViaKit;
  }
  return { preferred, direct, viaKit };
}



// Keep an already selected defuse kit if another usable kit improves estimated
// slack only marginally. Never retain a candidate with an invalid budget.
constexpr bool isBetterDefuseKitCandidate(float candidateSlack, bool candidateIsCurrent,
                                          float bestSlack, bool bestIsCurrent) {
  if (!(candidateSlack > 0.0f)) {
    return false;
  }
  if (!(bestSlack > 0.0f)) {
    return true;
  }
  if (candidateIsCurrent != bestIsCurrent
      && candidateSlack <= bestSlack + kDefuseRouteSwitchHysteresis
      && bestSlack <= candidateSlack + kDefuseRouteSwitchHysteresis) {
    return candidateIsCurrent;
  }
  return candidateSlack > bestSlack + (bestIsCurrent ? kDefuseRouteSwitchHysteresis : 0.0f);
}

constexpr bool shouldPreemptActiveDefuseForVisibleEnemy(bool visibleEnemy, bool hasDefuser,
                                                        float bombTimeRemaining) {
  const float fullDefuseTime = hasDefuser ? kDefuseTimeWithKit : kDefuseTimeWithoutKit;
  return visibleEnemy && bombTimeRemaining > fullDefuseTime + kDefuseSafetyMargin;
}

} // namespace ai
