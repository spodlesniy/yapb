//
// AiPB - AI controller implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_controller.h>

namespace ai {

Controller::Controller(ControlMode mode) : m_mode(mode) {
}

void Controller::setPolicy(const Policy *policy) {
  m_policy = policy;
}

Action Controller::decide(const Observation &observation) const {
  if ((m_mode != ControlMode::Neural && m_mode != ControlMode::Training) || m_policy == nullptr) {
    return {};
  }

  return m_policy->decide(observation);
}

} // namespace ai
