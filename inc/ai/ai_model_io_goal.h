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

// A recorded outcome belongs to a physically completed, correlated and fully
// recovered leg. Tentative directions never enter the history.
constexpr uint8_t kModelIOGoalHistoryCapacity = 9;
constexpr float kModelIOGoalRevisitRadius = 36.0f;
constexpr float kModelIOGoalProgressThreshold = 12.0f;
constexpr uint8_t kModelIOGoalStallLegs = 3;

enum class ModelIOGoalHistoryResult {
  Continue,
  Cycle,
  NoProgress,
  Invalid,
};

class ModelIOGoalHistory final {
private:
  Vec3 m_positions[kModelIOGoalHistoryCapacity] {};
  uint8_t m_count {};
  uint8_t m_stalled {};
  float m_bestDistance {};

public:
  void reset() {
    m_count = 0;
    m_stalled = 0;
    m_bestDistance = 0.0f;
  }

  bool begin(const Vec3 &start, const Vec3 &goal) {
    reset();
    if (!finiteVector(start) || !finiteVector(goal)) {
      return false;
    }
    m_positions[0] = start;
    m_count = 1;
    m_bestDistance = modelIOGoalDistance(start, goal);
    return std::isfinite(m_bestDistance);
  }

  bool active() const { return m_count > 0; }
  uint8_t count() const { return m_count; }
  uint8_t stalled() const { return m_stalled; }
  float bestDistance() const { return m_bestDistance; }

  bool isRevisit(const Vec3 &candidate) const {
    if (!active() || !finiteVector(candidate)) {
      return false;
    }
    for (uint8_t i = 0; i < m_count; ++i) {
      if (modelIOGoalDistance(candidate, m_positions[i]) <=
          kModelIOGoalRevisitRadius) {
        return true;
      }
    }
    return false;
  }

  ModelIOGoalHistoryResult recordConfirmed(const Vec3 &observed,
                                           const Vec3 &goal) {
    if (!active() || m_count >= kModelIOGoalHistoryCapacity ||
        !finiteVector(observed) || !finiteVector(goal)) {
      return ModelIOGoalHistoryResult::Invalid;
    }
    bool revisited = false;
    // Compare against earlier arrivals, not the immediately departed point.
    for (uint8_t i = 0; i + 1 < m_count; ++i) {
      if (modelIOGoalDistance(observed, m_positions[i]) <=
          kModelIOGoalRevisitRadius) {
        revisited = true;
        break;
      }
    }
    const float distance = modelIOGoalDistance(observed, goal);
    if (!std::isfinite(distance)) {
      return ModelIOGoalHistoryResult::Invalid;
    }
    if (distance + kModelIOGoalProgressThreshold < m_bestDistance) {
      m_stalled = 0;
    }
    else if (m_stalled < kModelIOGoalStallLegs) {
      ++m_stalled;
    }
    if (distance < m_bestDistance) {
      m_bestDistance = distance;
    }
    m_positions[m_count++] = observed;
    if (revisited) {
      return ModelIOGoalHistoryResult::Cycle;
    }
    return m_stalled >= kModelIOGoalStallLegs
        ? ModelIOGoalHistoryResult::NoProgress
        : ModelIOGoalHistoryResult::Continue;
  }
};

// Never treat the fixed-radius revisit veto as an authorization. It can only
// remove choices that passed the engine's current collision trace.
inline int modelIOGoalChooseNovelDirection(
    const ModelIODirectionCandidates &choices, const Vec3 &goal,
    const ModelIOGoalHistory &history) {
  if (!finiteVector(goal) || !history.active()) {
    return -1;
  }
  int best = -1;
  float bestDistance = std::numeric_limits<float>::infinity();
  for (uint8_t direction = 0; direction < kModelIODirectionCount; ++direction) {
    if (!choices.allowed(direction)) {
      continue;
    }
    const auto target = choices.target(direction);
    if (history.isRevisit(target) && !modelIOGoalReached(target, goal)) {
      continue;
    }
    const float distance = modelIOGoalDistance(target, goal);
    if (distance < bestDistance) {
      best = direction;
      bestDistance = distance;
    }
  }
  return best;
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
