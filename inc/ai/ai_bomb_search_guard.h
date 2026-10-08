//
// AiPB - planted-bomb search goal guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {


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
