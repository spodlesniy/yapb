//
// AiPB - AI model action decoder implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_inference_action_decoder.h>

namespace ai {
namespace {

bool decodeActionType (uint8_t actionId, ActionType &type) {
   switch (static_cast<InferenceActionId> (actionId)) {
   case InferenceActionId::None: type = ActionType::None; return true;
   case InferenceActionId::MoveToNode: type = ActionType::MoveToNode; return true;
   case InferenceActionId::MoveToPosition: type = ActionType::MoveToPosition; return true;
   case InferenceActionId::FollowPlayer: type = ActionType::FollowPlayer; return true;
   case InferenceActionId::SeekCover: type = ActionType::SeekCover; return true;
   case InferenceActionId::Retreat: type = ActionType::Retreat; return true;
   case InferenceActionId::HoldPosition: type = ActionType::HoldPosition; return true;
   case InferenceActionId::Explore: type = ActionType::Explore; return true;
   case InferenceActionId::AttackTarget: type = ActionType::AttackTarget; return true;
   case InferenceActionId::HuntTarget: type = ActionType::HuntTarget; return true;
   case InferenceActionId::AimAtTarget: type = ActionType::AimAtTarget; return true;
   case InferenceActionId::Fire: type = ActionType::Fire; return true;
   case InferenceActionId::Reload: type = ActionType::Reload; return true;
   case InferenceActionId::ChangeWeapon: type = ActionType::ChangeWeapon; return true;
   case InferenceActionId::PlantBomb: type = ActionType::PlantBomb; return true;
   case InferenceActionId::DefuseBomb: type = ActionType::DefuseBomb; return true;
   case InferenceActionId::PickupItem: type = ActionType::PickupItem; return true;
   case InferenceActionId::EscapeFromBomb: type = ActionType::EscapeFromBomb; return true;
   case InferenceActionId::RescueHostage: type = ActionType::RescueHostage; return true;
   case InferenceActionId::ProtectObjective: type = ActionType::ProtectObjective; return true;
   case InferenceActionId::Wait: type = ActionType::Wait; return true;
   case InferenceActionId::Camp: type = ActionType::Camp; return true;
   case InferenceActionId::ThrowGrenade: type = ActionType::ThrowGrenade; return true;
   case InferenceActionId::ThrowFlashbang: type = ActionType::ThrowFlashbang; return true;
   case InferenceActionId::ThrowSmoke: type = ActionType::ThrowSmoke; return true;
   case InferenceActionId::Count: break;
   }
   return false;
}

} // namespace

InferenceActionDecodeResult decodeInferenceAction (
   const InferenceActionOutput &output,
   const Observation &observation
) {
   InferenceActionDecodeResult result {};

   if (!output.hasSupportedSchema ()) {
      result.error = InferenceActionDecodeError::UnsupportedSchema;
      return result;
   }

   ActionType type {};
   if (!decodeActionType (output.actionId, type)) {
      result.error = InferenceActionDecodeError::InvalidActionId;
      return result;
   }

   result.action.type = type;
   result.action.targetType = getActionSpec (type).targetType;
   result.action.targetNode = output.targetNode;
   result.action.targetPlayer = output.targetPlayer;
   result.action.targetPosition = output.targetPosition;
   result.action.weaponType = static_cast<WeaponType> (output.weaponType);
   result.action.grenadeType = static_cast<GrenadeType> (output.grenadeType);
   result.action.duration = output.duration;
   result.action.confidence = output.confidence;

   const auto validation = ActionValidator::validate (result.action, observation);
   if (!validation.isValid ()) {
      result.error = InferenceActionDecodeError::InvalidAction;
      result.validationError = validation.error;
   }

   return result;
}

} // namespace ai
