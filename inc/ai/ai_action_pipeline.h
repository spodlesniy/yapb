//
// AiPB - AI action execution pipeline.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_executor.h>
#include <ai/ai_action_state.h>
#include <ai/ai_action_validator.h>

namespace ai {

// Validates new actions and serializes execution of one active action.
// While an action is active, new policy outputs are ignored and the current
// action is executed again until the executor reports a terminal result.
class ActionPipeline final {
private:
  ActionExecutor *m_executor {};
  ActionState &m_state;

public:
  ActionPipeline(ActionExecutor &executor, ActionState &state) : m_executor(&executor), m_state(state) {
  }

  ActionPipeline(const ActionPipeline &) = delete;
  ActionPipeline &operator=(const ActionPipeline &) = delete;
  ActionPipeline(ActionPipeline &&) = delete;
  ActionPipeline &operator=(ActionPipeline &&) = delete;

  ActionResult execute(const Action &action, const Observation &observation) {
    if (!m_state.isActive()) {
      if (!ActionValidator::validate(action, observation).isValid()) {
        return { action.type, ActionResultType::Invalid, 0.0f };
      }

      m_state.start(action);
    }

    const auto &activeAction = m_state.action();
    const auto result = m_executor->execute(activeAction, observation);
    m_state.updateResult(result);
    return m_state.result();
  }

  bool cancel() {
    m_executor->cancel();
    return m_state.cancel();
  }

  void reset() {
    m_executor->cancel();
    m_state.reset();
  }

  bool isActive() const {
    return m_state.isActive();
  }

  const Action &activeAction() const {
    return m_state.action();
  }

  const ActionResult &result() const {
    return m_state.result();
  }
};

} // namespace ai
