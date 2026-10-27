#pragma once

#include <cmath>

#include <aipb/explorer/engine_bridge.h>

// C++17-compatible control conversion and bounded GoldSrc probe scheduling.
namespace aipb::explorer::bridge {

inline bool valid(const AipbExplorerBridgeCommand &command) {
  return std::isfinite(command.forward) && command.forward >= -1.0f && command.forward <= 1.0f
    && std::isfinite(command.strafe) && command.strafe >= -1.0f && command.strafe <= 1.0f
    && std::isfinite(command.yaw_delta) && command.yaw_delta >= -180.0f && command.yaw_delta <= 180.0f
    && command.jump <= 1 && command.duck <= 1;
}

inline float yaw(float current, float delta) {
  const float wrapped = std::fmod(current + delta + 180.0f, 360.0f);
  return wrapped < 0.0f ? wrapped + 180.0f : wrapped - 180.0f;
}

// Resolve an exploration command relative to the bot's visible facing.
inline float initialViewYaw(float bodyYaw, float viewYaw) {
  return std::isfinite(bodyYaw) ? yaw(bodyYaw, 0.0f) : yaw(viewYaw, 0.0f);
}

inline void translateAxes(
  const AipbExplorerBridgeCommand &command,
  float maxSpeed,
  float &forwardSpeed,
  float &strafeSpeed
) {
  const float magnitude = std::sqrt(command.forward * command.forward + command.strafe * command.strafe);
  const float factor = magnitude > 1.0f ? 1.0f / magnitude : 1.0f;
  forwardSpeed = command.forward * factor * maxSpeed;
  strafeSpeed = command.strafe * factor * maxSpeed;
}

struct ProbeBudget {
  static constexpr int kMaxFrames = 128;
  static constexpr int kMaxDurationMs = 2000;
  static constexpr int kMaxSafetyFrames = 4096;

  int requested {};
  bool milliseconds {};
  int executed {};
  int simulatedMs {};

  [[nodiscard]] bool valid() const {
    return requested >= 1 && requested <= (milliseconds ? kMaxDurationMs : kMaxFrames);
  }

  [[nodiscard]] bool safetyLimit() const {
    return executed >= kMaxSafetyFrames;
  }

  [[nodiscard]] bool complete() const {
    return safetyLimit() || (milliseconds ? simulatedMs >= requested : executed >= requested);
  }

  void consume(int frameMs) {
    if (!complete() && frameMs >= 1 && frameMs <= 255) {
      ++executed;
      simulatedMs += frameMs;
    }
  }
};

}
