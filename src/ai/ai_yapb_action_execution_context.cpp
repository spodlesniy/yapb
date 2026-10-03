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

bool YaPBActionExecutionContext::attackTarget(int targetPlayer) {
  if (m_bot == nullptr || m_bot->pev == nullptr || targetPlayer <= 0 || targetPlayer > game.maxClients()) return false;
  auto *target = game.entityOfIndex(targetPlayer);
  if (game.isNullEntity(target) || !game.isPlayerEntity(target) || !game.isAliveEntity(target)) return false;
  const auto targetTeam = game.is(GameFlags::FreeForAll) ? game.getRealPlayerTeam(target) : game.getPlayerTeam(target);
  if (targetTeam == Team::Invalid || targetTeam == m_bot->m_team) return false;

  m_bot->m_enemy = target;
  m_bot->m_enemyOrigin = target->v.origin;
  m_bot->m_enemyBodyPartSet = nullptr;
  m_bot->m_enemySurpriseTime = 0.0f;
  m_bot->m_aimFlags |= AimFlags::Enemy;
  m_bot->m_moveToGoal = false;
  m_bot->m_checkTerrain = false;
  m_bot->m_wantsToFire = true;
  m_bot->m_navTimeset = game.time();
  m_bot->ignoreCollision();
  m_bot->focusEnemy();
  m_bot->attackMovement(false);
  return true;
}

void YaPBActionExecutionContext::cancelAttackTarget(int targetPlayer) {
  if (m_bot == nullptr) return;
  if (targetPlayer > 0 && m_bot->m_enemy != nullptr && !game.isNullEntity(m_bot->m_enemy)
      && game.indexOfEntity(m_bot->m_enemy) == targetPlayer) {
    m_bot->m_enemy = nullptr;
    m_bot->m_enemyOrigin.clear();
  }
  m_bot->m_aimFlags &= ~AimFlags::Enemy;
  m_bot->m_wantsToFire = false;
}

bool YaPBActionExecutionContext::huntTarget(int targetPlayer) {
  if (m_bot == nullptr || m_bot->pev == nullptr || targetPlayer <= 0 || targetPlayer > game.maxClients()) {
    return false;
  }

  auto *target = game.entityOfIndex(targetPlayer);
  if (game.isNullEntity(target) || !game.isPlayerEntity(target) || !game.isAliveEntity(target)) {
    return false;
  }

  const auto targetTeam = game.is(GameFlags::FreeForAll) ? game.getRealPlayerTeam(target) : game.getPlayerTeam(target);
  if (targetTeam == Team::Invalid || targetTeam == m_bot->m_team || !game.isNullEntity(m_bot->m_enemy)) {
    return false;
  }

  if (!m_huntTargetActive || m_huntTargetPlayer != targetPlayer) {
    m_huntTargetActive = true;
    m_huntTargetPlayer = targetPlayer;
    m_huntTargetOrigin = { target->v.origin.x, target->v.origin.y, target->v.origin.z };
    m_huntNavigationTaskCreated = false;
  }

  const auto targetOrigin = Vector { m_huntTargetOrigin.x, m_huntTargetOrigin.y, m_huntTargetOrigin.z };
  const int node = graph.getNearest(targetOrigin);
  if (!graph.exists(node)) {
    return false;
  }

  if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
    m_huntNavigationTaskCreated = true;
  }
  else if (m_bot->getTask()->data != node || m_bot->m_position.distanceSq(targetOrigin) > cr::sqrf(0.1f)) {
    m_bot->clearSearchNodes();
  }

  m_bot->getTask()->data = node;
  m_bot->m_position = targetOrigin;
  m_bot->m_prevGoalIndex = node;
  m_bot->m_chosenGoalIndex = node;
  m_bot->m_aimFlags |= AimFlags::Nav;
  return true;
}

bool YaPBActionExecutionContext::isHuntTargetReached(int targetPlayer) const {
  if (m_bot == nullptr || m_bot->pev == nullptr || !m_huntTargetActive || m_huntTargetPlayer != targetPlayer) {
    return false;
  }

  const auto targetOrigin = Vector { m_huntTargetOrigin.x, m_huntTargetOrigin.y, m_huntTargetOrigin.z };
  const int node = graph.getNearest(targetOrigin);
  if (!graph.exists(node)) {
    return false;
  }

  return m_bot->m_currentNodeIndex == node &&
         m_bot->pev->origin.distanceSq(graph[node].origin) <= cr::sqrf(cr::max(kNavigationReachDistance, graph[node].radius));
}

void YaPBActionExecutionContext::cancelHuntTarget(int targetPlayer) {
  if (m_bot == nullptr) {
    return;
  }

  if (m_huntTargetPlayer == targetPlayer) {
    if (m_huntNavigationTaskCreated) {
      m_bot->clearTask(Task::MoveToPosition);
    }
    m_bot->clearSearchNodes();
    m_bot->m_position.clear();
    m_huntTargetActive = false;
    m_huntTargetPlayer = -1;
    m_huntTargetOrigin = {};
    m_huntNavigationTaskCreated = false;
  }
}




} // namespace ai
