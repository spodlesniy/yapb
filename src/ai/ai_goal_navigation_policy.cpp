//
// AiPB - deterministic waypoint-goal policy.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_goal_navigation_policy.h>

namespace ai {

Action GoalNavigationPolicy::decide(const Observation &observation) const {
  if (!observation.bot.alive || observation.bot.currentGoalNode < 0 || observation.bot.currentGoalNode == observation.bot.currentNode) {
    return {};
  }

  Action action {};
  action.type = ActionType::MoveToNode;
  action.targetType = TargetType::Node;
  action.targetNode = observation.bot.currentGoalNode;
  action.confidence = 1.0f;
  return action;
}

} // namespace ai
