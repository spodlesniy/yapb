//
// AiPB - AI action execution pipeline.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_executor.h>
#include <ai/ai_action_validator.h>

namespace ai {

// Validates an action before forwarding it to the runtime executor.
// Invalid actions never reach the executor.
class ActionPipeline final {
private:
  const ActionExecutor *m_executor {};

public:
  explicit ActionPipeline(const ActionExecutor &executor)
    : m_executor(&executor) {
  }

  ActionResult execute(const Action &action, const Observation &observation) const {
    if (!ActionValidator::validate(action, observation).isValid()) {
      return { action.type, ActionResultType::Invalid, 0.0f };
    }

    return m_executor->execute(action, observation);
  }
};

} // namespace ai
