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

bool hasObservedEnemyTarget(const Action &action, const Observation &observation) {
  if (action.targetType != TargetType::Player || observation.combat.enemyEntity != action.targetPlayer
      || !(observation.combat.perceptionFlags & static_cast<uint32_t>(PerceptionFlag::SeeingEnemy))) {
    return false;
  }

  const auto count = observation.playerCount > kMaxObservedPlayers ? kMaxObservedPlayers : observation.playerCount;
  for (size_t i = 0; i < count; ++i) {
    const auto &player = observation.players[i];
    if (player.valid && player.alive && player.enemy && player.visible && player.entityIndex == action.targetPlayer) {
      return true;
    }
  }
  return false;
}


bool hasObservedLastEnemyTarget(const Action &action, const Observation &observation) {
  if (action.targetType != TargetType::Player || observation.combat.lastEnemyEntity != action.targetPlayer) {
    return false;
  }

  const auto count = observation.playerCount > kMaxObservedPlayers ? kMaxObservedPlayers : observation.playerCount;

  for (size_t i = 0; i < count; ++i) {
    const auto &player = observation.players[i];

    if (player.valid && player.alive && player.enemy && player.entityIndex == action.targetPlayer) {
      return true;
    }
  }

  return false;
}
} // namespace

BotActionExecutor::BotActionExecutor(ActionExecutionContext &context) : m_context(&context) {
}

bool BotActionExecutor::isActionStillOwned(const Action &action) const {
  if (m_context == nullptr) return false;
  if (action.type != ActionType::MoveToNode && action.type != ActionType::MoveToPosition
      && action.type != ActionType::HuntTarget && action.type != ActionType::SeekCover
      && action.type != ActionType::EscapeFromBomb) {
    return true;
  }
  return m_context->allowsNavigationOverride();
}

bool BotActionExecutor::suppressesLegacyTaskExecution() const {
  return m_directAttackTargetActive;
}

void BotActionExecutor::cancel() {
  if (m_directAttackTargetActive && m_context != nullptr) {
    m_context->cancelAttackTarget(m_directAttackAction.targetPlayer);
  }
  if (m_directFollowPlayerActive && m_context != nullptr) m_context->cancelFollowPlayer(m_directFollowPlayerAction.targetPlayer);
  if (m_directThrowGrenadeActive && m_context != nullptr) m_context->cancelThrowGrenade();
  if (m_directHuntTargetActive && m_context != nullptr) {
    m_context->cancelHuntTarget(m_directHuntAction.targetPlayer);
  }
  if (m_directSeekCoverActive && m_context != nullptr) {
    m_context->cancelSeekCover();
  }
  if (m_directEscapeFromBombActive && m_context != nullptr) {
    m_context->cancelEscapeFromBomb();
  }
  if (m_directPlantBombActive && m_context != nullptr) {
    m_context->cancelPlantBomb();
  }
  if (m_directDefuseBombActive && m_context != nullptr) {
    m_context->cancelDefuseBomb();
  }
  if (m_directPickupItemActive && m_context != nullptr) {
    m_context->cancelPickupItem();
  }
  if (m_directFireBreakableActive && m_context != nullptr) {
    m_context->cancelFireBreakable();
  }
  if (m_directCampActive && m_context != nullptr) {
    m_context->cancelCamp();
  }
  if (m_directWaitActive && m_context != nullptr) {
    m_context->cancelWait();
  }
  if (m_directHoldPositionActive && m_context != nullptr) {
    m_context->cancelHoldPosition();
  }
  if (m_directHideActive && m_context != nullptr) {
    m_context->cancelHide();
  }
  m_directAttackTargetActive = false;
  m_directAttackAction = {};
  m_directFollowPlayerActive = false;
  m_directFollowPlayerAction = {};
  m_directThrowGrenadeActive = false;
  m_directThrowGrenadeAction = {};
  m_directHuntTargetActive = false;
  m_directHuntAction = {};
  m_directSeekCoverActive = false;
  m_directEscapeFromBombActive = false;
  m_directPlantBombActive = false;
  m_directDefuseBombActive = false;
  m_directPickupItemActive = false;
  m_directFireBreakableActive = false;
  m_directCampActive = false;
  m_directWaitActive = false;
  m_directHoldPositionActive = false;
  m_directHideActive = false;
  m_observedTaskActive = false;
  m_observedTaskAction = {};
}

