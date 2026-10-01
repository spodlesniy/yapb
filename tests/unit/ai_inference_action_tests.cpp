//
// AiPB - AI model action decoder unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_inference_action_decoder.h>

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
