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



bool YaPBActionExecutionContext::seekCover() {
  if (m_bot == nullptr || m_bot->pev == nullptr || game.isNullEntity(m_bot->m_lastEnemy)
      || !game.isAliveEntity(m_bot->m_lastEnemy) || m_bot->m_lastEnemyOrigin.empty()) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();
  if (!m_seekCoverActive) {
    if (currentTask != Task::Normal && currentTask != Task::MoveToPosition && currentTask != Task::SeekCover) {
      return false;
    }

    m_bot->ensureCurrentNodeIndex();
    const float maxDistance = m_bot->m_infectedEnemyTeam ? 2048.0f : 1024.0f;
    const int node = m_bot->findCoverNode(maxDistance);

    if (!graph.exists(node)) {
      return false;
    }

    m_seekCoverActive = true;
    m_seekCoverNode = node;
    m_seekCoverNavigationTaskCreated = false;

    if (currentTask == Task::SeekCover || currentTask == Task::MoveToPosition) {
      m_bot->clearTask(currentTask);
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
    m_seekCoverNavigationTaskCreated = true;
  }
  else if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    const auto activeTask = m_bot->getCurrentTaskId();
    if (activeTask != Task::Normal) {
      return false;
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, m_seekCoverNode, 0.0f, true);
  }

  m_bot->getTask()->data = m_seekCoverNode;
  m_bot->m_prevGoalIndex = m_seekCoverNode;
  m_bot->m_chosenGoalIndex = m_seekCoverNode;
  m_bot->m_aimFlags |= AimFlags::Nav;
  return true;
}

bool YaPBActionExecutionContext::isSeekCoverReached() const {
  if (m_bot == nullptr || m_bot->pev == nullptr || !m_seekCoverActive || !graph.exists(m_seekCoverNode)) {
    return false;
  }

  const auto &path = graph[m_seekCoverNode];
  const float reachDistance = cr::max(kNavigationReachDistance, path.radius);
  return m_bot->m_currentNodeIndex == m_seekCoverNode
      && m_bot->pev->origin.distanceSq(path.origin) <= cr::sqrf(reachDistance);
}

void YaPBActionExecutionContext::cancelSeekCover() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_seekCoverActive && m_seekCoverNavigationTaskCreated
      && m_bot->getCurrentTaskId() == Task::MoveToPosition) {
    m_bot->clearTask(Task::MoveToPosition);
  }

  if (m_seekCoverActive) {
    m_bot->clearSearchNodes();
    m_bot->m_prevGoalIndex = kInvalidNodeIndex;
    m_bot->m_chosenGoalIndex = kInvalidNodeIndex;
    m_bot->m_position.clear();
    m_bot->m_aimFlags &= ~AimFlags::Nav;
  }

  m_seekCoverActive = false;
  m_seekCoverNode = kInvalidNodeIndex;
  m_seekCoverNavigationTaskCreated = false;
}

bool YaPBActionExecutionContext::pickupItem() {
  if (m_bot == nullptr || m_bot->pev == nullptr || game.isNullEntity(m_bot->m_pickupItem)) {
    return false;
  }

  return m_bot->getCurrentTaskId() == Task::PickupItem;
}

void YaPBActionExecutionContext::cancelPickupItem() {
  if (m_bot == nullptr) {
    return;
  }

  m_bot->ensurePickupEntitiesClear();
}

bool YaPBActionExecutionContext::defuseBomb() {
  if (m_bot == nullptr || m_bot->pev == nullptr || !gameState.isBombPlanted()) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();

  if (currentTask == Task::DefuseBomb) {
    return true;
  }

  if (currentTask != Task::Normal) {
    return false;
  }

  m_bot->startTask(Task::DefuseBomb, TaskPri::DefuseBomb, kInvalidNodeIndex, 0.0f, false);
  return true;
}

void YaPBActionExecutionContext::cancelDefuseBomb() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_bot->getCurrentTaskId() == Task::DefuseBomb) {
    m_bot->clearTask(Task::DefuseBomb);
  }
}

