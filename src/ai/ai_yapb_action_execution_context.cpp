//
// AiPB - YaPB action execution context.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>, based on PODBot by Markus Klinge ("CountFloyd").
//
// SPDX-License-Identifier: MIT
//

// crlib provides placement new before STL <new>; yapb.h must stay first on MSVC x86.
#include <yapb.h>

#include <cmath>

#include <ai/ai_bot_adapter.h>
#include <ai/ai_hunt_progress_guard.h>
#include <ai/ai_navigation_task_guard.h>
#include <ai/ai_objective_navigation_guard.h>
#include <ai/ai_yapb_action_execution_context.h>

namespace ai {
namespace {

constexpr float kNavigationReachDistance = 48.0f;

float getPlayerNoiseEndTime(edict_t *player) {
  for (const auto &client : util.getClients()) {
    if (client.ent == player) {
      return client.noise.last;
    }
  }
  return -1.0f;
}

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

  const bool legacyObjectiveActive =
      (m_bot->m_team == Team::CT && gameState.isBombPlanted())
      || (m_bot->m_team == Team::Terrorist && hasDroppedBombObjective());

  return ai::allowsNavigationOverride(
      m_bot->getCurrentTaskId(), Task::Normal, Task::MoveToPosition, legacyObjectiveActive);
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

  const float reachDistanceSq = m_bot->getNavigationReachDistanceSq();
  return m_bot->pev->origin.distanceSq(m_bot->m_pathOrigin) < reachDistanceSq;
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

