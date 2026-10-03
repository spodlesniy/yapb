//
// AiPB - AI action executor.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <cmath>

#include <ai/ai_action_execution_context.h>
#include <ai/ai_action_task_mapping.h>
#include <ai/ai_bot_action_executor.h>

namespace ai {
namespace {

bool isFinitePosition(const Vec3 &position) {
  return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

} // namespace

BotActionExecutor::BotActionExecutor(ActionExecutionContext &context) : m_context(&context) {
}

bool BotActionExecutor::isActionStillOwned(const Action &action) const {
  if (m_context == nullptr) {
    return false;
  }

  if (action.type != ActionType::MoveToNode && action.type != ActionType::MoveToPosition) {
    return true;
  }

  return m_context->allowsNavigationOverride();
}

ActionResult BotActionExecutor::execute(const Action &action, const Observation &observation) {
  if (m_context == nullptr || !m_context->isAlive() || !observation.bot.alive) {
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  // During the transitional Neural runtime, existing YaPB tasks remain authoritative for non-navigation behavior.
  // TODO: Replace task-stack acknowledgement with direct AI-owned execution for non-navigation actions.
  if (!isActionStillOwned(action)) {
    return { action.type, ActionResultType::Interrupted, 0.0f };
  }

  switch (action.type) {
  case ActionType::MoveToNode:
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeMoveToNode(action);

  case ActionType::MoveToPosition:
    m_observedTaskActive = false;
    m_observedTaskAction = {};
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
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return { action.type, ActionResultType::Rejected, 0.0f };
  }
}

// TODO: Replace this transitional task-stack acknowledgement with direct execution of the corresponding AI action.
ActionResult BotActionExecutor::executeObservedTaskAction(const Action &action, const Observation &observation) {
  if (!m_observedTaskActive || !sameObservedTaskAction(action, m_observedTaskAction)) {
    m_observedTaskActive = false;

    if (!actionMatchesObservedTask(action, observation)) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    m_observedTaskAction = action;
    m_observedTaskActive = true;
    return { action.type, ActionResultType::Accepted, 0.0f };
  }

  if (actionMatchesObservedTask(action, observation)) {
    return { action.type, ActionResultType::Accepted, 0.0f };
  }

  m_observedTaskActive = false;
  m_observedTaskAction = {};
  return { action.type, ActionResultType::Completed, 0.0f };
}

ActionResult BotActionExecutor::executeMoveToNode(const Action &action) {
  const int node = action.targetNode;

  if (!m_context->navigationNodeExists(node)) {
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  if (m_context->isNavigationTargetReached(node)) {
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_context->moveToNode(node);
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeMoveToPosition(const Action &action) {
  if (!isFinitePosition(action.targetPosition)) {
    return { action.type, ActionResultType::Invalid, 0.0f };
  }

  const int node = m_context->navigationNodeForPosition(action.targetPosition);

  if (!m_context->navigationNodeExists(node)) {
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  if (m_context->isNavigationTargetReached(node)) {
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_context->moveToPosition(action.targetPosition, node);
  return { action.type, ActionResultType::Accepted, 0.0f };
}

} // namespace ai
