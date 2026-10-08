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

constexpr bool shouldPreemptActiveDefuseForVisibleEnemy(bool visibleEnemy, bool hasDefuser,
                                                        float bombTimeRemaining) {
  const float fullDefuseTime = hasDefuser ? kDefuseTimeWithKit : kDefuseTimeWithoutKit;
  return visibleEnemy && bombTimeRemaining > fullDefuseTime + kDefuseSafetyMargin;
}

} // namespace ai
