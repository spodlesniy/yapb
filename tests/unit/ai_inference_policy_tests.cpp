//
// AiPB - AI inference boundary unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"
#include "ai_test_tools.h"

#include <ai/ai_inference_policy.h>

using ai::test::expect;

namespace {

class TestInferenceProvider final : public ai::InferenceProvider {
public:
   mutable int callCount {};

   ai::Action infer (const ai::Observation &observation) const override {
      ++callCount;

      ai::Action action {};
      action.type = ai::ActionType::MoveToNode;
      action.targetType = ai::TargetType::Node;
      action.targetNode = observation.bot.currentGoalNode;
      action.confidence = 0.8f;
      return action;
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
}

AI_TEST (testInferencePolicyWithoutProvider) {
   ai::InferencePolicy policy {};

   const ai::Action action = policy.decide ({});

   expect (action.type == ai::ActionType::None, "missing inference provider is non-invasive");
}

