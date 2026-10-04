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

  switch (observation.bot.currentTask) {
  case TaskType::MoveToPosition: {
    Action action {};
    action.type = ActionType::MoveToPosition;
    action.targetType = TargetType::Position;
    action.targetPosition = observation.bot.destination;
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
    Action action {};
    action.type = ActionType::HoldPosition;
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
  case TaskType::FollowUser:
  case TaskType::DoubleJump:
  case TaskType::ThrowExplosive:
  case TaskType::ThrowFlashbang:
  case TaskType::ThrowSmoke:
  case TaskType::Blind:
  case TaskType::Spraypaint:
    break;
  }

  return makeGoalNavigationAction(observation);
}

} // namespace ai
