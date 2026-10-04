//
// AiPB - AI model action encoder implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_inference_action_encoder.h>

namespace ai {
namespace {

bool encodeActionId(ActionType type, uint8_t &actionId) {
  switch (type) {
  case ActionType::None:
    actionId = static_cast<uint8_t>(InferenceActionId::None);
    return true;
  case ActionType::MoveToNode:
    actionId = static_cast<uint8_t>(InferenceActionId::MoveToNode);
    return true;
  case ActionType::MoveToPosition:
    actionId = static_cast<uint8_t>(InferenceActionId::MoveToPosition);
    return true;
  case ActionType::FollowPlayer:
    actionId = static_cast<uint8_t>(InferenceActionId::FollowPlayer);
    return true;
  case ActionType::SeekCover:
    actionId = static_cast<uint8_t>(InferenceActionId::SeekCover);
    return true;
  case ActionType::Retreat:
    actionId = static_cast<uint8_t>(InferenceActionId::Retreat);
    return true;
  case ActionType::HoldPosition:
    actionId = static_cast<uint8_t>(InferenceActionId::HoldPosition);
    return true;
  case ActionType::Explore:
    actionId = static_cast<uint8_t>(InferenceActionId::Explore);
    return true;
  case ActionType::AttackTarget:
    actionId = static_cast<uint8_t>(InferenceActionId::AttackTarget);
    return true;
  case ActionType::HuntTarget:
    actionId = static_cast<uint8_t>(InferenceActionId::HuntTarget);
    return true;
  case ActionType::AimAtTarget:
    actionId = static_cast<uint8_t>(InferenceActionId::AimAtTarget);
    return true;
  case ActionType::Fire:
    actionId = static_cast<uint8_t>(InferenceActionId::Fire);
    return true;
  case ActionType::Reload:
    actionId = static_cast<uint8_t>(InferenceActionId::Reload);
    return true;
  case ActionType::ChangeWeapon:
    actionId = static_cast<uint8_t>(InferenceActionId::ChangeWeapon);
    return true;
  case ActionType::PlantBomb:
    actionId = static_cast<uint8_t>(InferenceActionId::PlantBomb);
    return true;
  case ActionType::DefuseBomb:
    actionId = static_cast<uint8_t>(InferenceActionId::DefuseBomb);
    return true;
  case ActionType::PickupItem:
    actionId = static_cast<uint8_t>(InferenceActionId::PickupItem);
    return true;
  case ActionType::EscapeFromBomb:
    actionId = static_cast<uint8_t>(InferenceActionId::EscapeFromBomb);
    return true;
  case ActionType::RescueHostage:
    actionId = static_cast<uint8_t>(InferenceActionId::RescueHostage);
    return true;
  case ActionType::ProtectObjective:
    actionId = static_cast<uint8_t>(InferenceActionId::ProtectObjective);
    return true;
  case ActionType::Wait:
    actionId = static_cast<uint8_t>(InferenceActionId::Wait);
    return true;
  case ActionType::Camp:
    actionId = static_cast<uint8_t>(InferenceActionId::Camp);
    return true;
  case ActionType::ThrowGrenade:
    actionId = static_cast<uint8_t>(InferenceActionId::ThrowGrenade);
    return true;
  case ActionType::ThrowFlashbang:
    actionId = static_cast<uint8_t>(InferenceActionId::ThrowFlashbang);
    return true;
  case ActionType::ThrowSmoke:
    actionId = static_cast<uint8_t>(InferenceActionId::ThrowSmoke);
    return true;
  case ActionType::Hide:
    actionId = static_cast<uint8_t>(InferenceActionId::Hide);
    return true;
  case ActionType::Count:
    break;
  }
  return false;
}

} // namespace

InferenceActionEncodeResult encodeInferenceAction(const Action &action) {
  InferenceActionEncodeResult result {};

  if (!encodeActionId(action.type, result.output.actionId)) {
    result.error = InferenceActionEncodeError::UnsupportedAction;
    return result;
  }

  result.output.targetNode = action.targetNode;
  result.output.targetPlayer = action.targetPlayer;
  result.output.targetPosition = action.targetPosition;
  result.output.weaponType = static_cast<uint8_t>(action.weaponType);
  result.output.grenadeType = static_cast<uint8_t>(action.grenadeType);
  result.output.duration = action.duration;
  result.output.confidence = action.confidence;

  return result;
}

} // namespace ai
