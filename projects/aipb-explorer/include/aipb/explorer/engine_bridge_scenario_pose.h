#pragma once

#include <cmath>

#include <aipb/explorer/engine_bridge_scenario_start.h>

namespace aipb::explorer::scenario {

enum class PoseInitStatus {
  Ready,
  Busy,
  InvalidScenario,
  BotNotGrounded,
  SpeedUnavailable,
  StanceMismatch,
  InvalidVelocity
};

inline const char *poseInitStatusName(PoseInitStatus status) {
  switch (status) {
  case PoseInitStatus::Ready: return "Ready";
  case PoseInitStatus::Busy: return "Busy";
  case PoseInitStatus::InvalidScenario: return "InvalidScenario";
  case PoseInitStatus::BotNotGrounded: return "BotNotGrounded";
  case PoseInitStatus::SpeedUnavailable: return "SpeedUnavailable";
  case PoseInitStatus::StanceMismatch: return "StanceMismatch";
  case PoseInitStatus::InvalidVelocity: return "InvalidVelocity";
  }

  return "InvalidScenario";
}

struct PoseInitPlan {
  bool setPosition {};
  float position[3] {};
  float velocity[3] {};
  float yaw {};
  float pitch {};
  bool crouching {};
};

// Build a pose-only plan. Omitted velocity components and stance stay unchanged.
// Engine collision, floor support, and bot identity must still be checked live.
inline PoseInitStatus planScenarioPose(
  const Scenario &scenario,
  const ScenarioStartObservation &actual,
  bool explorerActive,
  PoseInitPlan &result
) {
  if (!validScenario(scenario)) {
    return PoseInitStatus::InvalidScenario;
  }

  if (explorerActive) {
    return PoseInitStatus::Busy;
  }

  if (!actual.onGround) {
    return PoseInitStatus::BotNotGrounded;
  }

  if (!std::isfinite(actual.maxSpeed) || actual.maxSpeed < 10.0f) {
    return PoseInitStatus::SpeedUnavailable;
  }

  if (scenario.hasBot("crouching") && scenario.crouching != actual.crouching) {
    return PoseInitStatus::StanceMismatch;
  }

  PoseInitPlan next {};
  next.setPosition = scenario.hasBot("x");
  constexpr const char *velocityKeys[] { "vx", "vy", "vz" };

  for (int axis = 0; axis < 3; ++axis) {
    const double position = next.setPosition
      ? scenario.position[axis] : actual.position[axis];
    if (!std::isfinite(position) || std::abs(position) > 16384.0) {
      return PoseInitStatus::InvalidScenario;
    }
    next.position[axis] = static_cast<float>(position);
    next.velocity[axis] = scenario.hasBot(velocityKeys[axis])
      ? scenario.velocity[axis] : actual.velocity[axis];

    if (!std::isfinite(next.velocity[axis]) || std::abs(next.velocity[axis]) > 400.0f) {
      return PoseInitStatus::InvalidVelocity;
    }
  }

  next.yaw = scenario.hasBot("yaw") ? scenario.yaw : actual.yaw;
  next.pitch = scenario.hasBot("pitch") ? scenario.pitch : actual.pitch;
  if (!std::isfinite(next.yaw) || !std::isfinite(next.pitch)) {
    return PoseInitStatus::InvalidScenario;
  }
  next.crouching = actual.crouching;
  result = next;
  return PoseInitStatus::Ready;
}

} // namespace aipb::explorer::scenario
