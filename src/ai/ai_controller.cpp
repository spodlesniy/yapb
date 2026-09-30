//
// YaPB - AI controller implementation.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_controller.h>

namespace ai {

Controller::Controller (ControlMode mode) : m_mode (mode) {
}

void Controller::setPolicy (const Policy *policy) {
   m_policy = policy;
}

Action Controller::decide (const Observation &observation) const {
   return m_policy != nullptr ? m_policy->decide (observation) : Action {};
}

} // namespace ai
