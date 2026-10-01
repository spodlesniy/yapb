//
// AiPB - deterministic waypoint-goal policy.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_policy.h>

namespace ai {

// Transitional policy that exposes YaPB's already selected navigation goal
// through the AI action interface. It deliberately does not choose a new goal.
class GoalNavigationPolicy final : public Policy {
public:
   Action decide (const Observation &observation) const override;
};

} // namespace ai
