//
// AiPB - bounded flashbang avoidance yaw without immediate angle snapping.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <ai/ai_aim_event.h>

namespace ai {

constexpr float kFlashAvoidanceYawRate = 720.0f;

constexpr bool shouldApplyFlashAvoidance(float now, float until, bool enemyVisible, bool blinded) {
  return now < until && !enemyVisible && !blinded;
}

inline float flashAvoidanceYawStep(float currentYaw, float targetYaw, float elapsed) {
  const float maxStep = elapsed > 0.0f ? elapsed * kFlashAvoidanceYawRate : 0.0f;
  const float delta = aimAngleDifference(targetYaw, currentYaw);
  const float step = delta > maxStep ? maxStep : delta < -maxStep ? -maxStep : delta;
  return aimAngleDifference(currentYaw + step, 0.0f);
}

} // namespace ai
