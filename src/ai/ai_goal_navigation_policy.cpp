//
// AiPB - deterministic task-aware teacher policy.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_goal_navigation_policy.h>

namespace ai {
namespace {

bool hasObservedPlayer(const Observation &observation, int32_t entityIndex) {
  const auto count = observation.playerCount > kMaxObservedPlayers ? kMaxObservedPlayers : observation.playerCount;

  for (size_t i = 0; i < count; ++i) {
    const auto &player = observation.players[i];
    if (player.valid && player.entityIndex == entityIndex) {
      return true;
    }
  }

  return false;
}

bool isReloadableWeapon(WeaponType weaponType) {
  switch (weaponType) {
  case WeaponType::Pistol:
  case WeaponType::Shotgun:
  case WeaponType::ZoomRifle:
  case WeaponType::Rifle:
  case WeaponType::SMG:
  case WeaponType::Sniper:
  case WeaponType::Heavy:
    return true;
  case WeaponType::Unknown:
  case WeaponType::None:
  case WeaponType::Melee:
    return false;
  }
  return false;
}

bool isReloadCompatible(ReloadState reloadState, WeaponType weaponType) {
  if (!isReloadableWeapon(weaponType)) {
    return false;
  }

  switch (reloadState) {
  case ReloadState::Primary:
    return weaponType != WeaponType::Pistol;
  case ReloadState::Secondary:
    return weaponType == WeaponType::Pistol;
  case ReloadState::None:
    return false;
  }
  return false;
}

Action makeTargetPlayerAction(ActionType type, const Observation &observation) {
  if (observation.combat.enemyEntity < 0 || !hasObservedPlayer(observation, observation.combat.enemyEntity)) {
    return {};
  }

  Action action {};
  action.type = type;
  action.targetType = TargetType::Player;
  action.targetPlayer = observation.combat.enemyEntity;
  action.confidence = 1.0f;
  return action;
}

Action makeGoalNavigationAction(const Observation &observation) {
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

constexpr float kBombPlantTravelTimeScale = 1.5f;
constexpr float kBombPlantReserveTime = 10.0f;
constexpr int kCounterTerroristTeam = 1;

bool shouldPrioritizeBombPlant(const Observation &observation) {
  if (!observation.bot.hasC4 || observation.bot.inBombZone || observation.bot.currentTask == TaskType::PlantBomb
      || observation.bot.currentGoalNode < 0 || observation.bot.maxSpeed <= 0.0f || observation.roundTimeRemaining <= 0.0f) {
    return false;
  }

  const auto count = observation.waypointCount > kMaxObservedWaypoints ? kMaxObservedWaypoints : observation.waypointCount;

  for (size_t i = 0; i < count; ++i) {
    const auto &waypoint = observation.waypoints[i];

    if (waypoint.index != observation.bot.currentGoalNode) {
      continue;
    }

    const float travelTime = waypoint.distance / observation.bot.maxSpeed;
    return observation.roundTimeRemaining <= travelTime * kBombPlantTravelTimeScale + kBombPlantReserveTime;
  }

  return false;
}

} // namespace

Action GoalNavigationPolicy::decide(const Observation &observation) const {
  if (!observation.bot.alive) {
    return {};
  }

  if (shouldPrioritizeBombPlant(observation)) {
    if (observation.bot.currentTask == TaskType::Normal) {
      return {};
    }

    const auto objectiveAction = makeGoalNavigationAction(observation);
    if (objectiveAction.type != ActionType::None) {
      return objectiveAction;
    }
  }

  if (observation.combat.reloadState != ReloadState::None &&
      isReloadCompatible(observation.combat.reloadState, observation.combat.weaponType)) {
    Action action {};
    action.type = ActionType::Reload;
    action.weaponType = observation.combat.reloadState == ReloadState::Secondary ? WeaponType::Pistol : WeaponType::Unknown;
    action.confidence = 1.0f;
    return action;
  }

  if (observation.bot.team == 0 && observation.bot.currentTask == TaskType::Camp &&
      (observation.bot.objectiveFlags & ObjectiveFlag::BombPlanted)) {
    Action action {};
    action.type = ActionType::ProtectObjective;
    action.confidence = 1.0f;
    return action;
  }

  const bool canStartHostageRescue =
      observation.bot.currentTask == TaskType::Normal || observation.bot.currentTask == TaskType::MoveToPosition;

  if (canStartHostageRescue && observation.bot.hasHostage && !observation.bot.inRescueZone) {
    Action action {};
    action.type = ActionType::RescueHostage;
    action.confidence = 1.0f;
    return action;
  }

  switch (observation.bot.currentTask) {
  case TaskType::MoveToPosition: {
    Action action {};
    action.type = ActionType::MoveToPosition;
    action.targetType = TargetType::Position;
    action.targetPosition = observation.bot.destination;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::FollowUser: {
    if (observation.bot.followTargetPlayer < 0 || !hasObservedPlayer(observation, observation.bot.followTargetPlayer))
      return makeGoalNavigationAction(observation);
    Action action {};
    action.type = ActionType::FollowPlayer;
    action.targetType = TargetType::Player;
    action.targetPlayer = observation.bot.followTargetPlayer;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::ThrowExplosive: {
    Action action {};
    action.type = ActionType::ThrowGrenade;
    action.targetType = TargetType::Position;
    action.targetPosition = observation.bot.throwTarget;
    action.grenadeType = GrenadeType::HE;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::ThrowFlashbang: {
    Action action {};
    action.type = ActionType::ThrowFlashbang;
    action.targetType = TargetType::Position;
    action.targetPosition = observation.bot.throwTarget;
    action.grenadeType = GrenadeType::Flashbang;
    action.confidence = 1.0f;
    return action;
  }
  case TaskType::ThrowSmoke: {
    Action action {};
    action.type = ActionType::ThrowSmoke;
    action.targetType = TargetType::Position;
    action.targetPosition = observation.bot.throwTarget;
    action.grenadeType = GrenadeType::Smoke;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::PickupItem: {
    Action action {};
    action.type = ActionType::PickupItem;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Camp: {
    Action action {};
    action.type = ActionType::Camp;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::PlantBomb:
  case TaskType::DefuseBomb:
  case TaskType::SeekCover:
  case TaskType::EscapeFromBomb: {
    Action action {};
    switch (observation.bot.currentTask) {
    case TaskType::PlantBomb:
      action.type = ActionType::PlantBomb;
      break;
    case TaskType::DefuseBomb:
      action.type = ActionType::DefuseBomb;
      break;
    case TaskType::SeekCover: {
      constexpr float kRetreatApproachThreshold = 30.0f;
      const float approach = observation.bot.health * observation.personality.aggression;
      const bool seeingEnemy = (observation.combat.perceptionFlags & static_cast<uint32_t>(PerceptionFlag::SeeingEnemy)) != 0;

      action.type = seeingEnemy && approach < kRetreatApproachThreshold
                      ? ActionType::Retreat
                      : ActionType::SeekCover;
      break;
    }
    case TaskType::EscapeFromBomb:
      action.type = ActionType::EscapeFromBomb;
      break;
    default:
      return {};
    }
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Attack: {
    const ActionType actionType = observation.combat.firePauseRemaining > 0.0f ? ActionType::AimAtTarget : ActionType::AttackTarget;
    const Action action = makeTargetPlayerAction(actionType, observation);
    return action.type != ActionType::None ? action : makeGoalNavigationAction(observation);
  }

  case TaskType::Hunt: {
    const Action action = makeTargetPlayerAction(ActionType::HuntTarget, observation);
    return action.type != ActionType::None ? action : makeGoalNavigationAction(observation);
  }

  case TaskType::ShootBreakable: {
    Action action {};
    action.type = ActionType::Fire;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Pause: {
    constexpr float kHoldPositionMinimumTaskTime = 10.0f;

    Action action {};
    action.type = observation.bot.taskTimeRemaining >= kHoldPositionMinimumTaskTime ? ActionType::HoldPosition : ActionType::Wait;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Hide: {
    Action action {};
    action.type = ActionType::Hide;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Normal: {
    const bool bombPlanted = (observation.bot.objectiveFlags & ObjectiveFlag::BombPlanted) != 0;
    if (observation.bot.hasC4 || (observation.bot.team == kCounterTerroristTeam && bombPlanted)) {
      return {};
    }

    Action action {};
    action.type = ActionType::Explore;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Unknown:
  case TaskType::DoubleJump:
  case TaskType::Blind:
  case TaskType::Spraypaint:
    break;
  }

  return makeGoalNavigationAction(observation);
}

} // namespace ai
