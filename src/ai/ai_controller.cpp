//
// YaPB - AI controller implementation.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_controller.h>

namespace {

class LegacyPolicy final : public ai::Policy {
public:
   ai::Action decide (const ai::Observation &) const override {
      // The existing YaPB decision system remains authoritative in phase 1.
      // This policy is only the default backend for the new abstraction.
      return {};
   }
};

const LegacyPolicy g_legacyPolicy {};

} // namespace

namespace ai {

Controller::Controller (ControlMode mode) : m_mode (mode), m_policy (&g_legacyPolicy) {
}

void Controller::setPolicy (const Policy *policy) {
   m_policy = policy != nullptr ? policy : &g_legacyPolicy;
}

Action Controller::decide (const Observation &observation) const {
   return m_policy != nullptr ? m_policy->decide (observation) : Action {};
}

} // namespace ai
