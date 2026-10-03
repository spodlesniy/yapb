//
// AiPB - AI action to observed-task mapping.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>

namespace ai {

constexpr bool actionMatchesObservedTask(const Action &action, const Observation &observation) {
  switch (action.type) {
  case ActionType::Wait:
    return observation.bot.currentTask == TaskType::Pause;

  case ActionType::HoldPosition:
    return observation.bot.currentTask == TaskType::Pause || observation.bot.currentTask == TaskType::Hide;

  case ActionType::Camp:
    return observation.bot.currentTask == TaskType::Camp;

  case ActionType::SeekCover:
    return observation.bot.currentTask == TaskType::SeekCover;

  case ActionType::AttackTarget:
    return observation.bot.currentTask == TaskType::Attack && action.targetPlayer == observation.combat.enemyEntity;

  case ActionType::HuntTarget:
    return observation.bot.currentTask == TaskType::Hunt && action.targetPlayer == observation.combat.enemyEntity;

  case ActionType::PlantBomb:
    return observation.bot.currentTask == TaskType::PlantBomb;

  case ActionType::DefuseBomb:
    return observation.bot.currentTask == TaskType::DefuseBomb;

  case ActionType::PickupItem:
    return observation.bot.currentTask == TaskType::PickupItem;

  case ActionType::EscapeFromBomb:
    return observation.bot.currentTask == TaskType::EscapeFromBomb;

  case ActionType::Fire:
    return observation.bot.currentTask == TaskType::ShootBreakable;

  default:
    return false;
  }
}

constexpr bool sameObservedTaskAction(const Action &left, const Action &right) {
  if (left.type != right.type) {
    return false;
  }

  switch (left.type) {
  case ActionType::AttackTarget:
  case ActionType::HuntTarget:
    return left.targetPlayer == right.targetPlayer;

  default:
    return true;
  }
}

} // namespace ai
