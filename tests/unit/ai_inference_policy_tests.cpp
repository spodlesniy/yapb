//
// AiPB - AI inference boundary unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"
#include "ai_test_tools.h"

#include <ai/ai_action_runtime.h>
#include <ai/ai_inference_policy.h>

using ai::test::expect;
using ai::test::TestExecutor;

namespace {

class TestInferenceProvider final : public ai::InferenceProvider {
public:
  mutable int callCount {};
  mutable ai::InferenceInput lastInput {};
  ai::InferenceStatus status { ai::InferenceStatus::Success };
  uint32_t resultSchemaVersion { ai::kInferenceActionSchemaVersion };

  ai::InferenceResult infer(const ai::InferenceInput &input) const override {
    ++callCount;
    lastInput = input;

    ai::InferenceResult result {};
    result.status = status;
    result.output.schemaVersion = resultSchemaVersion;
    result.output.actionId = static_cast<uint8_t>(ai::InferenceActionId::MoveToNode);
    result.output.targetNode = input.observation.bot.currentGoalNode;
    result.output.confidence = 0.8f;
    return result;
  }
};

} // namespace

AI_TEST(testInferencePolicy) {
  TestInferenceProvider provider {};
  ai::InferencePolicy policy { &provider };

  ai::Observation observation {};
  observation.bot.currentGoalNode = 17;

  const ai::Action action = policy.decide(observation);

  expect(action.type == ai::ActionType::MoveToNode, "inference policy returns provider action");
  expect(action.targetNode == 17, "inference policy forwards observation to provider");
  expect(action.confidence == 0.8f, "inference policy preserves provider output");
  expect(provider.callCount == 1, "inference provider is invoked exactly once");
  expect(provider.lastInput.schemaVersion == ai::kInferenceInputSchemaVersion, "inference input uses the current schema version");
  expect(provider.lastInput.observation.bot.currentGoalNode == 17, "inference input preserves observation state");
}

AI_TEST(testInferenceChain) {
  TestInferenceProvider provider {};
  ai::InferencePolicy policy { &provider };
  TestExecutor executor {};
  ai::ActionRuntime runtime { executor };

  runtime.setMode(ai::ControlMode::Neural);
  runtime.setPolicy(&policy);

  ai::Observation observation {};
  observation.bot.alive = true;
  observation.bot.currentGoalNode = 31;

  const ai::ActionResult result = runtime.step(observation);

  expect(provider.callCount == 1, "inference chain invokes provider exactly once");
  expect(provider.lastInput.observation.bot.currentGoalNode == 31, "inference chain forwards observation to provider");
  expect(result.action == ai::ActionType::MoveToNode, "inference chain forwards provider action into runtime");
  expect(result.type == ai::ActionResultType::Accepted, "inference chain reaches action executor");
  expect(runtime.isActive(), "inference action becomes active in runtime");
  expect(executor.callCount() == 1, "inference action is executed exactly once");
  expect(executor.lastAction().targetNode == 31, "executor receives provider-selected target");
}

AI_TEST(testInferenceChainStopsOnProviderFailure) {
  TestInferenceProvider provider {};
  provider.status = ai::InferenceStatus::Error;

  ai::InferencePolicy policy { &provider };
  TestExecutor executor {};
  ai::ActionRuntime runtime { executor };

  runtime.setMode(ai::ControlMode::Neural);
  runtime.setPolicy(&policy);

  ai::Observation observation {};
  observation.bot.alive = true;

  const ai::ActionResult result = runtime.step(observation);

  expect(provider.callCount == 1, "provider failure is observed once");
  expect(result.type == ai::ActionResultType::None, "provider failure becomes a runtime no-op");
  expect(!runtime.isActive(), "provider failure cannot activate an action");
  expect(executor.callCount() == 0, "provider failure does not reach the executor");
}

AI_TEST(testInferencePolicyWithoutProvider) {
  ai::InferencePolicy policy {};

  const ai::Action action = policy.decide({});

  expect(action.type == ai::ActionType::None, "missing inference provider is non-invasive");
}

AI_TEST(testInferencePolicyHandlesProviderStatus) {
  TestInferenceProvider provider {};
  ai::InferencePolicy policy { &provider };

  ai::Observation observation {};
  observation.bot.currentGoalNode = 23;

  provider.status = ai::InferenceStatus::NoDecision;
  expect(policy.decide(observation).type == ai::ActionType::None, "NoDecision status becomes a no-op action");

  provider.status = ai::InferenceStatus::InvalidInput;
  expect(policy.decide(observation).type == ai::ActionType::None, "InvalidInput status becomes a no-op action");

  provider.status = ai::InferenceStatus::Error;
  expect(policy.decide(observation).type == ai::ActionType::None, "provider error becomes a no-op action");
}

AI_TEST(testInferencePolicyHandlesSchemaMismatch) {
  TestInferenceProvider provider {};
  ai::InferencePolicy policy { &provider };

  provider.resultSchemaVersion = ai::kInferenceActionSchemaVersion + 1;

  const ai::Action action = policy.decide({});

  expect(action.type == ai::ActionType::None, "incompatible inference result schema is rejected");
}

AI_TEST(testInferenceContractDefaults) {
  const ai::InferenceInput input {};
  const ai::InferenceResult result {};

  expect(input.schemaVersion == ai::kInferenceInputSchemaVersion, "inference input defaults to current schema");
  expect(input.hasSupportedSchema(), "current inference input schema is supported");
  expect(result.output.schemaVersion == ai::kInferenceActionSchemaVersion, "inference output defaults to current schema");
  expect(!result.isSuccess(), "inference result defaults to no decision");
}
