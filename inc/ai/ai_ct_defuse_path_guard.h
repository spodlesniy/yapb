//
// AiPB - planted C4 CT path choice guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>.
// SPDX-License-Identifier: MIT
//
#pragma once

namespace ai {

// This is only an ETA estimate, not an assumption that the bomb can be
// reached or defused. Defuse always takes priority over route diversity.
constexpr float kCtBombRouteUnreachable = 32767.0f;
constexpr float kCtBombRouteEffectiveSpeed = 0.75f;
constexpr float kCtBombRouteArrivalReserve = 4.0f;
constexpr float kCtBombRouteMaxDetourFactor = 1.35f;
constexpr float kCtBombRouteMaxDetourAllowance = 96.0f;

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

} // namespace ai