  if (m_bot->m_moveSpeed > 20.0f) {
    const auto directMove = (target->v.origin - m_bot->pev->origin).normalize2d_apx();
    TraceResult tr {};

    if (m_bot->isBlockedForward(directMove, &tr)) {
      m_bot->m_moveSpeed = 0.0f;

      const bool wallOnLeft = m_bot->checkWallOnLeft(96.0f);
      const bool wallOnRight = m_bot->checkWallOnRight(96.0f);

      if (m_bot->m_strafeSpeed < 0.0f && wallOnLeft) {
        m_bot->m_strafeSpeed = 0.0f;
      }
      else if (m_bot->m_strafeSpeed > 0.0f && wallOnRight) {
        m_bot->m_strafeSpeed = 0.0f;
      }

      if (cr::fzero(m_bot->m_strafeSpeed)) {
        if (!wallOnLeft) {
          m_bot->m_strafeSpeed = -m_bot->pev->maxspeed;
        }
        else if (!wallOnRight) {
          m_bot->m_strafeSpeed = m_bot->pev->maxspeed;
        }
      }
    }
  }

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

bool YaPBActionExecutionContext::aimAtTarget(int targetPlayer) {
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
  m_bot->m_wantsToFire = false;
  m_bot->focusEnemy();
  m_bot->m_wantsToFire = false;
  return true;
}

void YaPBActionExecutionContext::cancelAimAtTarget(int targetPlayer) {
  if (m_bot == nullptr) return;
  if (targetPlayer > 0 && m_bot->m_enemy != nullptr && !game.isNullEntity(m_bot->m_enemy)
      && game.indexOfEntity(m_bot->m_enemy) == targetPlayer) {
    m_bot->m_enemy = nullptr;
    m_bot->m_enemyOrigin.clear();
  }
  m_bot->m_aimFlags &= ~AimFlags::Enemy;
  m_bot->m_wantsToFire = false;
}

bool YaPBActionExecutionContext::followPlayer(int targetPlayer) {
  if (m_bot == nullptr || m_bot->pev == nullptr || targetPlayer <= 0 || targetPlayer > game.maxClients()) return false;
  auto *target = game.entityOfIndex(targetPlayer);
  if (game.isNullEntity(target) || !game.isPlayerEntity(target) || !game.isAliveEntity(target) || target == m_bot->ent()) return false;
  const auto targetTeam = game.is(GameFlags::FreeForAll) ? game.getRealPlayerTeam(target) : game.getPlayerTeam(target);
  if (targetTeam == Team::Invalid || targetTeam != m_bot->m_team) return false;
  const auto currentTask = m_bot->getCurrentTaskId();
  if (currentTask == Task::FollowUser && m_bot->m_targetEntity == target) return true;
  if (currentTask != Task::Normal) return false;
  m_bot->m_targetEntity = target;
  m_bot->m_followWaitTime = 0.0f;
  m_bot->startTask(Task::FollowUser, TaskPri::FollowUser, kInvalidNodeIndex, 0.0f, true);
  return true;
}

bool YaPBActionExecutionContext::throwGrenade(const Vec3 &position) {
  if (m_bot == nullptr || m_bot->pev == nullptr || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return false;
  if (m_bot->getCurrentTaskId() != Task::Normal && m_bot->getCurrentTaskId() != Task::ThrowExplosive) return false;
  m_bot->m_throw = { position.x, position.y, position.z };
  if (m_bot->getCurrentTaskId() == Task::Normal) m_bot->startTask(Task::ThrowExplosive, TaskPri::Throw, kInvalidNodeIndex, 0.0f, false);
  return true;
}

void YaPBActionExecutionContext::cancelThrowGrenade() {
  if (m_bot == nullptr) return;
  if (m_bot->getCurrentTaskId() == Task::ThrowExplosive) m_bot->clearTask(Task::ThrowExplosive);
  m_bot->m_isUsingGrenade = false;
  m_bot->m_aimFlags &= ~AimFlags::Grenade;
  m_bot->m_throw.clear();
}

bool YaPBActionExecutionContext::throwFlashbang(const Vec3 &position) {
  if (m_bot == nullptr || m_bot->pev == nullptr || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return false;
  if (m_bot->getCurrentTaskId() != Task::Normal && m_bot->getCurrentTaskId() != Task::ThrowFlashbang) return false;
  m_bot->m_throw = { position.x, position.y, position.z };
  if (m_bot->getCurrentTaskId() == Task::Normal) m_bot->startTask(Task::ThrowFlashbang, TaskPri::Throw, kInvalidNodeIndex, 0.0f, false);
  return true;
}

void YaPBActionExecutionContext::cancelThrowFlashbang() {
  if (m_bot == nullptr) return;
  if (m_bot->getCurrentTaskId() == Task::ThrowFlashbang) m_bot->clearTask(Task::ThrowFlashbang);
  m_bot->m_isUsingGrenade = false;
  m_bot->m_aimFlags &= ~AimFlags::Grenade;
  m_bot->m_throw.clear();
}

bool YaPBActionExecutionContext::throwSmoke(const Vec3 &position) {
  if (m_bot == nullptr || m_bot->pev == nullptr || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();
  if (currentTask != Task::Normal && currentTask != Task::ThrowSmoke) {
    return false;
  }

  m_bot->m_throw = { position.x, position.y, position.z };
  m_bot->m_aiSmokeTargetActive = true;
  if (currentTask == Task::Normal) {
    m_bot->startTask(Task::ThrowSmoke, TaskPri::Throw, kInvalidNodeIndex, 0.0f, false);
  }
  return true;
}

void YaPBActionExecutionContext::cancelThrowSmoke() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_bot->getCurrentTaskId() == Task::ThrowSmoke) {
    m_bot->clearTask(Task::ThrowSmoke);
  }

  m_bot->m_isUsingGrenade = false;
  m_bot->m_aimFlags &= ~AimFlags::Grenade;
  m_bot->m_aiSmokeTargetActive = false;
  m_bot->m_throw.clear();
}

bool YaPBActionExecutionContext::changeWeapon(WeaponType weaponType) {
  if (m_bot == nullptr || m_bot->pev == nullptr) return false;
  const auto requestedType = static_cast<int>(weaponType);
  if (requestedType <= static_cast<int>(WeaponType::Unknown) || requestedType > static_cast<int>(WeaponType::Heavy)) return false;
  if (m_bot->m_weaponType == requestedType) return true;
  const int weapons = m_bot->pev->weapons;
  for (int weaponId = 1; weaponId < kMaxWeapons; ++weaponId) {
    if (!(weapons & cr::bit(weaponId)) || conf.getWeaponType(weaponId) != requestedType) continue;
    m_bot->selectWeaponById(weaponId);
    return true;
  }
  return false;
}

void YaPBActionExecutionContext::cancelChangeWeapon() {
}

void YaPBActionExecutionContext::cancelFollowPlayer(int targetPlayer) {
  if (m_bot == nullptr) return;
  if (targetPlayer > 0 && !game.isNullEntity(m_bot->m_targetEntity) && game.indexOfEntity(m_bot->m_targetEntity) == targetPlayer) {
    m_bot->m_targetEntity = nullptr;
  }
  if (m_bot->getCurrentTaskId() == Task::FollowUser) m_bot->clearTask(Task::FollowUser);
}

bool YaPBActionExecutionContext::huntTarget(int targetPlayer, const Vec3 &position) {
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
    m_huntTargetOrigin = position;
    m_huntBestDistance = -1.0f;
    m_huntLastProgressTime = game.time();
    m_huntSeenEvidenceTime = m_bot->m_seeEnemyTime;
    m_huntNoiseEndTime = getPlayerNoiseEndTime(target);
    m_huntNavigationTaskCreated = false;
  }

  const auto newEvidenceSeenTime = m_bot->m_seeEnemyTime;
  const auto newEvidenceNoiseEndTime = getPlayerNoiseEndTime(target);
  const auto rememberedOrigin = Vector { m_huntTargetOrigin.x, m_huntTargetOrigin.y, m_huntTargetOrigin.z };
  const auto candidateOrigin = Vector { position.x, position.y, position.z };
  if (shouldReanchorHuntTarget(
      hasNewerHuntEvidence(newEvidenceSeenTime, newEvidenceNoiseEndTime,
                           m_huntSeenEvidenceTime, m_huntNoiseEndTime),
      rememberedOrigin.distanceSq(candidateOrigin))) {
    m_huntTargetOrigin = position;
    m_huntBestDistance = -1.0f;
    m_huntLastProgressTime = game.time();
    m_huntSeenEvidenceTime = newEvidenceSeenTime;
    m_huntNoiseEndTime = newEvidenceNoiseEndTime;
  }

  const auto targetOrigin = Vector { m_huntTargetOrigin.x, m_huntTargetOrigin.y, m_huntTargetOrigin.z };
  const int node = graph.getNearest(targetOrigin);
  if (!graph.exists(node)) {
    return false;
  }

  const int fromNode = graph.getNearest(m_bot->pev->origin);
  if (graph.exists(fromNode)) {
    const float routeDistance = planner.preciseDistance(fromNode, node);
    if (isUsableHuntGraphDistance(routeDistance)
        && hasMeaningfulHuntProgress(m_huntBestDistance, routeDistance)) {
      m_huntBestDistance = routeDistance;
      m_huntLastProgressTime = game.time();
    }
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

bool YaPBActionExecutionContext::isHuntTargetStalled(int targetPlayer) const {
  if (!m_huntTargetActive || m_huntTargetPlayer != targetPlayer) {
    return false;
  }

  return isHuntProgressStalled(game.time(), m_huntLastProgressTime);
}

void YaPBActionExecutionContext::consumeHuntTargetMemory(int targetPlayer) {
  if (m_bot == nullptr || !m_huntTargetActive || m_huntTargetPlayer != targetPlayer
      || targetPlayer <= 0 || targetPlayer > game.maxClients()) {
    return;
  }

  auto *target = game.entityOfIndex(targetPlayer);
  if (game.isNullEntity(target) || m_bot->m_lastEnemy != target) {
    return;
  }

  if (hasNewerHuntEvidence(m_bot->m_seeEnemyTime, getPlayerNoiseEndTime(target),
                           m_huntSeenEvidenceTime, m_huntNoiseEndTime)) {
    return;
  }

  m_bot->m_lastEnemyOrigin.clear();
}

void YaPBActionExecutionContext::cancelHuntTarget(int targetPlayer) {
  if (m_bot == nullptr) {
    return;
  }

  if (m_huntTargetPlayer == targetPlayer) {
    if (m_huntNavigationTaskCreated) {
      m_bot->clearTask(Task::MoveToPosition);
    }
    if (m_bot->getCurrentTaskId() == Task::Hunt) {
      m_bot->clearTask(Task::Hunt);
    }
    m_bot->clearSearchNodes();
    m_bot->m_position.clear();
    m_huntTargetActive = false;
    m_huntTargetPlayer = -1;
    m_huntTargetOrigin = {};
    m_huntBestDistance = -1.0f;
    m_huntLastProgressTime = -1.0f;
    m_huntSeenEvidenceTime = -1.0f;
    m_huntNoiseEndTime = -1.0f;
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

bool YaPBActionExecutionContext::retreat() {
  if (m_bot == nullptr || m_bot->pev == nullptr || game.isNullEntity(m_bot->m_lastEnemy)
      || !game.isAliveEntity(m_bot->m_lastEnemy) || m_bot->m_lastEnemyOrigin.empty()) return false;

  const auto currentTask = m_bot->getCurrentTaskId();

  if (!m_retreatActive) {
    if (currentTask != Task::Normal && currentTask != Task::MoveToPosition && currentTask != Task::SeekCover) return false;

    m_bot->ensureCurrentNodeIndex();
    const float maxDistance = m_bot->m_infectedEnemyTeam ? 2048.0f : 1024.0f;
    const int node = m_bot->findCoverNode(maxDistance);

    if (!graph.exists(node)) return false;

    m_retreatActive = true;
    m_retreatNode = node;
    m_retreatNavigationTaskCreated = false;
    m_retreatHideTaskCreated = false;

    if (currentTask == Task::MoveToPosition || currentTask == Task::SeekCover) {
      m_bot->clearTask(currentTask);
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
    m_retreatNavigationTaskCreated = true;
  }
  else if (m_retreatHideTaskCreated) {
    return currentTask == Task::Hide;
  }
  else if (currentTask == Task::Normal) {
    if (m_bot->m_currentNodeIndex == m_retreatNode) {
      m_bot->startHideBehavior();
      m_retreatHideTaskCreated = true;
      return true;
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, m_retreatNode, 0.0f, true);
  }
  else if (currentTask != Task::MoveToPosition) {
    return false;
  }

  m_bot->getTask()->data = m_retreatNode;
  m_bot->m_prevGoalIndex = m_retreatNode;
  m_bot->m_chosenGoalIndex = m_retreatNode;
  m_bot->m_aimFlags |= AimFlags::Nav;
  return true;
}

bool YaPBActionExecutionContext::isRetreatReached() const {
  return m_bot != nullptr && m_bot->pev != nullptr && m_retreatActive && m_retreatHideTaskCreated
      && m_bot->getCurrentTaskId() == Task::Hide;
}

void YaPBActionExecutionContext::cancelRetreat() {
  if (m_bot == nullptr) return;

  if (m_retreatActive && m_retreatNavigationTaskCreated && m_bot->getCurrentTaskId() == Task::MoveToPosition) {
    m_bot->clearTask(Task::MoveToPosition);
  }

  if (m_retreatActive) {
    m_bot->clearSearchNodes();
    m_bot->m_prevGoalIndex = kInvalidNodeIndex;
    m_bot->m_chosenGoalIndex = kInvalidNodeIndex;
    m_bot->m_position.clear();
    m_bot->m_aimFlags &= ~AimFlags::Nav;
  }

  m_retreatActive = false;
  m_retreatNode = kInvalidNodeIndex;
  m_retreatNavigationTaskCreated = false;
  m_retreatHideTaskCreated = false;
}

bool YaPBActionExecutionContext::wait() {
  if (m_bot == nullptr || m_bot->pev == nullptr) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();

  if (currentTask == Task::Pause) {
    return true;
  }

  if (currentTask != Task::Normal) {
    return false;
  }

  const auto duration = m_bot->rg(30.0f, 60.0f);
  m_bot->startTask(Task::Pause, TaskPri::Pause, kInvalidNodeIndex, game.time() + duration, false);
  return true;
}

void YaPBActionExecutionContext::cancelWait() {
  if (m_bot != nullptr && m_bot->getCurrentTaskId() == Task::Pause) {
    m_bot->clearTask(Task::Pause);
  }
}

bool YaPBActionExecutionContext::holdPosition() {
  if (m_bot == nullptr || m_bot->pev == nullptr) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();

  if (currentTask == Task::Pause) {
    return true;
  }

  if (currentTask != Task::Normal) {
    return false;
  }

  const auto duration = m_bot->rg(30.0f, 60.0f);
  m_bot->startTask(Task::Pause, TaskPri::Pause, kInvalidNodeIndex, game.time() + duration, false);
  return true;
}

void YaPBActionExecutionContext::cancelHoldPosition() {
  if (m_bot != nullptr && m_bot->getCurrentTaskId() == Task::Pause) {
    m_bot->clearTask(Task::Pause);
  }
}

bool YaPBActionExecutionContext::hide() {
  if (m_bot == nullptr || m_bot->pev == nullptr) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();

  if (currentTask == Task::Hide) {
    return true;
  }

  if (currentTask != Task::Normal || !game.isAliveEntity(m_bot->m_lastEnemy) || m_bot->m_lastEnemyOrigin.empty()) {
    return false;
  }

  m_bot->findValidNode();

  if (!graph.exists(m_bot->m_currentNodeIndex) || m_bot->m_path == nullptr) {
    return false;
  }

  m_bot->startHideBehavior();
  return true;
}

void YaPBActionExecutionContext::cancelHide() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_bot->getCurrentTaskId() == Task::Hide) {
    m_bot->clearTask(Task::Hide);
  }

  m_bot->m_campButtons = 0;
  m_bot->m_prevGoalIndex = kInvalidNodeIndex;
  m_bot->m_aimFlags &= ~AimFlags::Camp;
}

bool YaPBActionExecutionContext::camp() {
  if (m_bot == nullptr || m_bot->pev == nullptr) return false;

  if (m_bot->m_team == Team::CT
      && gameState.isBombPlanted()
      && !m_bot->isBombDefusing(gameState.getBombOrigin())
      && !m_bot->isOutOfBombTimer()) {
    return false;
  }

  const auto currentTask = m_bot->getCurrentTaskId();
  if (currentTask == Task::Camp) return true;
  if (currentTask != Task::Normal) return false;
  const auto duration = m_bot->rg(cv_camping_time_min.as<float>(), cv_camping_time_max.as<float>());
  m_bot->startTask(Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time() + duration, true);
  return true;
}

void YaPBActionExecutionContext::cancelCamp() {
  if (m_bot != nullptr && m_bot->getCurrentTaskId() == Task::Camp) m_bot->clearTask(Task::Camp);
}

bool YaPBActionExecutionContext::fireBreakable() {
  if (m_bot == nullptr || m_bot->pev == nullptr || !game.isBreakableEntity(m_bot->m_breakableEntity)
      || !m_bot->m_breakableOrigin.empty() == false || !game.isNullEntity(m_bot->m_enemy)) {
    return false;
  }

  return m_bot->getCurrentTaskId() == Task::ShootBreakable;
}

void YaPBActionExecutionContext::cancelFireBreakable() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_bot->getCurrentTaskId() == Task::ShootBreakable) {
    m_bot->clearTask(Task::ShootBreakable);
  }

  m_bot->m_breakableEntity = nullptr;
  m_bot->m_breakableOrigin.clear();
}

bool YaPBActionExecutionContext::pickupItem() {
  if (m_bot == nullptr || m_bot->pev == nullptr || game.isNullEntity(m_bot->m_pickupItem)
      || (m_bot->m_hasC4 && m_bot->m_inBombZone)) {
    return false;
  }

  const bool plantedC4PickupActive =
      m_bot->m_pickupType == Pickup::PlantedC4 && gameState.isBombPlanted();

  return ai::ownsObjectivePickupTask(
      m_bot->getCurrentTaskId(), Task::PickupItem, Task::Pause, plantedC4PickupActive);
}

void YaPBActionExecutionContext::cancelPickupItem() {
  if (m_bot == nullptr) {
    return;
  }

  // PickupItem hands a nearby planted C4 to DefuseBomb. Preserve the entity
  // across that semantic action transition so defuseBomb_() can keep using it
  // directly even when the bomb is tucked behind nearby geometry.
  if (m_bot->getCurrentTaskId() == Task::DefuseBomb
      && m_bot->m_pickupType == Pickup::PlantedC4
      && !game.isNullEntity(m_bot->m_pickupItem)) {
    m_bot->m_states &= ~Sense::PickupItem;
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

  if (m_bot->m_pickupType == Pickup::PlantedC4) {
    m_bot->m_pickupItem = nullptr;
    m_bot->m_pickupType = Pickup::None;
  }
  m_bot->m_entity.clear();
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

namespace {
constexpr float kExploreMinPathDistance = 256.0f;
constexpr float kExploreMaxPathDistance = 1536.0f;
}

bool YaPBActionExecutionContext::explore() {
  if (m_bot == nullptr || m_bot->pev == nullptr) return false;

  const auto currentTask = m_bot->getCurrentTaskId();
  if (!m_exploreActive) {
    if (currentTask != Task::Normal && currentTask != Task::MoveToPosition) return false;

    m_bot->ensureCurrentNodeIndex();
    if (!graph.exists(m_bot->m_currentNodeIndex)) return false;

    int bestNode = kInvalidNodeIndex;
    int bestHistoryCount = INT_MAX;
    float bestDistance = -1.0f;
    int fallbackNode = kInvalidNodeIndex;
    float fallbackDistance = -1.0f;

    for (const auto &path : graph) {
      if (path.number == m_bot->m_currentNodeIndex || (path.flags & NodeFlag::Ladder) || m_bot->isOccupiedNode(path.number, true)) continue;

      const float distance = planner.dist(m_bot->m_currentNodeIndex, path.number);
      if (distance <= 0.0f || distance >= kInfiniteHeuristic) continue;

      if (distance > fallbackDistance) {
        fallbackDistance = distance;
        fallbackNode = path.number;
      }

      if (distance < kExploreMinPathDistance || distance > kExploreMaxPathDistance) continue;

      int historyCount = 0;
      for (const auto goal : m_bot->m_goalHist) {
        if (goal == path.number) ++historyCount;
      }

      if (historyCount < bestHistoryCount || (historyCount == bestHistoryCount && distance > bestDistance)) {
        bestHistoryCount = historyCount;
        bestDistance = distance;
        bestNode = path.number;
      }
    }

    if (!graph.exists(bestNode)) bestNode = fallbackNode;
    if (!graph.exists(bestNode)) return false;

    m_exploreActive = true;
    m_exploreNode = bestNode;
    m_exploreNavigationTaskCreated = false;

    if (currentTask == Task::MoveToPosition) m_bot->clearTask(Task::MoveToPosition);
    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, bestNode, 0.0f, true);
    m_exploreNavigationTaskCreated = true;
  }
  else if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    if (m_bot->getCurrentTaskId() != Task::Normal) return false;
    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, m_exploreNode, 0.0f, true);
  }

  m_bot->getTask()->data = m_exploreNode;
  m_bot->m_prevGoalIndex = m_exploreNode;
  m_bot->m_chosenGoalIndex = m_exploreNode;
  m_bot->m_aimFlags |= AimFlags::Nav;
  return true;
}

bool YaPBActionExecutionContext::isExploreReached() const {
  if (m_bot == nullptr || m_bot->pev == nullptr || !m_exploreActive || !graph.exists(m_exploreNode)) return false;
  const auto &path = graph[m_exploreNode];
  const float reachDistance = cr::max(kNavigationReachDistance, path.radius);
  return m_bot->m_currentNodeIndex == m_exploreNode
      && m_bot->pev->origin.distanceSq(path.origin) <= cr::sqrf(reachDistance);
}

void YaPBActionExecutionContext::cancelExplore() {
  if (m_bot == nullptr) return;
  if (m_exploreActive && m_exploreNavigationTaskCreated && m_bot->getCurrentTaskId() == Task::MoveToPosition)
    m_bot->clearTask(Task::MoveToPosition);
  if (m_exploreActive) {
    m_bot->clearSearchNodes();
    m_bot->m_prevGoalIndex = kInvalidNodeIndex;
    m_bot->m_chosenGoalIndex = kInvalidNodeIndex;
    m_bot->m_position.clear();
    m_bot->m_aimFlags &= ~AimFlags::Nav;
  }
  m_exploreActive = false;
  m_exploreNode = kInvalidNodeIndex;
  m_exploreNavigationTaskCreated = false;
}

bool YaPBActionExecutionContext::protectObjective() {
  if (m_bot == nullptr || m_bot->pev == nullptr || m_bot->m_team != Team::Terrorist
      || !game.mapIs(MapFlags::Demolition) || !gameState.isBombPlanted() || gameState.getBombOrigin().empty()) {
    return false;
  }

  const auto &bombOrigin = gameState.getBombOrigin();
  auto currentTask = m_bot->getCurrentTaskId();

  // Legacy combat/cover/blind tasks temporarily preempt movement. Keep the
  // chosen bomb-defense waypoint and let the legacy task finish rather than
  // reporting an interrupted semantic objective and discarding its route.
  if (m_protectObjectiveActive
      && isTransientPlantedBombDefenseTask(currentTask, Task::Attack, Task::SeekCover, Task::Blind)) {
    return true;
  }

  if (currentTask != Task::Normal && currentTask != Task::MoveToPosition
      && currentTask != Task::Camp && currentTask != Task::Hunt) {
    return false;
  }

  if (!m_protectObjectiveActive) {
    m_bot->ensureCurrentNodeIndex();
    const int fromNode = graph.exists(m_bot->m_currentNodeIndex)
        ? m_bot->m_currentNodeIndex : graph.getNearest(m_bot->pev->origin);
    const int bombNode = graph.getNearest(bombOrigin);
    if (!graph.exists(fromNode) || !graph.exists(bombNode)) {
      return false;
    }

    const float secondsLeft = gameState.getBombTimeLeft();
    const float maxSpeed = m_bot->pev->maxspeed;
    const auto routeTo = [&](int node) {
      return graph.exists(node) ? planner.preciseDistance(fromNode, node)
                                : kPlantedBombReinforcementUnreachableRoute;
    };
    const auto canArriveAt = [&](int node) {
      return canArriveAtPlantedBombDefenseInTime(routeTo(node), maxSpeed, secondsLeft);
    };

    // Reserve positions teammates are already approaching, not only occupied ones.
    const auto crowdCost = [&](int candidate) {
      float cost = 0.0f;
      for (const auto &ally : bots) {
        if (!ally || ally == m_bot || !ally->m_isAlive || ally->m_team != Team::Terrorist) continue;
        const auto task = ally->getCurrentTaskId();
        const int goal = ally->m_chosenGoalIndex;
        if ((task != Task::MoveToPosition && task != Task::Camp) || !graph.exists(goal)
            || graph[goal].origin.distanceSq(bombOrigin) > cr::sqrf(kPlantedBombReinforcementRadius)) continue;
        cost += bombDefenseCrowdingCost(graph[candidate].origin.distanceSq(graph[goal].origin));
      }
      return cost;
    };

    int node = kInvalidNodeIndex;
    if (m_bot->m_defuseNotified) {
      // When CTs are defusing, the bomb itself takes precedence over a flank.
      node = bombNode;
    }
    else {
      // Preserve the legacy defense choice for nearby T bots. Its search
      // radius is relative to the bot and cannot serve distant reinforcements.
      if (routeTo(bombNode) <= kPlantedBombReinforcementFarRoute) {
        const int localNode = m_bot->findDefendNode(bombOrigin, true, true);
        if (graph.exists(localNode)
            && !(graph[localNode].flags & (NodeFlag::Ladder | NodeFlag::CTOnly))
            && graph[localNode].origin.distanceSq(bombOrigin)
                <= cr::sqrf(kPlantedBombReinforcementRadius)
            && canArriveAt(localNode) && crowdCost(localNode) == 0.0f) {
          node = localNode;
        }
      }

      if (!graph.exists(node)) {
        // Reinforcements choose destinations AROUND the planted C4, not random
        // nodes near their current position. First prefer C4-visible points;
        // on sparse maps fall back to nearby reachable flanking positions.
        for (int pass = 0; pass < 2 && !graph.exists(node); ++pass) {
          int bestNode = kInvalidNodeIndex;
          float bestRouteDistance = kPlantedBombReinforcementUnreachableRoute;
          bool bestCamp = false;
          float bestCrowdCost = 0.0f;
          int bestConnections = -1;

          for (const auto &path : graph) {
            if ((path.flags & (NodeFlag::Ladder | NodeFlag::CTOnly))
                || path.number == bombNode || m_bot->isOccupiedNode(path.number)
                || path.origin.distanceSq(bombOrigin) > cr::sqrf(kPlantedBombReinforcementRadius)
                || (pass == 0 && !vistab.visible(path.number, bombNode))) {
              continue;
            }

            const float distance = routeTo(path.number);
            if (!canArriveAtPlantedBombDefenseInTime(distance, maxSpeed, secondsLeft)) {
              continue;
            }
            const bool isCamp = (path.flags & NodeFlag::Camp) != 0;
            const float crowding = crowdCost(path.number);
            const int connections = countWalkableBombDefenseConnections(path.links, [&](const auto &link) {
              return graph.exists(link.index) && link.index != path.number
                  && !(link.flags & PathFlag::Jump)
                  && !(graph[link.index].flags & NodeFlag::Ladder);
            });
            if (isBetterDistributedBombDefenseNode(
                distance, isCamp, crowding, path.number,
                bestRouteDistance, bestCamp, bestCrowdCost, bestNode,
                connections, bestConnections)) {
              bestNode = path.number;
              bestRouteDistance = distance;
              bestCamp = isCamp;
              bestCrowdCost = crowding;
              bestConnections = connections;
            }
          }
          node = bestNode;
        }
      }

      // If map waypoints offer no suitable side position, reaching the bomb
      // is still better than using a disconnected or random defense node.
      if (!graph.exists(node)) {
        node = bombNode;
      }
    }

    if (!graph.exists(node) || !canArriveAt(node)) {
      return false;
    }

    m_protectObjectiveActive = true;
    m_protectObjectiveNode = node;
    m_protectObjectiveNavigationTaskCreated = false;

    if (currentTask == Task::MoveToPosition || currentTask == Task::Camp || currentTask == Task::Hunt) {
      m_bot->clearTask(currentTask);
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
    m_protectObjectiveNavigationTaskCreated = true;
    currentTask = Task::MoveToPosition;
  }
  else if (m_bot->m_defuseNotified) {
    const int node = graph.getNearest(bombOrigin);

    if (!graph.exists(node)) {
      return false;
    }

    m_protectObjectiveNode = node;

    if (currentTask == Task::Camp || currentTask == Task::Hunt) {
      m_bot->clearTask(currentTask);
      currentTask = m_bot->getCurrentTaskId();
    }

    if (currentTask == Task::Normal) {
      m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
      m_protectObjectiveNavigationTaskCreated = true;
      currentTask = Task::MoveToPosition;
    }
    else if (currentTask == Task::MoveToPosition && m_bot->getTask()->data != node) {
      m_bot->clearSearchNodes();
    }
  }
  else if (currentTask == Task::Normal || currentTask == Task::Camp) {
    if (!graph.exists(m_protectObjectiveNode)) {
      return false;
    }

    const float reachDistance = cr::max(kNavigationReachDistance, graph[m_protectObjectiveNode].radius);
    const bool defenseNodeReached = hasReachedPlantedBombDefenseNode(
      m_bot->m_currentNodeIndex, m_protectObjectiveNode,
      m_bot->pev->origin.distanceSq(graph[m_protectObjectiveNode].origin),
      cr::sqrf(reachDistance));

    if (!defenseNodeReached) {
      // A combat interruption can end in Normal or an unrelated Camp. Resume
      // the original defense route; do not defend the bomb from a random spot.
      if (currentTask == Task::Camp) {
        m_bot->clearTask(Task::Camp);
      }
      m_bot->clearSearchNodes();
      m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition,
                       m_protectObjectiveNode, 0.0f, true);
      m_protectObjectiveNavigationTaskCreated = true;
      currentTask = Task::MoveToPosition;
    }
    else if (currentTask == Task::Normal) {
      const float bombTimeLeft = gameState.getBombTimeLeft();
      if (bombTimeLeft > 0.0f) {
        m_bot->startTask(Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time() + bombTimeLeft, true);
        currentTask = Task::Camp;
      }
    }
  }
  else if (currentTask != Task::MoveToPosition) {
    return false;
  }

  if (currentTask == Task::MoveToPosition) {
    if (m_bot->m_isStuck) {
      const int currentNode = m_bot->m_currentNodeIndex;
      const int nearestNode = m_bot->findNearestNode();

      if (graph.exists(nearestNode) && nearestNode != currentNode) {
        m_bot->changeNodeIndex(nearestNode);
        m_bot->clearSearchNodes();
      }
      else if (m_bot->findNextBestNode() && m_bot->m_currentNodeIndex != currentNode) {
        m_bot->clearSearchNodes();
      }
    }

    m_bot->getTask()->data = m_protectObjectiveNode;
    m_bot->m_prevGoalIndex = m_protectObjectiveNode;
    m_bot->m_chosenGoalIndex = m_protectObjectiveNode;
    m_bot->m_aimFlags |= AimFlags::Nav;
  }

  return true;
}

bool YaPBActionExecutionContext::isProtectObjectiveReached() const {
  return m_protectObjectiveActive && (m_bot == nullptr || m_bot->pev == nullptr || !gameState.isBombPlanted());
}

void YaPBActionExecutionContext::cancelProtectObjective() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_protectObjectiveActive) {
    if (m_bot->getCurrentTaskId() == Task::MoveToPosition) {
      m_bot->clearTask(Task::MoveToPosition);
    }
    else if (m_bot->getCurrentTaskId() == Task::Camp) {
      m_bot->clearTask(Task::Camp);
    }

    m_bot->clearSearchNodes();
    m_bot->m_prevGoalIndex = kInvalidNodeIndex;
    m_bot->m_chosenGoalIndex = kInvalidNodeIndex;
    m_bot->m_position.clear();
    m_bot->m_campButtons = 0;
    m_bot->m_aimFlags &= ~(AimFlags::Nav | AimFlags::Camp);
  }

