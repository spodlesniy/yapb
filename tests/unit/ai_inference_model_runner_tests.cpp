//
// AiPB - AI model runner boundary unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_inference_model_runner.h>

#include <cstddef>

using ai::test::expect;

namespace {

class TestModelRunner final : public ai::InferenceModelRunner {
public:
   mutable int callCount {};
   mutable ai::InferenceFeatures lastFeatures {};
   ai::InferenceResult result {};

   ai::InferenceResult run (const ai::InferenceFeatures &features) const override {
      ++callCount;
      lastFeatures = features;
      return result;
   }
};

} // namespace

AI_TEST (testModelInferenceProvider) {
   TestModelRunner runner {};
   runner.result.status = ai::InferenceStatus::Success;
   runner.result.output.actionId = static_cast<uint8_t> (ai::InferenceActionId::Fire);
   runner.result.output.confidence = 0.9f;

   ai::ModelInferenceProvider provider { &runner };

   ai::InferenceInput input {};
   input.observation.bot.health = 75.0f;

   const auto result = provider.infer (input);

   expect (result.status == ai::InferenceStatus::Success,
      "model inference provider forwards runner success");
   expect (result.output.actionId == static_cast<uint8_t> (ai::InferenceActionId::Fire),
      "model inference provider preserves runner action output");
   expect (runner.callCount == 1,
      "model runner is invoked exactly once");
   expect (runner.lastFeatures.hasSupportedSchema (),
      "model runner receives a supported feature schema");
   expect (runner.lastFeatures.values[static_cast<size_t> (ai::InferenceFeature::Core::Health)] == 0.75f,
      "model runner receives encoded observation features");
}

AI_TEST (testModelInferenceProviderWithoutRunner) {
   ai::ModelInferenceProvider provider {};

   const auto result = provider.infer ({});

   expect (result.status == ai::InferenceStatus::Error,
      "missing model runner reports an inference error");
}

AI_TEST (testModelInferenceProviderRejectsInputSchema) {
   TestModelRunner runner {};
   ai::ModelInferenceProvider provider { &runner };

   ai::InferenceInput input {};
   input.schemaVersion = ai::kInferenceInputSchemaVersion + 1;

   const auto result = provider.infer (input);

   expect (result.status == ai::InferenceStatus::InvalidInput,
      "unsupported input schema is rejected before model execution");
   expect (runner.callCount == 0,
      "invalid input schema never reaches model runner");
}

AI_TEST (testModelInferenceProviderPropagatesRunnerError) {
   TestModelRunner runner {};
   runner.result.status = ai::InferenceStatus::Error;

   ai::ModelInferenceProvider provider { &runner };

   const auto result = provider.infer ({});

   expect (result.status == ai::InferenceStatus::Error,
      "model runner errors propagate unchanged");
   expect (!result.isSuccess (),
      "model runner error cannot be reported as success");
}

