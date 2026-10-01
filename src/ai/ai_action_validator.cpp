//
// AiPB - AI action validator implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_action_validator.h>

#include <cmath>
#include <cstdint>

namespace ai {
namespace {

bool isValidActionType(ActionType type) {
  return static_cast<uint8_t> (type) < static_cast<uint8_t> (ActionType::Count);
}

bool isValidTargetType(TargetType type) {
  return static_cast<uint8_t> (type) <= static_cast<uint8_t> (TargetType::Position);
}

bool isValidWeaponType(WeaponType type) {
  return static_cast<uint8_t> (type) <= static_cast<uint8_t> (WeaponType::Heavy);
}

bool isConcreteWeaponType(WeaponType type) {
  return isValidWeaponType(type) && type != WeaponType::Unknown && type != WeaponType::None;
}

bool isValidGrenadeType(GrenadeType type) {
  return static_cast<uint8_t> (type) <= static_cast<uint8_t> (GrenadeType::Smoke);
}

bool isConcreteGrenadeType(GrenadeType type) {
  return isValidGrenadeType(type) && type != GrenadeType::None;
}

bool isFinite(const Vec3 &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isZero(const Vec3 &value) {
  return value.x == 0.0f && value.y == 0.0f && value.z == 0.0f;
}

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

ActionValidationResult invalid(ActionValidationError error) {
  return { error };
}

} // namespace

ActionValidationResult ActionValidator::validate(const Action &action, const Observation &observation) {
  if (!isValidActionType(action.type)) {
    return invalid(ActionValidationError::InvalidActionType);
  }

  if (!isValidTargetType(action.targetType)) {
    return invalid(ActionValidationError::InvalidTargetType);
  }

  const auto spec = getActionSpec(action.type);
  if (action.targetType != spec.targetType) {
    return invalid(ActionValidationError::TargetTypeMismatch);
  }

  if (!std::isfinite(action.confidence) || action.confidence < 0.0f || action.confidence > 1.0f) {
    return invalid(ActionValidationError::InvalidConfidence);
  }

  if (!std::isfinite(action.duration) || action.duration < 0.0f) {
    return invalid(ActionValidationError::InvalidDuration);
  }

  if (spec.requiredParameters & actionParameter(ActionParameter::TargetNode)) {
    if (action.targetNode < 0) {
      return invalid(ActionValidationError::MissingTargetNode);
    }
  }

  if (spec.requiredParameters & actionParameter(ActionParameter::TargetPlayer)) {
    if (action.targetPlayer < 0) {
      return invalid(ActionValidationError::MissingTargetPlayer);
    }
    if (!hasObservedPlayer(observation, action.targetPlayer)) {
      return invalid(ActionValidationError::UnknownTargetPlayer);
    }
  }

  if (spec.requiredParameters & actionParameter(ActionParameter::TargetPosition)) {
    if (!isFinite(action.targetPosition)) {
      return invalid(ActionValidationError::InvalidTargetPosition);
    }
  }

  if (spec.requiredParameters & actionParameter(ActionParameter::WeaponType)) {
    if (!isConcreteWeaponType(action.weaponType)) {
      return invalid(ActionValidationError::InvalidWeaponType);
    }
  }

  if (spec.requiredParameters & actionParameter(ActionParameter::GrenadeType)) {
    if (!isConcreteGrenadeType(action.grenadeType)) {
      return invalid(ActionValidationError::InvalidGrenadeType);
    }
  }

  if (spec.requiredParameters & actionParameter(ActionParameter::WeaponType)) {
    // Required weapon parameters were validated above.
  } else if (spec.optionalParameters & actionParameter(ActionParameter::WeaponType)) {
    if (action.weaponType != WeaponType::Unknown && !isConcreteWeaponType(action.weaponType)) {
      return invalid(ActionValidationError::InvalidWeaponType);
    }
  } else if (action.weaponType != WeaponType::Unknown) {
    return invalid(ActionValidationError::UnexpectedParameter);
  }

  if (spec.optionalParameters & actionParameter(ActionParameter::GrenadeType)) {
    if (action.grenadeType != GrenadeType::None && !isConcreteGrenadeType(action.grenadeType)) {
      return invalid(ActionValidationError::InvalidGrenadeType);
    }
  } else if (action.type != ActionType::ThrowGrenade && action.grenadeType != GrenadeType::None) {
    return invalid(ActionValidationError::UnexpectedParameter);
  }

  if (spec.optionalParameters & actionParameter(ActionParameter::Duration)) {
    if (action.duration < 0.0f) {
      return invalid(ActionValidationError::InvalidDuration);
    }
  } else if (action.duration != 0.0f) {
    return invalid(ActionValidationError::UnexpectedParameter);
  }

  switch (action.targetType) {
  case TargetType::None:
    if (action.targetNode != -1 || action.targetPlayer != -1 || !isZero(action.targetPosition)) {
      return invalid(ActionValidationError::UnexpectedParameter);
    }
    break;
  case TargetType::Node:
    if (action.targetNode < 0) {
      return invalid(ActionValidationError::InvalidTargetNode);
    }
    if (action.targetPlayer != -1 || !isZero(action.targetPosition)) {
      return invalid(ActionValidationError::UnexpectedParameter);
    }
    break;
  case TargetType::Player:
    if (action.targetPlayer < 0) {
      return invalid(ActionValidationError::MissingTargetPlayer);
    }
    if (action.targetNode != -1 || !isZero(action.targetPosition)) {
      return invalid(ActionValidationError::UnexpectedParameter);
    }
    break;
  case TargetType::Position:
    if (!isFinite(action.targetPosition)) {
      return invalid(ActionValidationError::InvalidTargetPosition);
    }
    if (action.targetNode != -1 || action.targetPlayer != -1) {
      return invalid(ActionValidationError::UnexpectedParameter);
    }
    break;
  }
  return {};
}

} // namespace ai
