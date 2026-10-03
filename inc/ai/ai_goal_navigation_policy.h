//
// AiPB - deterministic task-aware teacher policy.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_policy.h>

namespace ai {

// Deterministic teacher policy that exposes the current YaPB task through
// the AI action interface whenever the observation contains enough data.
// Unsupported tasks fall back to the selected navigation goal.
class GoalNavigationPolicy final : public Policy {
public:
  Action decide(const Observation &observation) const override;
};

} // namespace ai
