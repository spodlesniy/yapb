//
// AiPB - AI action validation unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_action.h>
#include <ai/ai_action_validator.h>

#include <limits>

using ai::test::expect;

AI_TEST(testActionValidation) {
  ai::Observation observation {};
  observation.playerCount = 1;
  observation.players[0].entityIndex = 7;
  observation.players[0].valid = true;

  ai::Action none {};
  expect(ai::ActionValidator::validate(none, observation).isValid(), "default none action is valid");

  ai::Action node {};
  node.type = ai::ActionType::MoveToNode;
  node.targetType = ai::TargetType::Node;
  node.targetNode = 12;
  node.confidence = 0.8f;
  expect(ai::ActionValidator::validate(node, observation).isValid(), "valid node action is accepted");

  node.targetNode = -1;
  expect(ai::ActionValidator::validate(node, observation).error == ai::ActionValidationError::MissingTargetNode,
         "missing node target is rejected");

  node.targetNode = 12;
  node.targetType = ai::TargetType::Player;
  expect(ai::ActionValidator::validate(node, observation).error == ai::ActionValidationError::TargetTypeMismatch,
         "node action with player target type is rejected");

  ai::Action position {};
  position.type = ai::ActionType::MoveToPosition;
  position.targetType = ai::TargetType::Position;
  position.targetPosition = { 10.0f, -20.0f, 30.0f };
  position.confidence = 0.5f;
  expect(ai::ActionValidator::validate(position, observation).isValid(), "valid position action is accepted");

  position.targetPosition.x = std::numeric_limits<float>::quiet_NaN();
  expect(ai::ActionValidator::validate(position, observation).error == ai::ActionValidationError::InvalidTargetPosition,
         "non-finite target position is rejected");

  ai::Action attack {};
  attack.type = ai::ActionType::AttackTarget;
  attack.targetType = ai::TargetType::Player;
  attack.targetPlayer = 7;
  expect(ai::ActionValidator::validate(attack, observation).isValid(), "observed player target is accepted");

  attack.targetPlayer = 99;
  expect(ai::ActionValidator::validate(attack, observation).error == ai::ActionValidationError::UnknownTargetPlayer,
         "unknown player target is rejected");

  attack.targetPlayer = -1;
  expect(ai::ActionValidator::validate(attack, observation).error == ai::ActionValidationError::MissingTargetPlayer,
         "missing player target is rejected");

  ai::Action changeWeapon {};
  changeWeapon.type = ai::ActionType::ChangeWeapon;
  changeWeapon.weaponType = ai::WeaponType::Rifle;
  expect(ai::ActionValidator::validate(changeWeapon, observation).isValid(), "valid weapon change is accepted");

  changeWeapon.weaponType = ai::WeaponType::Unknown;
  expect(ai::ActionValidator::validate(changeWeapon, observation).error == ai::ActionValidationError::InvalidWeaponType,
         "missing weapon type is rejected");

  ai::Action grenade {};
  grenade.type = ai::ActionType::ThrowGrenade;
  grenade.targetType = ai::TargetType::Position;
  grenade.targetPosition = { 1.0f, 2.0f, 3.0f };
  grenade.grenadeType = ai::GrenadeType::Smoke;
  expect(ai::ActionValidator::validate(grenade, observation).isValid(), "valid grenade action is accepted");

  grenade.grenadeType = ai::GrenadeType::None;
  expect(ai::ActionValidator::validate(grenade, observation).error == ai::ActionValidationError::InvalidGrenadeType,
         "missing grenade type is rejected");

  ai::Action wait {};
  wait.type = ai::ActionType::Wait;
  wait.duration = 2.0f;
  expect(ai::ActionValidator::validate(wait, observation).isValid(), "positive wait duration is accepted");

  wait.duration = -1.0f;
  expect(ai::ActionValidator::validate(wait, observation).error == ai::ActionValidationError::InvalidDuration,
         "negative duration is rejected");

  ai::Action confidence {};
  confidence.type = ai::ActionType::Fire;
  confidence.confidence = std::numeric_limits<float>::infinity();
  expect(ai::ActionValidator::validate(confidence, observation).error == ai::ActionValidationError::InvalidConfidence,
         "non-finite confidence is rejected");

  confidence.confidence = 1.1f;
  expect(ai::ActionValidator::validate(confidence, observation).error == ai::ActionValidationError::InvalidConfidence,
         "out-of-range confidence is rejected");

  ai::Action unexpected {};
  unexpected.type = ai::ActionType::Fire;
  unexpected.targetNode = 4;
  expect(ai::ActionValidator::validate(unexpected, observation).error == ai::ActionValidationError::UnexpectedParameter,
         "unexpected action parameter is rejected");

  ai::Action invalidType {};
  invalidType.type = static_cast<ai::ActionType>(255);
  expect(ai::ActionValidator::validate(invalidType, observation).error == ai::ActionValidationError::InvalidActionType,
         "unknown action type is rejected");

  ai::Action invalidTarget {};
  invalidTarget.targetType = static_cast<ai::TargetType>(255);
  expect(ai::ActionValidator::validate(invalidTarget, observation).error == ai::ActionValidationError::InvalidTargetType,
         "unknown target type is rejected");
}
