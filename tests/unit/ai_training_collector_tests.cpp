//
// AiPB - automatic training lifecycle collector unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"
#include "ai_test_tools.h"

#include <ai/ai_training_collector.h>

using ai::test::expect;
using ai::test::expectNear;

namespace {

class FixedRewardProvider final : public ai::RewardProvider {
public:
  mutable int callCount {};
  mutable float lastRewardInputGameTime {};
  mutable float lastRewardNextGameTime {};
  mutable ai::ActionType lastAction { ai::ActionType::None };
  mutable ai::ActionResultType lastResult { ai::ActionResultType::None };

  float compute(const ai::Observation &observation, const ai::Action &action, const ai::Observation &nextObservation,
                const ai::ActionResult &result) const override {
    ++callCount;
    lastRewardInputGameTime = observation.gameTime;
    lastRewardNextGameTime = nextObservation.gameTime;
    lastAction = action.type;
    lastResult = result.type;
    return 2.5f;
  }
};

ai::Observation makeObservation(float gameTime, int currentNode) {
  ai::Observation observation {};
  observation.gameTime = gameTime;
  observation.bot.alive = true;
  observation.bot.currentNode = currentNode;
  return observation;
}

} // namespace

AI_TEST(testTrainingCollectorRecordsCompletedAction) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  FixedRewardProvider rewards {};
  ai::TrainingCollector collector { recorder, rewards };

  TestExecutor executor {};
  ai::ActionRuntime runtime { executor };
  TestPolicy policy {};
  runtime.setMode(ai::ControlMode::Neural);
  runtime.setPolicy(&policy);

  const auto first = collector.step(runtime, makeObservation(1.0f, 40));
  expect(first.type == ai::ActionResultType::Accepted, "collector forwards the initial accepted action");
  expect(recorder.hasPendingAction(), "collector starts a training action");
  expect(buffer.empty(), "non-terminal action is not recorded");

  executor.setResult(ai::ActionResultType::Completed);
  const auto completed = collector.step(runtime, makeObservation(2.0f, 41));

  expect(completed.type == ai::ActionResultType::Completed, "collector forwards the terminal action result");
  expect(buffer.size() == 1, "collector records one completed transition");
  expect(!recorder.hasPendingAction(), "recorded transition clears pending recorder state");
  expect(rewards.callCount == 1, "collector computes reward exactly once");
  expectNear(rewards.lastRewardInputGameTime, 1.0f, 0.0001f, "reward receives the initial observation");
  expectNear(rewards.lastRewardNextGameTime, 2.0f, 0.0001f, "reward receives the terminal observation");
  expect(rewards.lastAction == ai::ActionType::MoveToNode, "reward receives the selected action");
  expect(rewards.lastResult == ai::ActionResultType::Completed, "reward receives the terminal result");
  expectNear(buffer.at(0).reward, 2.5f, 0.0001f, "computed reward is stored in the transition");
}

AI_TEST(testTrainingCollectorRecordsImmediateCompletion) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  FixedRewardProvider rewards {};
  ai::TrainingCollector collector { recorder, rewards };

  TestExecutor executor {};
  executor.setResult(ai::ActionResultType::Completed);

  ai::ActionRuntime runtime { executor };
  TestPolicy policy {};
  runtime.setMode(ai::ControlMode::Neural);
  runtime.setPolicy(&policy);

  const auto result = collector.step(runtime, makeObservation(5.0f, 20));

  expect(result.type == ai::ActionResultType::Completed, "collector sees an immediate terminal result");
  expect(buffer.size() == 1, "collector records an immediately completed action");
  expect(!recorder.hasPendingAction(), "immediate completion clears pending recorder state");
  expect(rewards.callCount == 1, "immediate completion computes one reward");
}

AI_TEST(testTrainingCollectorIgnoresNonNeuralRuntime) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  FixedRewardProvider rewards {};
  ai::TrainingCollector collector { recorder, rewards };

  TestExecutor executor {};
  ai::ActionRuntime runtime { executor };
  TestPolicy policy {};
  runtime.setMode(ai::ControlMode::Legacy);
  runtime.setPolicy(&policy);

  const auto result = collector.step(runtime, makeObservation(1.0f, 40));

  expect(result.type == ai::ActionResultType::None, "non-neural runtime produces no training action");
  expect(buffer.empty(), "non-neural runtime does not record transitions");
  expect(rewards.callCount == 0, "non-neural runtime does not request a reward");
}

AI_TEST(testTrainingCollectorRecordsCancellation) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  FixedRewardProvider rewards {};
  ai::TrainingCollector collector { recorder, rewards };

  TestExecutor executor {};
  ai::ActionRuntime runtime { executor };
  TestPolicy policy {};
  runtime.setMode(ai::ControlMode::Neural);
  runtime.setPolicy(&policy);

  collector.step(runtime, makeObservation(10.0f, 30));
  expect(recorder.hasPendingAction(), "cancellation test starts a pending action");

  expect(collector.cancel(runtime, makeObservation(10.5f, 31)), "collector cancels the active runtime action");
  expect(runtime.result().type == ai::ActionResultType::Interrupted, "runtime exposes the interrupted result");
  expect(buffer.size() == 1, "cancellation records a terminal transition");
  expect(buffer.at(0).result.type == ai::ActionResultType::Interrupted, "transition stores the interruption result");
  expect(rewards.callCount == 1, "cancellation computes one reward");
}

} // namespace
