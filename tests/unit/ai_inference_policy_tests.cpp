//
// AiPB - AI inference boundary unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_inference_policy.h>

using ai::test::expect;

namespace {

class TestInferenceProvider final : public ai::InferenceProvider {
public:
   mutable int callCount {};
   ai::InferenceInput lastInput {};
   ai::InferenceStatus status { ai::InferenceStatus::Success };
   uint32_t resultSchemaVersion { ai::kInferenceSchemaVersion };

   ai::InferenceResult infer (const ai::InferenceInput &input) const override {
      ++callCount;
      lastInput = input;

      ai::InferenceResult result {};
      result.schemaVersion = resultSchemaVersion;
      result.status = status;
      result.action.type = ai::ActionType::MoveToNode;
      result.action.targetType = ai::TargetType::Node;
      result.action.targetNode = input.observation.bot.currentGoalNode;
      result.action.confidence = 0.8f;
      return result;
   }
};

} // namespace

AI_TEST (testInferencePolicy) {
   TestInferenceProvider provider {};
   ai::InferencePolicy policy { &provider };

   ai::Observation observation {};
   observation.bot.currentGoalNode = 17;

   const ai::Action action = policy.decide (observation);

   expect (action.type == ai::ActionType::MoveToNode, "inference policy returns provider action");
   expect (action.targetNode == 17, "inference policy forwards observation to provider");
   expect (action.confidence == 0.8f, "inference policy preserves provider output");
   expect (provider.callCount == 1, "inference provider is invoked exactly once");
   expect (provider.lastInput.schemaVersion == ai::kInferenceSchemaVersion,
      "inference input uses the current schema version");
   expect (provider.lastInput.observation.bot.currentGoalNode == 17,
      "inference input preserves observation state");
}

AI_TEST (testInferencePolicyWithoutProvider) {
   ai::InferencePolicy policy {};

   const ai::Action action = policy.decide ({});

   expect (action.type == ai::ActionType::None, "missing inference provider is non-invasive");
}

AI_TEST (testInferencePolicyHandlesProviderStatus) {
   TestInferenceProvider provider {};
   ai::InferencePolicy policy { &provider };

   ai::Observation observation {};
   observation.bot.currentGoalNode = 23;

   provider.status = ai::InferenceStatus::NoDecision;
   expect (policy.decide (observation).type == ai::ActionType::None,
      "NoDecision status becomes a no-op action");

   provider.status = ai::InferenceStatus::InvalidInput;
   expect (policy.decide (observation).type == ai::ActionType::None,
      "InvalidInput status becomes a no-op action");

   provider.status = ai::InferenceStatus::Error;
   expect (policy.decide (observation).type == ai::ActionType::None,
      "provider error becomes a no-op action");
}

AI_TEST (testInferencePolicyHandlesSchemaMismatch) {
   TestInferenceProvider provider {};
   ai::InferencePolicy policy { &provider };

   provider.resultSchemaVersion = ai::kInferenceSchemaVersion + 1;

   const ai::Action action = policy.decide ({});

   expect (action.type == ai::ActionType::None,
      "incompatible inference result schema is rejected");
}

AI_TEST (testInferenceContractDefaults) {
   const ai::InferenceInput input {};
   const ai::InferenceResult result {};

   expect (input.schemaVersion == ai::kInferenceSchemaVersion,
      "inference input defaults to current schema");
   expect (input.hasSupportedSchema (),
      "current inference input schema is supported");
   expect (result.schemaVersion == ai::kInferenceSchemaVersion,
      "inference result defaults to current schema");
   expect (!result.isSuccess (),
      "inference result defaults to no decision");
}

