//
// AiPB - one-shot deterministic integration-test policy.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>

#include <ai/ai_policy.h>

namespace ai {

// No GoldSrc API, enemy coordinates, or YaPB tactical decisions are available here.
class MockMovePolicy final : public Policy {
private:
  mutable bool m_issued {};
  bool m_armed {};
  Vec3 m_target {};

public:
  bool arm(const Vec3 &target) {
    if (m_armed || !std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z)) {
      return false;
    }
    m_target = target;
    m_armed = true;
    m_issued = false;
    return true;
  }

  Action decide(const Observation &observation) const override {
    if (!m_armed || m_issued || !observation.bot.alive) {
      return {};
    }
    m_issued = true;
    Action result {};
    result.type = ActionType::MoveToPosition;
    result.targetType = TargetType::Position;
    result.targetPosition = m_target;
    result.confidence = 1.0f;
    return result;
  }

  void reset() {
    m_issued = false;
    m_armed = false;
    m_target = {};
  }

  bool armed() const {
    return m_armed;
  }
};

} // namespace ai
