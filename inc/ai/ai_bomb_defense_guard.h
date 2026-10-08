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

// Dropped-C4 defense should prefer positions that expose the defender to fewer
// graph locations. Historical damage and route cost are deterministic tie-breaks.
constexpr bool isBetterBombDefenseCover(int candidateExposure, int candidateDamage,
                                        float candidateRouteDistance, int candidateNode,
                                        int bestExposure, int bestDamage,
                                        float bestRouteDistance, int bestNode) {
  if (bestNode < 0) {
    return true;
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


constexpr bool shouldPreemptActiveDefuseForVisibleEnemy(bool visibleEnemy, bool hasDefuser,
                                                        float bombTimeRemaining) {
  const float fullDefuseTime = hasDefuser ? kDefuseTimeWithKit : kDefuseTimeWithoutKit;
  return visibleEnemy && bombTimeRemaining > fullDefuseTime + kDefuseSafetyMargin;
}

} // namespace ai
