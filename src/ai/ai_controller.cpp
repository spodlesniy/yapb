//
// AiPB - AI controller implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
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
