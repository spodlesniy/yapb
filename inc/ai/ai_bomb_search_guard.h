//
// AiPB - planted-bomb search goal guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {



constexpr float kBombsiteSoundBoundsPadding = 128.0f;
// A target goal may lie slightly outside the actual bomb trigger volume.
constexpr float kBombsiteGoalAssociationPadding = 64.0f;
constexpr float kBombsiteSearchClusterRadius = 512.0f;
constexpr float kBombsiteSoundSafetyMargin = 64.0f;

// Keep the radius identical to isBombAudible(), including strict boundaries.
// No planted-bomb position is needed to compute it.
constexpr float bombAudibleRadiusAtPercent(float elapsedPercent) {
  return elapsedPercent > 85.0f ? 4096.0f
       : elapsedPercent > 68.0f ? 2048.0f
       : elapsedPercent > 52.0f ? 1280.0f
       : elapsedPercent > 28.0f ? 1024.0f : 768.0f;
}

constexpr float bombSearchAbs(float value) {
  return value < 0.0f ? -value : value;
}

constexpr float bombSearchMax(float a, float b) {
  return a > b ? a : b;
}

constexpr bool validBombTargetBounds(float minX, float minY, float maxX, float maxY) {
  return minX <= maxX && minY <= maxY
      && maxX - minX > 1.0f && maxY - minY > 1.0f;
}

constexpr bool isGoalInsideBombTargetVolume(float goalX, float goalY,
                                            float minX, float minY, float maxX, float maxY) {
  return validBombTargetBounds(minX, minY, maxX, maxY)
      && goalX >= minX - kBombsiteGoalAssociationPadding
      && goalX <= maxX + kBombsiteGoalAssociationPadding
      && goalY >= minY - kBombsiteGoalAssociationPadding
      && goalY <= maxY + kBombsiteGoalAssociationPadding;
}

// Include all trigger volumes in the waypoint's visited-goal cluster. This
// also makes the test conservative on maps with nearby bomb target brushes.
constexpr bool isBombTargetNearGoal(float goalX, float goalY,
                                    float minX, float minY, float maxX, float maxY) {
  if (!validBombTargetBounds(minX, minY, maxX, maxY)) {
    return false;
  }
  const float dx = goalX < minX ? minX - goalX : (goalX > maxX ? goalX - maxX : 0.0f);
  const float dy = goalY < minY ? minY - goalY : (goalY > maxY ? goalY - maxY : 0.0f);
  return dx * dx + dy * dy <= kBombsiteSearchClusterRadius * kBombsiteSearchClusterRadius;
}

// Silence may rule out a site only if even the FARTHEST possible C4 position
// in the full bomb-target brush would be audible. Bounds and sound margins
// deliberately account for player placement and localization uncertainty.
constexpr bool isEntireBombTargetAudible(float botX, float botY,
                                         float minX, float minY, float maxX, float maxY,
                                         float hearingRadius) {
  if (!validBombTargetBounds(minX, minY, maxX, maxY)
      || hearingRadius <= kBombsiteSoundBoundsPadding + kBombsiteSoundSafetyMargin) {
    return false;
  }
  const float dx = bombSearchMax(bombSearchAbs(botX - minX), bombSearchAbs(botX - maxX))
      + kBombsiteSoundBoundsPadding;
  const float dy = bombSearchMax(bombSearchAbs(botY - minY), bombSearchAbs(botY - maxY))
      + kBombsiteSoundBoundsPadding;
  const float safeRadius = hearingRadius - kBombsiteSoundSafetyMargin;
  return dx * dx + dy * dy <= safeRadius * safeRadius;
}

constexpr bool canRetargetCtToAudibleBomb(bool bombPlanted, bool objectivesEnabled,
                                           bool bombAudible, bool routeReachable) {
  return bombPlanted && objectivesEnabled && bombAudible && routeReachable;
}

constexpr bool shouldChangeAudibleBombGoal(int currentGoal, int audibleBombGoal) {
  return currentGoal != audibleBombGoal;
}

constexpr bool isBombSearchGoalEligible(bool visited, bool allowVisited) {
  return allowVisited || !visited;
}

constexpr bool isBetterBombSearchGoal(float candidateDistance, int candidateGoal,
                                      float bestDistance, int bestGoal) {
  return candidateDistance < bestDistance
      || (candidateDistance == bestDistance && (bestGoal < 0 || candidateGoal < bestGoal));
}

} // namespace ai
