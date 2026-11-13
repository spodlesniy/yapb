//
// AiPB - bounded local-goal movement selection for operator tests.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

#include <ai/ai_model_io_directions.h>

namespace ai::runtime_io {

constexpr float kModelIOGoalRadius = 16.0f;
constexpr float kModelIOGoalVerticalTolerance = 8.0f;
constexpr float kModelIOGoalMaxInitialDistance = 768.0f;

// These geometric checks do not authorize an action. Every candidate still
// requires an independent GoldSrc TraceHull before it may be executed.
inline float modelIOGoalDistance(const Vec3 &a, const Vec3 &b) {
  if (!finiteVector(a) || !finiteVector(b)) {
    return std::numeric_limits<float>::infinity();
  }
  const double dx = static_cast<double>(a.x) - b.x;
  const double dy = static_cast<double>(a.y) - b.y;
  const double dz = static_cast<double>(a.z) - b.z;
  return static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
}

inline bool modelIOGoalReached(const Vec3 &origin, const Vec3 &goal) {
  return modelIOGoalDistance(origin, goal) <= kModelIOGoalRadius;
}

inline bool modelIOGoalCanArm(const Vec3 &origin, const Vec3 &goal) {
  if (!finiteVector(origin) || !finiteVector(goal) ||
      std::abs(origin.z - goal.z) > kModelIOGoalVerticalTolerance ||
      modelIOGoalReached(origin, goal)) {
    return false;
  }
  const double dx = static_cast<double>(origin.x) - goal.x;
  const double dy = static_cast<double>(origin.y) - goal.y;
  return std::hypot(dx, dy) <= kModelIOGoalMaxInitialDistance;
}

// The forward direction is aligned with the operator goal, unlike the
// view-yaw-relative external memory trial. The last step may be shorter.
inline Vec3 modelIOGoalDirectionTarget(const Vec3 &origin, const Vec3 &goal,
                                       uint8_t direction) {
  if (!finiteVector(origin) || !finiteVector(goal) ||
      direction >= kModelIODirectionCount) {
    return {};
  }
  const float dx = goal.x - origin.x;
  const float dy = goal.y - origin.y;
  const float distance = std::hypot(dx, dy);
  if (!std::isfinite(distance) || distance <= 0.0f) {
    return {};
  }
  const float step = distance < kModelIODirectionDistance
      ? distance : kModelIODirectionDistance;
  const float offset = direction == 1 ? 1.5707963267948966f :
      direction == 2 ? -1.5707963267948966f : 0.0f;
  const float angle = std::atan2(dy, dx) + offset;
  return { origin.x + std::cos(angle) * step,
           origin.y + std::sin(angle) * step, origin.z };
}

// Only previously traced and admitted targets are eligible. A blocked forward
// route may choose a side as a bounded detour without claiming pathfinding.
inline int modelIOGoalChooseDirection(const ModelIODirectionCandidates &choices,
                                      const Vec3 &goal) {
  if (!finiteVector(goal)) {
    return -1;
  }
  int best = -1;
  float bestDistance = std::numeric_limits<float>::infinity();
  for (uint8_t direction = 0; direction < kModelIODirectionCount; ++direction) {
    if (!choices.allowed(direction)) {
      continue;
    }
    const float distance = modelIOGoalDistance(choices.target(direction), goal);
    if (distance < bestDistance) {
      best = direction;
      bestDistance = distance;
    }
  }
  return best;
}

} // namespace ai::runtime_io
