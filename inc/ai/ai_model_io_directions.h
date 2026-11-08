//
// AiPB - trace-authorized local movement choices for external Model I/O trials.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstdint>

#include <ai/ai_server_state.h>

namespace ai::runtime_io {

constexpr uint8_t kModelIODirectionCount = 3;
constexpr float kModelIODirectionDistance = 96.0f;
constexpr float kModelIODirectionTolerance = 0.75f;

enum class ModelIODirection : uint8_t {
  Forward = 0,
  Left = 1,
  Right = 2,
};

inline const char *modelIODirectionName(int direction) {
  switch (direction) {
  case 0: return "forward";
  case 1: return "left";
  case 2: return "right";
  default: return "none";
  }
}

// The remote endpoint can derive the three nominal positions from exported
// self origin/yaw, but only the engine knows which hull traces are clear.
inline Vec3 modelIODirectionTarget(const Vec3 &origin, float yawDegrees, uint8_t direction) {
  if (!finiteVector(origin) || !std::isfinite(yawDegrees) ||
      direction >= kModelIODirectionCount) {
    return {};
  }
  const float offset = direction == static_cast<uint8_t>(ModelIODirection::Left) ? 90.0f
      : direction == static_cast<uint8_t>(ModelIODirection::Right) ? -90.0f : 0.0f;
  const float radians = (yawDegrees + offset) * 0.017453292519943295f;
  return { origin.x + std::cos(radians) * kModelIODirectionDistance,
           origin.y + std::sin(radians) * kModelIODirectionDistance,
           origin.z };
}

// No candidate is authorized by construction. An engine TraceHull must
// independently admit it before an external decision may select it.
class ModelIODirectionCandidates final {
private:
  Vec3 m_targets[kModelIODirectionCount] {};
  bool m_allowed[kModelIODirectionCount] {};
  uint8_t m_count {};

public:
  bool allow(uint8_t direction, const Vec3 &target) {
    if (direction >= kModelIODirectionCount || m_allowed[direction] ||
        !finiteVector(target)) {
      return false;
    }
    m_allowed[direction] = true;
    m_targets[direction] = target;
    ++m_count;
    return true;
  }

  int match(const Vec3 &target) const {
    if (!finiteVector(target)) {
      return -1;
    }
    for (uint8_t i = 0; i < kModelIODirectionCount; ++i) {
      if (m_allowed[i] &&
          std::abs(m_targets[i].x - target.x) <= kModelIODirectionTolerance &&
          std::abs(m_targets[i].y - target.y) <= kModelIODirectionTolerance &&
          std::abs(m_targets[i].z - target.z) <= kModelIODirectionTolerance) {
        return i;
      }
    }
    return -1;
  }

  uint8_t count() const { return m_count; }
  uint8_t mask() const {
    uint8_t result {};
    for (uint8_t i = 0; i < kModelIODirectionCount; ++i) {
      if (m_allowed[i]) {
        result = static_cast<uint8_t>(result | (1u << i));
      }
    }
    return result;
  }
  bool allowed(uint8_t direction) const {
    return direction < kModelIODirectionCount && m_allowed[direction];
  }
  Vec3 target(uint8_t direction) const {
    return allowed(direction) ? m_targets[direction] : Vec3 {};
  }
  int first() const {
    for (uint8_t i = 0; i < kModelIODirectionCount; ++i) {
      if (m_allowed[i]) {
        return i;
      }
    }
    return -1;
  }
};

} // namespace ai::runtime_io
