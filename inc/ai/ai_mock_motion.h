//
// AiPB - deterministic mock direct-movement safety and reach contract.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>

#include <ai/ai_controller.h>
#include <ai/ai_observation.h>

namespace ai {

constexpr int kMockDirectNavigationNode = -2;
constexpr float kMockTargetHorizontalTolerance = 12.0f;
constexpr float kMockTargetVerticalTolerance = 18.0f;

inline bool mockTrialModeAllowed(ControlMode mode) {
  return mode == ControlMode::Neural;
}

inline bool mockReachedTarget(const Vec3 &current, const Vec3 &target) {
  if (!std::isfinite(current.x) || !std::isfinite(current.y) || !std::isfinite(current.z) ||
      !std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z)) {
    return false;
  }
  const double dx = static_cast<double>(current.x) - target.x;
  const double dy = static_cast<double>(current.y) - target.y;
  return dx * dx + dy * dy <=
      static_cast<double>(kMockTargetHorizontalTolerance) * kMockTargetHorizontalTolerance &&
      std::abs(static_cast<double>(current.z) - target.z) <= kMockTargetVerticalTolerance;
}

inline float mockDistanceToTarget(const Vec3 &current, const Vec3 &target) {
  const double dx = static_cast<double>(current.x) - target.x;
  const double dy = static_cast<double>(current.y) - target.y;
  const double dz = static_cast<double>(current.z) - target.z;
  return static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
}

} // namespace ai
