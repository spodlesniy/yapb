#pragma once

#include <cmath>
#include <cstdint>

#include <aipb/explorer/engine_bridge_math.h>

namespace aipb::explorer::bridge {

// Engine-observed evidence. Never infer traversability from the requested input.
struct TraceSample {
  AipbExplorerBridgeCommand command {};
  AipbExplorerBridgeState before {};
  AipbExplorerBridgeState after {};
  float maxSpeed {};
  uint8_t msec {};
  int oldButtons {};
};

inline bool validState(const AipbExplorerBridgeState &s) {
  return std::isfinite(s.x) && std::isfinite(s.y) && std::isfinite(s.z)
    && std::isfinite(s.vx) && std::isfinite(s.vy) && std::isfinite(s.vz)
    && std::isfinite(s.yaw) && std::isfinite(s.pitch)
    && s.yaw >= -180.0f && s.yaw < 180.0f && s.pitch >= -90.0f && s.pitch <= 90.0f
    && s.on_ground <= 1 && s.crouching <= 1;
}

// A fixed-capacity snapshot belongs in static storage, not a game frame stack.
struct TraceBuffer {
  TraceSample samples[ProbeBudget::kMaxSafetyFrames] {};
  int count {};
  int botIndex { -1 };
  bool completed {};

  void clear() {
    count = 0;
    botIndex = -1;
    completed = false;
  }

  bool append(const TraceSample &sample) {
    if (completed || count >= ProbeBudget::kMaxSafetyFrames || !valid(sample.command)
      || sample.msec == 0 || !std::isfinite(sample.maxSpeed) || sample.maxSpeed < 0.0f
      || !validState(sample.before) || !validState(sample.after)
      || sample.after.tick != sample.before.tick + 1
      || (count != 0 && (sample.before.tick != samples[count - 1].after.tick
        || sample.before.x != samples[count - 1].after.x
        || sample.before.y != samples[count - 1].after.y
        || sample.before.z != samples[count - 1].after.z))) {
      return false;
    }
    samples[count++] = sample;
    return true;
  }
};

struct ReplayDifference {
  double position {};
  double velocity {};
  float yawDegrees {};
  float pitchDegrees {};
  bool stanceMismatch {};
  bool groundMismatch {};
  bool valid {};

  [[nodiscard]] bool within(double positionTolerance, double velocityTolerance,
    float angularTolerance) const {
    return valid && position <= positionTolerance && velocity <= velocityTolerance
      && yawDegrees <= angularTolerance && pitchDegrees <= angularTolerance
      && !stanceMismatch && !groundMismatch;
  }
};

inline ReplayDifference compareStates(const AipbExplorerBridgeState &expected,
  const AipbExplorerBridgeState &actual) {
  ReplayDifference result {};
  result.valid = validState(expected) && validState(actual);
  if (!result.valid) {
    return result;
  }
  const double dx = actual.x - expected.x;
  const double dy = actual.y - expected.y;
  const double dz = actual.z - expected.z;
  const double dvx = actual.vx - expected.vx;
  const double dvy = actual.vy - expected.vy;
  const double dvz = actual.vz - expected.vz;
  result.position = std::sqrt(dx * dx + dy * dy + dz * dz);
  result.velocity = std::sqrt(dvx * dvx + dvy * dvy + dvz * dvz);
  result.yawDegrees = std::abs(yaw(actual.yaw, -expected.yaw));
  result.pitchDegrees = std::abs(actual.pitch - expected.pitch);
  result.stanceMismatch = actual.crouching != expected.crouching;
  result.groundMismatch = actual.on_ground != expected.on_ground;
  return result;
}

} // namespace aipb::explorer::bridge
