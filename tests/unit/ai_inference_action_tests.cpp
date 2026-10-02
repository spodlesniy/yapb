//
// AiPB - AI model action decoder unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_inference_action_decoder.h>
#include <ai/ai_inference_action_encoder.h>

#include <limits>

using ai::test::expect;

AI_TEST(testInferenceActionDecode) {
  ai::InferenceActionOutput output {};
  output.actionId = static_cast<uint8_t>(ai::InferenceActionId::MoveToNode);
  output.targetNode = 42;
  output.confidence = 0.75f;

  const auto decoded = ai::decodeInferenceAction(output, {});

  expect(decoded.isValid(), "valid model output decodes successfully");
  expect(decoded.action.type == ai::ActionType::MoveToNode, "model action id maps to action type");
  expect(decoded.action.targetType == ai::TargetType::Node, "target type is derived from action specification");
  expect(decoded.action.targetNode == 42, "model node parameter is preserved");
  expect(decoded.action.confidence == 0.75f, "model confidence is preserved");
}

AI_TEST(testInferenceActionDecodePlayerTarget) {
  ai::InferenceActionOutput output {};
  output.actionId = static_cast<uint8_t>(ai::InferenceActionId::AttackTarget);
  output.targetPlayer = 7;
  output.confidence = 1.0f;

  ai::Observation observation {};
  observation.playerCount = 1;
  observation.players[0].entityIndex = 7;
  observation.players[0].valid = true;

  const auto decoded = ai::decodeInferenceAction(output, observation);

  expect(decoded.isValid(), "valid player-target output decodes successfully");
  expect(decoded.action.targetType == ai::TargetType::Player, "player target type comes from action specification");
  expect(decoded.action.targetPlayer == 7, "player target parameter is preserved");
}

AI_TEST(testInferenceActionDecodeRejectsInvalidId) {
  ai::InferenceActionOutput output {};
  output.actionId = 255;

  const auto decoded = ai::decodeInferenceAction(output, {});

  expect(decoded.error == ai::InferenceActionDecodeError::InvalidActionId, "unknown model action id is rejected");
}

AI_TEST(testInferenceActionDecodeRejectsSchema) {
  ai::InferenceActionOutput output {};
  output.schemaVersion = ai::kInferenceActionSchemaVersion + 1;

  const auto decoded = ai::decodeInferenceAction(output, {});

  expect(decoded.error == ai::InferenceActionDecodeError::UnsupportedSchema, "unknown model output schema is rejected");
}

AI_TEST(testInferenceActionDecodeUsesValidator) {
  ai::InferenceActionOutput output {};
  output.actionId = static_cast<uint8_t>(ai::InferenceActionId::Fire);
  output.targetNode = 12;

  const auto decoded = ai::decodeInferenceAction(output, {});

  expect(decoded.error == ai::InferenceActionDecodeError::InvalidAction, "invalid model parameters are rejected by action validation");
  expect(decoded.validationError == ai::ActionValidationError::UnexpectedParameter,
         "decoder exposes the underlying action validation error");
}

AI_TEST(testInferenceActionDecodeRejectsNonFiniteOutput) {
  ai::InferenceActionOutput output {};
  output.actionId = static_cast<uint8_t>(ai::InferenceActionId::Fire);
  output.confidence = std::numeric_limits<float>::quiet_NaN();

  const auto decoded = ai::decodeInferenceAction(output, {});

  expect(decoded.error == ai::InferenceActionDecodeError::InvalidAction, "non-finite model confidence is rejected");
  expect(decoded.validationError == ai::ActionValidationError::InvalidConfidence, "decoder reports invalid confidence");
}

AI_TEST(testInferenceActionIdsRemainStable) {
  const auto value = [](ai::InferenceActionId id) { return static_cast<uint8_t>(id); };

  expect(value(ai::InferenceActionId::None) == 0, "None action id remains stable");
  expect(value(ai::InferenceActionId::MoveToNode) == 1, "MoveToNode action id remains stable");
  expect(value(ai::InferenceActionId::AttackTarget) == 8, "AttackTarget action id remains stable");
  expect(value(ai::InferenceActionId::ThrowSmoke) == 24, "ThrowSmoke action id remains stable");
  expect(value(ai::InferenceActionId::Count) == 25, "action id count remains stable");
}

AI_TEST(testInferenceActionEncode) {
  ai::Action action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 42;
  action.weaponType = ai::WeaponType::Rifle;
  action.grenadeType = ai::GrenadeType::None;
  action.duration = 1.5f;
  action.confidence = 0.75f;

  const auto encoded = ai::encodeInferenceAction(action);

  expect(encoded.isValid(), "supported action encodes successfully");
  expect(encoded.output.schemaVersion == ai::kInferenceActionSchemaVersion, "encoder preserves the model action schema");
  expect(encoded.output.actionId == static_cast<uint8_t>(ai::InferenceActionId::MoveToNode),
         "action type maps to the stable model action id");
  expect(encoded.output.targetNode == 42, "node target parameter is preserved");
  expect(encoded.output.weaponType == static_cast<uint8_t>(ai::WeaponType::Rifle), "weapon parameter is preserved");
  expect(encoded.output.duration == 1.5f, "duration parameter is preserved");
  expect(encoded.output.confidence == 0.75f, "confidence parameter is preserved");
}

AI_TEST(testInferenceActionEncodePreservesTerminalActionParameters) {
  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetPlayer = 7;
  action.targetPosition = { 10.0f, -20.0f, 30.0f };
  action.duration = 0.5f;
  action.confidence = 1.0f;

  const auto encoded = ai::encodeInferenceAction(action);

  expect(encoded.isValid(), "combat action encodes successfully");
  expect(encoded.output.actionId == static_cast<uint8_t>(ai::InferenceActionId::AttackTarget),
         "combat action maps to the stable model action id");
  expect(encoded.output.targetPlayer == 7, "player target parameter is preserved");
  expect(encoded.output.targetPosition.x == 10.0f, "target position X is preserved");
  expect(encoded.output.targetPosition.y == -20.0f, "target position Y is preserved");
  expect(encoded.output.targetPosition.z == 30.0f, "target position Z is preserved");
}

AI_TEST(testInferenceActionEncodeRejectsUnsupportedAction) {
  ai::Action action {};
  action.type = ai::ActionType::Count;

  const auto encoded = ai::encodeInferenceAction(action);

  expect(encoded.error == ai::InferenceActionEncodeError::UnsupportedAction,
         "unsupported action type is rejected by the encoder");
}

AI_TEST(testInferenceActionEncodeMapsStableIds) {
  ai::Action move {};
  move.type = ai::ActionType::MoveToNode;
  const auto moveEncoded = ai::encodeInferenceAction(move);
  expect(moveEncoded.output.actionId == 1, "MoveToNode model id remains stable");

  ai::Action attack {};
  attack.type = ai::ActionType::AttackTarget;
  const auto attackEncoded = ai::encodeInferenceAction(attack);
  expect(attackEncoded.output.actionId == 8, "AttackTarget model id remains stable");

  ai::Action smoke {};
  smoke.type = ai::ActionType::ThrowSmoke;
  const auto smokeEncoded = ai::encodeInferenceAction(smoke);
  expect(smokeEncoded.output.actionId == 24, "ThrowSmoke model id remains stable");
}
