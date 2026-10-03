//
// AiPB - AI action executor.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_executor.h>

namespace ai {

class ActionExecutionContext;

// Bridges high-level AI intents into runtime capabilities. Engine-specific integration is supplied by
// ActionExecutionContext implementations; actions without direct execution semantics remain under YaPB
// task-stack control.
class BotActionExecutor final : public ActionExecutor {
private:
  ActionExecutionContext *m_context {};
  Action m_observedTaskAction {};
  bool m_observedTaskActive {};
  Action m_directAttackAction {};
  bool m_directAttackTargetActive {};

private:
  ActionResult executeMoveToNode(const Action &action);
  ActionResult executeMoveToPosition(const Action &action);
  ActionResult executeAttackTarget(const Action &action, const Observation &observation);
  ActionResult executeObservedTaskAction(const Action &action, const Observation &observation);

public:
  explicit BotActionExecutor(ActionExecutionContext &context);

  ActionResult execute(const Action &action, const Observation &observation) override;
  void cancel() override;

  bool isActionStillOwned(const Action &action) const;
  bool suppressesLegacyTaskExecution() const;
};

} // namespace ai
