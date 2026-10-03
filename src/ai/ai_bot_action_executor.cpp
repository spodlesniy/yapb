//
// AiPB - YaPB navigation action executor.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_action_task_mapping.h>
#include <ai/ai_bot_action_executor.h>
#include <ai/ai_navigation_task_guard.h>

#include <yapb.h>

#include <cmath>

namespace ai {

namespace {

constexpr float kNavigationReachDistance = 48.0f;

bool isFinitePosition(const Vec3 &position) {
  return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

} // namespace

BotActionExecutor::BotActionExecutor(Bot &bot) : m_bot(&bot) {
}

bool BotActionExecutor::isActionStillOwned(const Action &action) const {
  if (m_bot == nullptr) {
    return false;
  }

  if (action.type != ActionType::MoveToNode && action.type != ActionType::MoveToPosition) {
    return true;
  }

  return allowsNavigationOverride(m_bot->getCurrentTaskId(), Task::Normal, Task::MoveToPosition);
}

ActionResult BotActionExecutor::execute(const Action &action, const Observation &observation) {
  if (m_bot == nullptr || m_bot->pev == nullptr || !observation.bot.alive) {
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  // During the transitional Neural runtime, existing YaPB tasks remain
  // authoritative for non-navigation behavior. AI navigation must not
  // silently replace combat, objective, or other higher-priority tasks.
  // TODO: Replace task-stack acknowledgement with direct AI-owned execution for non-navigation actions.
  if (!isActionStillOwned(action)) {
    return { action.type, ActionResultType::Interrupted, 0.0f };
  }

  switch (action.type) {
  case ActionType::MoveToNode:
    return executeMoveToNode(action);

  case ActionType::MoveToPosition:
    return executeMoveToPosition(action);

  case ActionType::Wait:
  case ActionType::HoldPosition:
  case ActionType::Camp:
  case ActionType::SeekCover:
  case ActionType::AttackTarget:
  case ActionType::HuntTarget:
  case ActionType::PlantBomb:
  case ActionType::DefuseBomb:
  case ActionType::PickupItem:
  case ActionType::EscapeFromBomb:
  case ActionType::Fire:
    return executeObservedTaskAction(action, observation);

  default:
    return { action.type, ActionResultType::Rejected, 0.0f };
  }
}

// TODO: Replace this transitional acknowledgement path with direct execution of the corresponding AI action.
ActionResult BotActionExecutor::executeObservedTaskAction(const Action &action, const Observation &observation) {
  if (!actionMatchesObservedTask(action, observation)) {
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  return { action.type, ActionResultType::Completed, 0.0f };
}

ActionResult BotActionExecutor::executeMoveToNode(const Action &action) {
  const int node = action.targetNode;

  if (!graph.exists(node)) {
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  if (isNavigationTargetReached(node)) {
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  const bool targetChanged = m_bot->getCurrentTaskId() != Task::MoveToPosition || m_bot->getTask()->data != node;

  if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
  } else if (targetChanged) {
    m_bot->clearSearchNodes();
  }

  m_bot->getTask()->data = node;
  m_bot->m_position.clear();
  m_bot->m_prevGoalIndex = node;
  m_bot->m_chosenGoalIndex = node;

  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeMoveToPosition(const Action &action) {
  if (!isFinitePosition(action.targetPosition)) {
    return { action.type, ActionResultType::Invalid, 0.0f };
  }

  const Vector target {
    action.targetPosition.x,
    action.targetPosition.y,
    action.targetPosition.z,
  };

  const int node = graph.getNearest(target);

  if (!graph.exists(node)) {
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  if (isNavigationTargetReached(node)) {
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  const bool targetChanged = m_bot->getCurrentTaskId() != Task::MoveToPosition || m_bot->getTask()->data != node ||
                             m_bot->m_position.distanceSq(target) > cr::sqrf(0.1f);

  if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
  } else if (targetChanged) {
    m_bot->clearSearchNodes();
  }

  m_bot->getTask()->data = node;
  m_bot->m_position = target;
  m_bot->m_prevGoalIndex = node;
  m_bot->m_chosenGoalIndex = node;

  return { action.type, ActionResultType::Accepted, 0.0f };
}

bool BotActionExecutor::isNavigationTargetReached(int node) const {
  if (!graph.exists(node) || m_bot->m_currentNodeIndex != node) {
    return false;
  }

  const auto &path = graph[node];
  const float reachDistance = cr::max(kNavigationReachDistance, path.radius);
  return m_bot->pev->origin.distanceSq(path.origin) <= cr::sqrf(reachDistance);
}

} // namespace ai
