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
// ActionExecutionContext implementations; task-owned actions remain under YaPB task-stack control until their
// direct AI-owned execution semantics are implemented.
class BotActionExecutor final : public ActionExecutor {
private:
  ActionExecutionContext *m_context {};
  Action m_observedTaskAction {};
  bool m_observedTaskActive {};

private:
  ActionResult executeMoveToNode(const Action &action);
  ActionResult executeMoveToPosition(const Action &action);
  ActionResult executeObservedTaskAction(const Action &action, const Observation &observation);

public:
  explicit BotActionExecutor(ActionExecutionContext &context);

  ActionResult execute(const Action &action, const Observation &observation) override;

  bool isActionStillOwned(const Action &action) const;
};

} // namespace ai
