//
// AiPB - YaPB action execution context.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>, based on PODBot by Markus Klinge ("CountFloyd").
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_navigation_task_guard.h>
#include <ai/ai_yapb_action_execution_context.h>

namespace ai {
namespace {

constexpr float kNavigationReachDistance = 48.0f;

} // namespace

YaPBActionExecutionContext::YaPBActionExecutionContext(Bot &bot) : m_bot(&bot) {
}

bool YaPBActionExecutionContext::isAlive() const {
  return m_bot != nullptr && m_bot->pev != nullptr;
}

bool YaPBActionExecutionContext::allowsNavigationOverride() const {
  if (m_bot == nullptr) {
    return false;
  }

  return ai::allowsNavigationOverride(m_bot->getCurrentTaskId(), Task::Normal, Task::MoveToPosition);
}

bool YaPBActionExecutionContext::navigationNodeExists(int node) const {
  return graph.exists(node);
}

int YaPBActionExecutionContext::navigationNodeForPosition(const Vec3 &position) const {
  const Vector target { position.x, position.y, position.z };
  return graph.getNearest(target);
}

bool YaPBActionExecutionContext::isNavigationTargetReached(int node) const {
  if (m_bot == nullptr || m_bot->pev == nullptr || !graph.exists(node) || m_bot->m_currentNodeIndex != node) {
    return false;
  }

  const auto &path = graph[node];
  const float reachDistance = cr::max(kNavigationReachDistance, path.radius);
  return m_bot->pev->origin.distanceSq(path.origin) <= cr::sqrf(reachDistance);
}

void YaPBActionExecutionContext::moveToNode(int node) {
  if (m_bot == nullptr) {
    return;
  }

  const bool moveTaskActive = m_bot->getCurrentTaskId() == Task::MoveToPosition;
  const auto *task = moveTaskActive ? m_bot->getTask() : nullptr;
  const bool targetChanged = !moveTaskActive || task == nullptr || task->data != node;

  if (!moveTaskActive) {
    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
  }
  else if (targetChanged) {
    m_bot->clearSearchNodes();
  }

  m_bot->getTask()->data = node;
  m_bot->m_position.clear();
  m_bot->m_prevGoalIndex = node;
  m_bot->m_chosenGoalIndex = node;
}

void YaPBActionExecutionContext::moveToPosition(const Vec3 &position, int node) {
  if (m_bot == nullptr) {
    return;
  }

  const Vector target { position.x, position.y, position.z };
  const bool moveTaskActive = m_bot->getCurrentTaskId() == Task::MoveToPosition;
  const auto *task = moveTaskActive ? m_bot->getTask() : nullptr;
  const bool targetChanged = !moveTaskActive || task == nullptr || task->data != node ||
                             m_bot->m_position.distanceSq(target) > cr::sqrf(0.1f);

  if (!moveTaskActive) {
    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
  }
  else if (targetChanged) {
    m_bot->clearSearchNodes();
  }

  m_bot->getTask()->data = node;
  m_bot->m_position = target;
  m_bot->m_prevGoalIndex = node;
  m_bot->m_chosenGoalIndex = node;
}

} // namespace ai