ActionResult BotActionExecutor::execute(const Action &action, const Observation &observation) {
  if (m_context == nullptr || !m_context->isAlive() || !observation.bot.alive) {
    cancel();
    return { action.type, ActionResultType::Rejected, 0.0f };
  }

  if (!isActionStillOwned(action)) {
    cancel();
    return { action.type, ActionResultType::Interrupted, 0.0f };
  }

  switch (action.type) {
  case ActionType::MoveToNode:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeMoveToNode(action);

  case ActionType::MoveToPosition:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeMoveToPosition(action);

  case ActionType::AttackTarget:
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeAttackTarget(action, observation);

  case ActionType::FollowPlayer:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeFollowPlayer(action, observation);

  case ActionType::ThrowGrenade:
    return executeThrowGrenade(action, observation);

  case ActionType::HuntTarget:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directSeekCoverActive = false;
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeHuntTarget(action, observation);

  case ActionType::SeekCover:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directEscapeFromBombActive = false;
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeSeekCover(action);

  case ActionType::EscapeFromBomb:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeEscapeFromBomb(action, observation);

  case ActionType::Camp:
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeCamp(action);

  case ActionType::Wait:
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeWait(action);

  case ActionType::HoldPosition:
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeHoldPosition(action);

  case ActionType::Hide:
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeHide(action);

  case ActionType::PlantBomb:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_directEscapeFromBombActive = false;
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executePlantBomb(action, observation);

  case ActionType::DefuseBomb:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_directEscapeFromBombActive = false;
    m_directPlantBombActive = false;
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeDefuseBomb(action, observation);

  case ActionType::PickupItem:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_directEscapeFromBombActive = false;
    m_directPlantBombActive = false;
    m_directDefuseBombActive = false;
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executePickupItem(action);

  case ActionType::Fire:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_directEscapeFromBombActive = false;
    m_directPlantBombActive = false;
    m_directDefuseBombActive = false;
    m_directPickupItemActive = false;
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return executeFireBreakable(action);

  default:
    m_observedTaskActive = false;
    m_observedTaskAction = {};
    return { action.type, ActionResultType::Rejected, 0.0f };
  }
}

ActionResult BotActionExecutor::executeAttackTarget(const Action &action, const Observation &observation) {
  if (!hasObservedEnemyTarget(action, observation)) {
    if (!m_directAttackTargetActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  if (!m_context->attackTarget(action.targetPlayer)) {
    if (!m_directAttackTargetActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  m_directAttackAction = action;
  m_directAttackTargetActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeFollowPlayer(const Action &action, const Observation &observation) {
  if (action.targetType != TargetType::Player || action.targetPlayer != observation.bot.followTargetPlayer) {
    if (!m_directFollowPlayerActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->followPlayer(action.targetPlayer)) {
    if (!m_directFollowPlayerActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directFollowPlayerAction = action;
  m_directFollowPlayerActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeThrowGrenade(const Action &action, const Observation &observation) {
  if (action.grenadeType != GrenadeType::HE || action.targetType != TargetType::Position || !isFinitePosition(action.targetPosition)) return { action.type, ActionResultType::Invalid, 0.0f };
  if (m_directThrowGrenadeActive && observation.bot.currentTask != TaskType::ThrowExplosive) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  if (!m_context->throwGrenade(action.targetPosition)) {
    if (!m_directThrowGrenadeActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  m_directThrowGrenadeAction = action;
  m_directThrowGrenadeActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeHuntTarget(const Action &action, const Observation &observation) {
  if (!hasObservedLastEnemyTarget(action, observation)) {
    if (!m_directHuntTargetActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (m_context->isHuntTargetReached(action.targetPlayer)) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->huntTarget(action.targetPlayer)) {
    if (!m_directHuntTargetActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directHuntAction = action;
  m_directHuntTargetActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeSeekCover(const Action &action) {
  if (m_context->isSeekCoverReached()) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->seekCover()) {
    if (!m_directSeekCoverActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directSeekCoverActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeEscapeFromBomb(const Action &action, const Observation &observation) {
  const bool bombPlanted = observation.bot.objectiveFlags & ObjectiveFlag::BombPlanted;

  if (!bombPlanted) {
    if (!m_directEscapeFromBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (m_context->isEscapeFromBombReached()) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->escapeFromBomb()) {
    if (!m_directEscapeFromBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directEscapeFromBombActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executePlantBomb(const Action &action, const Observation &observation) {
  const uint32_t flags = observation.bot.objectiveFlags;
  const bool bombPlanted = flags & ObjectiveFlag::BombPlanted;
  const bool bombCarrier = flags & ObjectiveFlag::BombCarrier;
  const bool inBombZone = flags & ObjectiveFlag::InBombZone;

  if (bombPlanted || !bombCarrier || !inBombZone) {
    if (!m_directPlantBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->plantBomb()) {
    if (!m_directPlantBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directPlantBombActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeDefuseBomb(const Action &action, const Observation &observation) {
  const bool bombPlanted = observation.bot.objectiveFlags & ObjectiveFlag::BombPlanted;

  if (!bombPlanted) {
    if (!m_directDefuseBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->defuseBomb()) {
    if (!m_directDefuseBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directDefuseBombActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executePickupItem(const Action &action) {
  if (!m_context->pickupItem()) {
    if (!m_directPickupItemActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directPickupItemActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeFireBreakable(const Action &action) {
  if (!m_context->fireBreakable()) {
    if (!m_directFireBreakableActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directFireBreakableActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeCamp(const Action &action) {
  if (!m_context->camp()) {
    if (!m_directCampActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  m_directCampActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeWait(const Action &action) {
  if (!m_context->wait()) {
    if (!m_directWaitActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directWaitActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeHoldPosition(const Action &action) {
  if (!m_context->holdPosition()) {
    if (!m_directHoldPositionActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directHoldPositionActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeHide(const Action &action) {
  if (!m_context->hide()) {
    if (!m_directHideActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directHideActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

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
