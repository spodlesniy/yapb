//
// AiPB - C4 carrier goal candidate selection.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <cstddef>

namespace ai {

constexpr float kBombCarrierSiteClusterRadius = 512.0f;
constexpr float kBombCarrierUnreachableDistance = 32767.0f;
constexpr float kBombCarrierDetourFactor = 1.9f;
constexpr float kBombCarrierDetourAllowance = 256.0f;
constexpr float kBombCarrierAllySupportRadius = 640.0f;

struct BombCarrierGoalOption {
  int node { -1 };
  float score {};
  float routeDistance {};
  float x {}, y {}, z {};
};

constexpr bool isBombCarrierGoalReachable(float distance) {
  return distance >= 0.0f && distance < kBombCarrierUnreachableDistance;
}

constexpr bool canConsiderBombCarrierGoal(float routeDistance, float shortestDistance) {
  return isBombCarrierGoalReachable(routeDistance)
      && isBombCarrierGoalReachable(shortestDistance)
      && routeDistance <= shortestDistance * kBombCarrierDetourFactor
                            + kBombCarrierDetourAllowance;
}

constexpr bool isSameBombCarrierSite(const BombCarrierGoalOption &a,
                                    const BombCarrierGoalOption &b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz
      <= kBombCarrierSiteClusterRadius * kBombCarrierSiteClusterRadius;
}

// Distance is still important, but learned danger, previous success, allies
// heading toward a site, and bounded randomized variation can alter the order.
constexpr float bombCarrierGoalScore(float routeDistance, int danger,
                                      int priorSuccess, int supportingAllies,
                                      bool preserveSite, float variation,
                                      bool rusher) {
  return routeDistance
      + static_cast<float>(danger) * (rusher ? 0.15f : 0.35f)
      - static_cast<float>(priorSuccess) * 0.20f
      - static_cast<float>(supportingAllies) * 160.0f
      - (preserveSite ? 180.0f : 0.0f)
      + variation;
}

constexpr bool betterBombCarrierGoal(const BombCarrierGoalOption &a,
                                      const BombCarrierGoalOption &b) {
  return a.score < b.score || (a.score == b.score && a.node < b.node);
}

// Preserve at most one waypoint per nearby bombsite, retaining the best
// candidate for each site rather than inserting the same node four times.
template <size_t N>
void retainBombCarrierGoal(BombCarrierGoalOption (&options)[N],
                           const BombCarrierGoalOption &candidate) {
  for (size_t i = 0; i < N; ++i) {
    if (options[i].node >= 0 && isSameBombCarrierSite(options[i], candidate)) {
      if (betterBombCarrierGoal(candidate, options[i])) options[i] = candidate;
      return;
    }
  }
  for (size_t i = 0; i < N; ++i) {
    if (options[i].node < 0) {
      options[i] = candidate;
      return;
    }
  }
  size_t worst = 0;
  for (size_t i = 1; i < N; ++i) {
    if (betterBombCarrierGoal(options[worst], options[i])) worst = i;
  }
  if (betterBombCarrierGoal(candidate, options[worst])) options[worst] = candidate;
}

template <size_t N>
int chooseBombCarrierGoal(const BombCarrierGoalOption (&options)[N]) {
  int best = -1;
  for (size_t i = 0; i < N; ++i) {
    if (options[i].node >= 0
        && (best < 0 || betterBombCarrierGoal(options[i], options[best]))) {
      best = static_cast<int>(i);
    }
  }
  return best < 0 ? -1 : options[best].node;
}

} // namespace ai
