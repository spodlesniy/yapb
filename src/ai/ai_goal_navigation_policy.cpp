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

} // namespace

Action GoalNavigationPolicy::decide(const Observation &observation) const {
  if (!observation.bot.alive) {
    return {};
  }

  if (observation.combat.reloadState != ReloadState::None) {
    Action action {};
    action.type = ActionType::Reload;
    action.weaponType = observation.combat.reloadState == ReloadState::Secondary ? WeaponType::Pistol : WeaponType::Unknown;
    action.confidence = 1.0f;
    return action;
  }

  if (observation.bot.team == 0 && observation.bot.currentTask == TaskType::Camp
      && (observation.bot.objectiveFlags & ObjectiveFlag::BombPlanted)) {
    Action action {};
    action.type = ActionType::ProtectObjective;
    action.confidence = 1.0f;
    return action;
  }

  const bool canStartHostageRescue = observation.bot.currentTask == TaskType::Normal
                                    || observation.bot.currentTask == TaskType::MoveToPosition;

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
    if (observation.bot.followTargetPlayer < 0 || !hasObservedPlayer(observation, observation.bot.followTargetPlayer)) return makeGoalNavigationAction(observation);
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
    case TaskType::SeekCover:
      action.type = ActionType::SeekCover;
      break;
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
    const Action action = makeTargetPlayerAction(ActionType::AttackTarget, observation);
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
    action.type = observation.bot.taskTimeRemaining >= kHoldPositionMinimumTaskTime
                    ? ActionType::HoldPosition
                    : ActionType::Wait;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Hide: {
    Action action {};
    action.type = ActionType::Hide;
    action.confidence = 1.0f;
    return action;
  }

  case TaskType::Unknown:
  case TaskType::Normal:
  case TaskType::DoubleJump:
  case TaskType::Blind:
  case TaskType::Spraypaint:
    break;
  }

  return makeGoalNavigationAction(observation);
}

} // namespace ai
