//
// AiPB - training reward provider.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_result.h>

namespace ai {

// Calculates the reward associated with a completed training transition.
// Reward policy stays outside the recorder so training semantics can evolve
// independently from transition storage and runtime execution.
class RewardProvider {
public:
  virtual float compute(const Observation &observation, const Action &action, const Observation &nextObservation,
                        const ActionResult &result) const = 0;
};

// Neutral default used until a game-specific reward policy is configured.
// It intentionally contributes no reward and does not encode training heuristics.
class ZeroRewardProvider final : public RewardProvider {
public:
  float compute(const Observation &, const Action &, const Observation &, const ActionResult &) const override {
    return 0.0f;
  }
};

} // namespace ai
