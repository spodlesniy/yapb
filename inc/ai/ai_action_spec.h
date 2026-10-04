//
// AiPB - AI action semantics specification.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>

#include <cstdint>

namespace ai {

enum class ActionParameter : uint32_t {
  TargetNode = 1u << 0,
  TargetPlayer = 1u << 1,
  TargetPosition = 1u << 2,
  WeaponType = 1u << 3,
  GrenadeType = 1u << 4,
  Duration = 1u << 5,
};

struct ActionSpec {
  TargetType targetType { TargetType::None };
  uint32_t requiredParameters {};
  uint32_t optionalParameters {};
};

constexpr uint32_t actionParameter(ActionParameter parameter) {
  return static_cast<uint32_t>(parameter);
}

constexpr ActionSpec getActionSpec(ActionType type) {
  switch (type) {
  case ActionType::None:
    return {};
  case ActionType::MoveToNode:
    return { TargetType::Node, actionParameter(ActionParameter::TargetNode), 0 };
  case ActionType::MoveToPosition:
    return { TargetType::Position, actionParameter(ActionParameter::TargetPosition), 0 };
  case ActionType::FollowPlayer:
    return { TargetType::Player, actionParameter(ActionParameter::TargetPlayer), 0 };
  case ActionType::SeekCover:
  case ActionType::Retreat:
  case ActionType::Explore:
    return {};
  case ActionType::HoldPosition:
  case ActionType::Wait:
  case ActionType::Camp:
  case ActionType::Hide:
    return { TargetType::None, 0, actionParameter(ActionParameter::Duration) };
  case ActionType::AttackTarget:
  case ActionType::HuntTarget:
  case ActionType::AimAtTarget:
    return { TargetType::Player, actionParameter(ActionParameter::TargetPlayer), 0 };
  case ActionType::Fire:
    return {};
  case ActionType::Reload:
    return { TargetType::None, 0, actionParameter(ActionParameter::WeaponType) };
  case ActionType::ChangeWeapon:
    return { TargetType::None, actionParameter(ActionParameter::WeaponType), 0 };
  case ActionType::PlantBomb:
  case ActionType::DefuseBomb:
  case ActionType::PickupItem:
  case ActionType::EscapeFromBomb:
  case ActionType::RescueHostage:
  case ActionType::ProtectObjective:
    return {};
  case ActionType::ThrowGrenade:
    return { TargetType::Position, actionParameter(ActionParameter::TargetPosition) | actionParameter(ActionParameter::GrenadeType), 0 };
  case ActionType::ThrowFlashbang:
  case ActionType::ThrowSmoke:
    return { TargetType::Position, actionParameter(ActionParameter::TargetPosition), 0 };
  case ActionType::Count:
    break;
  }
  return {};
}

} // namespace ai
