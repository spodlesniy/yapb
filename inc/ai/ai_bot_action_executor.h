//
// AiPB - YaPB AI action executor.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_executor.h>

class Bot;

namespace ai {

// Bridges high-level AI intents into the existing YaPB task and navigation systems. Task-owned actions remain
// under YaPB task-stack control; this executor only acknowledges matching observed tasks. It does not implement
// movement, pathfinding, collision, or GoldSrc input handling.
class BotActionExecutor final : public ActionExecutor {
private:
  Bot *m_bot {};

private:
  ActionResult executeMoveToNode(const Action &action);
  ActionResult executeMoveToPosition(const Action &action);
  ActionResult executeObservedTaskAction(const Action &action, const Observation &observation);
  bool isNavigationTargetReached(int node) const;

public:
  explicit BotActionExecutor(Bot &bot);

  ActionResult execute(const Action &action, const Observation &observation) override;

  bool isActionStillOwned(const Action &action) const;
};

} // namespace ai