bool YaPBActionExecutionContext::plantBomb() {
  if (m_bot == nullptr || m_bot->pev == nullptr || !m_bot->m_hasC4 || !m_bot->m_inBombZone
      || gameState.isBombPlanted()) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();

  if (currentTask == Task::PlantBomb) {
    return true;
  }

  if (currentTask != Task::Normal) {
    return false;
  }

  m_bot->startTask(Task::PlantBomb, TaskPri::PlantBomb, kInvalidNodeIndex, 0.0f, false);
  return true;
}

void YaPBActionExecutionContext::cancelPlantBomb() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_bot->getCurrentTaskId() == Task::PlantBomb) {
    m_bot->clearTask(Task::PlantBomb);
  }
}

bool YaPBActionExecutionContext::escapeFromBomb() {
  if (m_bot == nullptr || m_bot->pev == nullptr || !gameState.isBombPlanted()) {
    return false;
  }

  if (!m_escapeFromBombActive) {
    const auto currentTask = m_bot->getCurrentTaskId();

    if (currentTask != Task::Normal && currentTask != Task::MoveToPosition && currentTask != Task::EscapeFromBomb) {
      return false;
    }

    const auto &bombOrigin = gameState.getBombOrigin();

    if (bombOrigin.empty()) {
      return false;
    }

    const float safeRadius = m_bot->rg(1513.0f, 2048.0f);
    float nearestDistanceSq = kInfiniteDistance;
    int bestNode = kInvalidNodeIndex;

    for (const auto &path : graph) {
      if (path.origin.distanceSq(bombOrigin) < cr::sqrf(safeRadius) || m_bot->isOccupiedNode(path.number)) {
        continue;
      }

      const float distanceSq = m_bot->pev->origin.distanceSq(path.origin);

      if (distanceSq < nearestDistanceSq) {
        nearestDistanceSq = distanceSq;
        bestNode = path.number;
      }
    }

    if (!graph.exists(bestNode)) {
      bestNode = graph.getFarest(m_bot->pev->origin, safeRadius);
    }

    if (!graph.exists(bestNode)) {
      return false;
    }

    m_escapeFromBombActive = true;
    m_escapeFromBombNode = bestNode;
    m_escapeFromBombNavigationTaskCreated = false;

    if (currentTask == Task::MoveToPosition || currentTask == Task::EscapeFromBomb) {
      m_bot->clearTask(currentTask);
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, bestNode, 0.0f, true);
    m_escapeFromBombNavigationTaskCreated = true;
  }

  if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    if (m_bot->getCurrentTaskId() != Task::Normal) {
      return false;
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, m_escapeFromBombNode, 0.0f, true);
  }

  m_bot->getTask()->data = m_escapeFromBombNode;
  m_bot->m_position.clear();
  m_bot->m_prevGoalIndex = m_escapeFromBombNode;
  m_bot->m_chosenGoalIndex = m_escapeFromBombNode;
  m_bot->m_aimFlags |= AimFlags::Nav;
  return true;
}

bool YaPBActionExecutionContext::isEscapeFromBombReached() const {
  if (m_bot == nullptr || m_bot->pev == nullptr || !m_escapeFromBombActive
      || !graph.exists(m_escapeFromBombNode)) {
    return false;
  }

  const auto &path = graph[m_escapeFromBombNode];
  const float reachDistance = cr::max(kNavigationReachDistance, path.radius);

  return m_bot->m_currentNodeIndex == m_escapeFromBombNode
      && m_bot->pev->origin.distanceSq(path.origin) <= cr::sqrf(reachDistance);
}

void YaPBActionExecutionContext::cancelEscapeFromBomb() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_escapeFromBombActive && m_escapeFromBombNavigationTaskCreated
      && m_bot->getCurrentTaskId() == Task::MoveToPosition) {
    m_bot->clearTask(Task::MoveToPosition);
  }

  if (m_escapeFromBombActive) {
    m_bot->clearSearchNodes();
    m_bot->m_prevGoalIndex = kInvalidNodeIndex;
    m_bot->m_chosenGoalIndex = kInvalidNodeIndex;
    m_bot->m_position.clear();
    m_bot->m_aimFlags &= ~AimFlags::Nav;
  }

  m_escapeFromBombActive = false;
  m_escapeFromBombNode = kInvalidNodeIndex;
  m_escapeFromBombNavigationTaskCreated = false;
}



} // namespace ai
