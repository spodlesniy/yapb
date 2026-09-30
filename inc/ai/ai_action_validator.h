//
// AiPB - AI action validator.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>
#include <ai/ai_action_spec.h>

#include <cstdint>

namespace ai {

enum class ActionValidationError : uint8_t {
  None,
  InvalidActionType,
  InvalidTargetType,
  TargetTypeMismatch,
  MissingTargetNode,
  InvalidTargetNode,
  MissingTargetPlayer,
  UnknownTargetPlayer,
  InvalidTargetPosition,
  InvalidWeaponType,
  InvalidGrenadeType,
  InvalidDuration,
  InvalidConfidence,
  UnexpectedParameter,
};

struct ActionValidationResult {
  ActionValidationError error { ActionValidationError::None };

  bool isValid() const {
    return error == ActionValidationError::None;
  }
};

class ActionValidator final {
public:
  static ActionValidationResult validate(const Action &action, const Observation &observation);
};

} // namespace ai