  m_protectObjectiveActive = false;
  m_protectObjectiveNode = kInvalidNodeIndex;
  m_protectObjectiveNavigationTaskCreated = false;
}

bool YaPBActionExecutionContext::reload(WeaponType weaponType) {
  if (m_bot == nullptr || m_bot->pev == nullptr || m_bot->usesKnife() || m_bot->m_isUsingGrenade) {
    return false;
  }

  if (!m_reloadActive) {
    const bool secondary = weaponType == WeaponType::Pistol;
    m_reloadStateIssued = secondary ? static_cast<int>(Reload::Secondary) : static_cast<int>(Reload::Primary);
    m_bot->m_reloadState = m_reloadStateIssued;

    if (m_bot->getAmmo() <= 0) {
      m_bot->m_reloadState = Reload::None;
      return false;
    }

    m_reloadActive = true;
  }

  m_bot->checkReload();
  return m_reloadActive;
}

bool YaPBActionExecutionContext::isReloadCompleted() const {
  if (!m_reloadActive) {
    return false;
  }

  if (m_bot == nullptr || m_bot->pev == nullptr) {
    return true;
  }

  return m_bot->m_reloadState != m_reloadStateIssued || !m_bot->m_isReloading;
}

void YaPBActionExecutionContext::cancelReload() {
  if (m_bot != nullptr) {
    // Preserve a state that checkReload() already advanced to (for example
    // Primary -> Secondary). Only clear the state still owned by this action.
    if (m_bot->m_reloadState == m_reloadStateIssued) {
      m_bot->m_reloadState = Reload::None;
    }
    m_bot->m_isReloading = false;
  }
  m_reloadActive = false;
  m_reloadStateIssued = static_cast<int>(Reload::None);
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

    const auto isEligibleEscapeNode = [&](const Path &path) {
      if (path.origin.distanceSq(bombOrigin) < cr::sqrf(safeRadius) || m_bot->isOccupiedNode(path.number)) {
        return false;
      }

      if ((m_bot->m_team == Team::CT && (path.flags & NodeFlag::TerroristOnly))
          || (m_bot->m_team == Team::Terrorist && (path.flags & NodeFlag::CTOnly))) {
        return false;
      }

      return true;
    };

    for (const auto &path : graph) {
      if (!(path.flags & NodeFlag::Camp) || !isEligibleEscapeNode(path)) {
        continue;
      }

      const float distanceSq = m_bot->pev->origin.distanceSq(path.origin);

      if (distanceSq < nearestDistanceSq) {
        nearestDistanceSq = distanceSq;
        bestNode = path.number;
      }
    }

    if (!graph.exists(bestNode)) {
      nearestDistanceSq = kInfiniteDistance;

      for (const auto &path : graph) {
        if (!isEligibleEscapeNode(path)) {
          continue;
        }

        const float distanceSq = m_bot->pev->origin.distanceSq(path.origin);

        if (distanceSq < nearestDistanceSq) {
          nearestDistanceSq = distanceSq;
          bestNode = path.number;
        }
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
    m_escapeFromBombHoldTaskCreated = false;

    if (currentTask == Task::MoveToPosition || currentTask == Task::EscapeFromBomb) {
      m_bot->clearTask(currentTask);
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, bestNode, 0.0f, true);
    m_escapeFromBombNavigationTaskCreated = true;
  }

  if (isEscapeFromBombReached()) {
    if (m_bot->getCurrentTaskId() == Task::MoveToPosition) {
      m_bot->clearTask(Task::MoveToPosition);
      m_escapeFromBombNavigationTaskCreated = false;
    }

    if (m_bot->getCurrentTaskId() != Task::Camp) {
      if (m_bot->getCurrentTaskId() != Task::Normal) {
        return false;
      }

      const float holdTime = game.time() + cr::max(1.0f, gameState.getBombTimeLeft());
      m_bot->startTask(Task::Camp, TaskPri::Camp, kInvalidNodeIndex, holdTime, true);
      m_escapeFromBombHoldTaskCreated = true;
    }

    return true;
  }

  if (m_escapeFromBombHoldTaskCreated && m_bot->getCurrentTaskId() == Task::Camp) {
    m_bot->clearTask(Task::Camp);
    m_escapeFromBombHoldTaskCreated = false;
  }

  if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    if (m_bot->getCurrentTaskId() != Task::Normal) {
      return false;
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, m_escapeFromBombNode, 0.0f, true);
    m_escapeFromBombNavigationTaskCreated = true;
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

  if (m_escapeFromBombActive && m_escapeFromBombHoldTaskCreated
      && m_bot->getCurrentTaskId() == Task::Camp) {
    m_bot->clearTask(Task::Camp);
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
  m_escapeFromBombHoldTaskCreated = false;
}

bool YaPBActionExecutionContext::rescueHostage() {
  if (m_bot == nullptr || m_bot->pev == nullptr || !m_bot->m_hasHostage
      || !game.mapIs(MapFlags::HostageRescue)) {
    return false;
  }

  if (!m_rescueHostageActive) {
    const auto currentTask = m_bot->getCurrentTaskId();

    if (currentTask != Task::Normal && currentTask != Task::MoveToPosition) {
      return false;
    }

    m_bot->ensureCurrentNodeIndex();
    const int node = m_bot->findGoalPost(GoalTactic::RescueHostage, nullptr, nullptr);

    if (!graph.exists(node) || !(graph[node].flags & NodeFlag::Rescue)) {
      return false;
    }

    m_rescueHostageActive = true;
    m_rescueHostageNode = node;
    m_rescueHostageNavigationTaskCreated = false;

    if (currentTask == Task::MoveToPosition) {
      m_bot->clearTask(Task::MoveToPosition);
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, node, 0.0f, true);
    m_rescueHostageNavigationTaskCreated = true;
  }

  if (m_bot->getCurrentTaskId() != Task::MoveToPosition) {
    if (m_bot->getCurrentTaskId() != Task::Normal) {
      return false;
    }

    m_bot->startTask(Task::MoveToPosition, TaskPri::MoveToPosition, m_rescueHostageNode, 0.0f, true);
  }

  m_bot->getTask()->data = m_rescueHostageNode;
  m_bot->m_position.clear();
  m_bot->m_prevGoalIndex = m_rescueHostageNode;
  m_bot->m_chosenGoalIndex = m_rescueHostageNode;
  m_bot->m_aimFlags |= AimFlags::Nav;
  return true;
}

void YaPBActionExecutionContext::cancelRescueHostage() {
  if (m_bot == nullptr) {
    return;
  }

  if (m_rescueHostageActive && m_rescueHostageNavigationTaskCreated
      && m_bot->getCurrentTaskId() == Task::MoveToPosition) {
    m_bot->clearTask(Task::MoveToPosition);
  }

  if (m_rescueHostageActive) {
    m_bot->clearSearchNodes();
    m_bot->m_prevGoalIndex = kInvalidNodeIndex;
    m_bot->m_chosenGoalIndex = kInvalidNodeIndex;
    m_bot->m_position.clear();
    m_bot->m_aimFlags &= ~AimFlags::Nav;
  }

  m_rescueHostageActive = false;
  m_rescueHostageNode = kInvalidNodeIndex;
  m_rescueHostageNavigationTaskCreated = false;
}



} // namespace ai
