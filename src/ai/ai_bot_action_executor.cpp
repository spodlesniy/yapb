//
// AiPB - AI action executor.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <cmath>

#include <ai/ai_action_execution_context.h>
#include <ai/ai_bot_action_executor.h>

namespace ai {
namespace {

bool isFinitePosition(const Vec3 &position) {
  return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

bool hasObservedVisibleEnemy(const Observation &observation) {
  if (observation.combat.enemyEntity < 0
      || !(observation.combat.perceptionFlags & static_cast<uint32_t>(PerceptionFlag::SeeingEnemy))) {
    return false;
  }

  const auto count = observation.playerCount > kMaxObservedPlayers ? kMaxObservedPlayers : observation.playerCount;
  for (size_t i = 0; i < count; ++i) {
    const auto &player = observation.players[i];
    if (player.valid && player.alive && player.enemy && player.visible
        && player.entityIndex == observation.combat.enemyEntity) {
      return true;
    }
  }
  return false;
}

bool hasObservedEnemyTarget(const Action &action, const Observation &observation) {
  return action.targetType == TargetType::Player
      && observation.combat.enemyEntity == action.targetPlayer
      && hasObservedVisibleEnemy(observation);
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

bool bombObjectivePreemptsHunt(const Observation &observation) {
  if (observation.combat.perceptionFlags & static_cast<uint32_t>(PerceptionFlag::SeeingEnemy)) {
    return false;
  }

  if (observation.bot.objectiveFlags & ObjectiveFlag::BombPlanted) {
    return true;
  }

  return observation.bot.team == 0 && (observation.bot.objectiveFlags & ObjectiveFlag::BombDropped);
}
} // namespace

BotActionExecutor::BotActionExecutor(ActionExecutionContext &context) : m_context(&context) {
}

bool BotActionExecutor::isActionStillOwned(const Action &action) const {
  if (m_context == nullptr) return false;

  switch (action.type) {
  case ActionType::MoveToNode:
  case ActionType::MoveToPosition:
  case ActionType::Explore:
    return m_context->allowsNavigationOverride();

  default:
    return true;
  }
}

bool BotActionExecutor::suppressesLegacyTaskExecution() const {
  return m_directAttackTargetActive || m_directAimTargetActive;
}

void BotActionExecutor::cancel() {
  if (m_directAttackTargetActive && m_context != nullptr) {
    m_context->cancelAttackTarget(m_directAttackAction.targetPlayer);
  }
  if (m_directAimTargetActive && m_context != nullptr) {
    m_context->cancelAimAtTarget(m_directAimAction.targetPlayer);
  }
  if (m_directFollowPlayerActive && m_context != nullptr) m_context->cancelFollowPlayer(m_directFollowPlayerAction.targetPlayer);
  if (m_directChangeWeaponActive && m_context != nullptr) m_context->cancelChangeWeapon();
  if (m_directThrowGrenadeActive && m_context != nullptr) m_context->cancelThrowGrenade();
  if (m_directThrowFlashbangActive && m_context != nullptr) m_context->cancelThrowFlashbang();
  if (m_directThrowSmokeActive && m_context != nullptr) m_context->cancelThrowSmoke();
  if (m_directHuntTargetActive && m_context != nullptr) {
    m_context->cancelHuntTarget(m_directHuntAction.targetPlayer);
  }
  if (m_directSeekCoverActive && m_context != nullptr) {
    m_context->cancelSeekCover();
  }
  if (m_directRetreatActive && m_context != nullptr) {
    m_context->cancelRetreat();
  }
  if (m_directExploreActive && m_context != nullptr) {
    m_context->cancelExplore();
  }
  if (m_directProtectObjectiveActive && m_context != nullptr) {
    m_context->cancelProtectObjective();
  }
  if (m_directReloadActive && m_context != nullptr) {
    m_context->cancelReload();
  }
  if (m_directEscapeFromBombActive && m_context != nullptr) {
    m_context->cancelEscapeFromBomb();
  }
  if (m_directRescueHostageActive && m_context != nullptr) {
    m_context->cancelRescueHostage();
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
  m_directAimTargetActive = false;
  m_directAimAction = {};
  m_directFollowPlayerActive = false;
  m_directChangeWeaponActive = false;
  m_directChangeWeaponAction = {};
  m_directThrowGrenadeActive = false;
  m_directThrowGrenadeAction = {};
  m_directThrowFlashbangActive = false;
  m_directThrowFlashbangAction = {};
  m_directThrowSmokeActive = false;
  m_directThrowSmokeAction = {};
  m_directHuntTargetActive = false;
  m_directHuntAction = {};
  m_directSeekCoverActive = false;
  m_directRetreatActive = false;
  m_directExploreActive = false;
  m_directProtectObjectiveActive = false;
  m_directReloadActive = false;
  m_directEscapeFromBombActive = false;
  m_directRescueHostageActive = false;
  m_directPlantBombActive = false;
  m_directDefuseBombActive = false;
  m_directPickupItemActive = false;
  m_directFireBreakableActive = false;
  m_directCampActive = false;
  m_directWaitActive = false;
  m_directHoldPositionActive = false;
  m_directHideActive = false;
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
    return executeMoveToNode(action);

  case ActionType::MoveToPosition:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    return executeMoveToPosition(action);

  case ActionType::AttackTarget:
    if (m_directAimTargetActive && m_context != nullptr) {
      m_context->cancelAimAtTarget(m_directAimAction.targetPlayer);
      m_directAimTargetActive = false;
      m_directAimAction = {};
    }
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    return executeAttackTarget(action, observation);

  case ActionType::AimAtTarget:
    if (m_directAttackTargetActive && m_context != nullptr) {
      m_context->cancelAttackTarget(m_directAttackAction.targetPlayer);
      m_directAttackTargetActive = false;
      m_directAttackAction = {};
    }
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    return executeAimAtTarget(action, observation);

  case ActionType::FollowPlayer:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    return executeFollowPlayer(action, observation);

  case ActionType::ChangeWeapon:
    return executeChangeWeapon(action, observation);

  case ActionType::ThrowGrenade:
    if (m_directThrowFlashbangActive && m_context != nullptr) {
      m_context->cancelThrowFlashbang();
      m_directThrowFlashbangActive = false;
      m_directThrowFlashbangAction = {};
    }
    if (m_directThrowSmokeActive && m_context != nullptr) {
      m_context->cancelThrowSmoke();
      m_directThrowSmokeActive = false;
      m_directThrowSmokeAction = {};
    }
    return executeThrowGrenade(action, observation);

  case ActionType::ThrowFlashbang:
    if (m_directThrowGrenadeActive && m_context != nullptr) {
      m_context->cancelThrowGrenade();
      m_directThrowGrenadeActive = false;
      m_directThrowGrenadeAction = {};
    }
    if (m_directThrowSmokeActive && m_context != nullptr) {
      m_context->cancelThrowSmoke();
      m_directThrowSmokeActive = false;
      m_directThrowSmokeAction = {};
    }
    return executeThrowFlashbang(action, observation);

  case ActionType::ThrowSmoke:
    if (m_directThrowGrenadeActive && m_context != nullptr) {
      m_context->cancelThrowGrenade();
      m_directThrowGrenadeActive = false;
      m_directThrowGrenadeAction = {};
    }
    if (m_directThrowFlashbangActive && m_context != nullptr) {
      m_context->cancelThrowFlashbang();
      m_directThrowFlashbangActive = false;
      m_directThrowFlashbangAction = {};
    }
    return executeThrowSmoke(action, observation);

  case ActionType::HuntTarget:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directSeekCoverActive = false;
    return executeHuntTarget(action, observation);

  case ActionType::SeekCover:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directRetreatActive = false;
    m_directEscapeFromBombActive = false;
    return executeSeekCover(action);

  case ActionType::Retreat:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directExploreActive = false;
    if (m_directSeekCoverActive && m_context != nullptr) {
      m_context->cancelSeekCover();
      m_directSeekCoverActive = false;
    }
    if (m_directEscapeFromBombActive && m_context != nullptr) {
      m_context->cancelEscapeFromBomb();
      m_directEscapeFromBombActive = false;
    }
    return executeRetreat(action);

  case ActionType::Explore:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    if (m_directSeekCoverActive && m_context != nullptr) {
      m_context->cancelSeekCover();
      m_directSeekCoverActive = false;
    }
    if (m_directRetreatActive && m_context != nullptr) {
      m_context->cancelRetreat();
      m_directRetreatActive = false;
    }
    if (m_directEscapeFromBombActive && m_context != nullptr) {
      m_context->cancelEscapeFromBomb();
      m_directEscapeFromBombActive = false;
    }
    return executeExplore(action);

  case ActionType::ProtectObjective:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    if (m_directSeekCoverActive && m_context != nullptr) {
      m_context->cancelSeekCover();
      m_directSeekCoverActive = false;
    }
    if (m_directRetreatActive && m_context != nullptr) {
      m_context->cancelRetreat();
      m_directRetreatActive = false;
    }
    if (m_directExploreActive && m_context != nullptr) {
      m_context->cancelExplore();
      m_directExploreActive = false;
    }
    if (m_directEscapeFromBombActive && m_context != nullptr) {
      m_context->cancelEscapeFromBomb();
      m_directEscapeFromBombActive = false;
    }
    return executeProtectObjective(action);

  case ActionType::Reload:
    if (m_directProtectObjectiveActive && m_context != nullptr) {
      m_context->cancelProtectObjective();
      m_directProtectObjectiveActive = false;
    }
    return executeReload(action);

  case ActionType::EscapeFromBomb:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    return executeEscapeFromBomb(action, observation);

  case ActionType::Camp:
    return executeCamp(action);

  case ActionType::Wait:
    return executeWait(action);

  case ActionType::HoldPosition:
    return executeHoldPosition(action);

  case ActionType::Hide:
    return executeHide(action);

  case ActionType::PlantBomb:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_directEscapeFromBombActive = false;
    return executePlantBomb(action, observation);

  case ActionType::RescueHostage:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_directEscapeFromBombActive = false;
    return executeRescueHostage(action, observation);

  case ActionType::DefuseBomb:
    m_directAttackTargetActive = false;
    m_directAttackAction = {};
    m_directHuntTargetActive = false;
    m_directHuntAction = {};
    m_directSeekCoverActive = false;
    m_directEscapeFromBombActive = false;
    m_directPlantBombActive = false;
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
    return executeFireBreakable(action);

  default:
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

ActionResult BotActionExecutor::executeAimAtTarget(const Action &action, const Observation &observation) {
  if (!hasObservedEnemyTarget(action, observation)) {
    if (!m_directAimTargetActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->aimAtTarget(action.targetPlayer)) {
    if (!m_directAimTargetActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directAimAction = action;
  m_directAimTargetActive = true;
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

ActionResult BotActionExecutor::executeChangeWeapon(const Action &action, const Observation &observation) {
  if (action.weaponType == WeaponType::Unknown || action.weaponType == WeaponType::None) return { action.type, ActionResultType::Invalid, 0.0f };
  if (observation.combat.weaponType == action.weaponType) {
    if (m_directChangeWeaponActive) cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  if (m_directChangeWeaponActive && m_directChangeWeaponAction.weaponType == action.weaponType) return { action.type, ActionResultType::Accepted, 0.0f };
  if (!m_context->changeWeapon(action.weaponType)) {
    if (m_directChangeWeaponActive) {
      cancel();
      return { action.type, ActionResultType::Failed, 0.0f };
    }
    return { action.type, ActionResultType::Rejected, 0.0f };
  }
  if (m_directChangeWeaponActive) m_context->cancelChangeWeapon();
  m_directChangeWeaponAction = action;
  m_directChangeWeaponActive = true;
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

ActionResult BotActionExecutor::executeThrowFlashbang(const Action &action, const Observation &observation) {
  if (action.grenadeType != GrenadeType::Flashbang || action.targetType != TargetType::Position || !isFinitePosition(action.targetPosition)) {
    return { action.type, ActionResultType::Invalid, 0.0f };
  }
  if (m_directThrowFlashbangActive && observation.bot.currentTask != TaskType::ThrowFlashbang) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  if (!m_context->throwFlashbang(action.targetPosition)) {
    if (!m_directThrowFlashbangActive) return { action.type, ActionResultType::Rejected, 0.0f };
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }
  m_directThrowFlashbangAction = action;
  m_directThrowFlashbangActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeThrowSmoke(const Action &action, const Observation &observation) {
  if (action.grenadeType != GrenadeType::Smoke || action.targetType != TargetType::Position || !isFinitePosition(action.targetPosition)) {
    return { action.type, ActionResultType::Invalid, 0.0f };
  }

  if (m_directThrowSmokeActive && observation.bot.currentTask != TaskType::ThrowSmoke) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->throwSmoke(action.targetPosition)) {
    if (!m_directThrowSmokeActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Failed, 0.0f };
  }

  m_directThrowSmokeAction = action;
  m_directThrowSmokeActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeHuntTarget(const Action &action, const Observation &observation) {
  if (bombObjectivePreemptsHunt(observation)) {
    if (!m_directHuntTargetActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Interrupted, 0.0f };
  }

  if (!hasObservedLastEnemyTarget(action, observation)) {
    if (!m_directHuntTargetActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (m_context->isHuntTargetReached(action.targetPlayer)) {
    m_context->consumeHuntTargetMemory(action.targetPlayer);
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  const Vec3 rememberedPosition {
    observation.bot.origin.x + observation.combat.lastEnemyRelativeOrigin.x,
    observation.bot.origin.y + observation.combat.lastEnemyRelativeOrigin.y,
    observation.bot.origin.z + observation.combat.lastEnemyRelativeOrigin.z,
  };

  if (!m_context->huntTarget(action.targetPlayer, rememberedPosition)) {
    if (!m_directHuntTargetActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (m_context->isHuntTargetStalled(action.targetPlayer)) {
    m_context->consumeHuntTargetMemory(action.targetPlayer);
    cancel();
    return { action.type, ActionResultType::Interrupted, 0.0f };
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

ActionResult BotActionExecutor::executeRetreat(const Action &action) {
  if (m_context->isRetreatReached()) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->retreat()) {
    if (!m_directRetreatActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Interrupted, 0.0f };
  }

  m_directRetreatActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeExplore(const Action &action) {
  if (m_context->isExploreReached()) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->explore()) {
    if (!m_directExploreActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  m_directExploreActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeProtectObjective(const Action &action) {
  if (m_context->isProtectObjectiveReached()) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->protectObjective()) {
    if (!m_directProtectObjectiveActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Interrupted, 0.0f };
  }

  m_directProtectObjectiveActive = true;
  return { action.type, ActionResultType::Accepted, 0.0f };
}

ActionResult BotActionExecutor::executeReload(const Action &action) {
  if (m_context->isReloadCompleted()) {
    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->reload(action.weaponType)) {
    if (!m_directReloadActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Failed, 0.0f };
  }

  m_directReloadActive = true;
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

ActionResult BotActionExecutor::executeRescueHostage(const Action &action, const Observation &observation) {
  const bool hasHostage = observation.bot.hasHostage;
  if (!hasHostage) {
    if (!m_directRescueHostageActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Completed, 0.0f };
  }

  if (!m_context->rescueHostage()) {
    if (!m_directRescueHostageActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Failed, 0.0f };
  }

  m_directRescueHostageActive = true;
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
    return { action.type, bombPlanted ? ActionResultType::Completed : ActionResultType::Interrupted, 0.0f };
  }

  if (hasObservedVisibleEnemy(observation)) {
    if (!m_directPlantBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Interrupted, 0.0f };
  }

  if (!m_context->plantBomb()) {
    if (!m_directPlantBombActive) {
      return { action.type, ActionResultType::Rejected, 0.0f };
    }

    cancel();
    return { action.type, ActionResultType::Interrupted, 0.0f };
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
