//
// AiPB - AI action loop.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_pipeline.h>
#include <ai/ai_controller.h>

namespace ai {

// Connects observations to policy decisions and validated action execution.
// A policy is queried only when no action is active; the pipeline continues
// the active action until it reaches a terminal result.
class ActionLoop final {
private:
  const Controller *m_controller {};
  ActionPipeline *m_pipeline {};

public:
  ActionLoop(const Controller &controller, ActionPipeline &pipeline) : m_controller(&controller), m_pipeline(&pipeline) {
  }

  ActionResult step(const Observation &observation) {
    if (m_pipeline->isActive()) {
      return m_pipeline->execute(Action {}, observation);
    }

    const auto action = m_controller->decide(observation);
    if (action.type == ActionType::None) {
      return { ActionType::None, ActionResultType::None, 0.0f };
    }

    return m_pipeline->execute(action, observation);
  }

  bool cancel() {
    return m_pipeline->cancel();
  }

  void reset() {
    m_pipeline->reset();
  }

  bool isActive() const {
    return m_pipeline->isActive();
  }

  const Action &activeAction() const {
    return m_pipeline->activeAction();
  }

  const ActionResult &result() const {
    return m_pipeline->result();
  }
};

} // namespace ai
