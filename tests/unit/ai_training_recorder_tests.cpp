//
// AiPB - training transition recorder unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_training_recorder.h>

using ai::test::expect;

namespace {

ai::Action makeMoveAction(int targetNode) {
  ai::Action action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetType = ai::TargetType::Node;
  action.targetNode = targetNode;
  action.confidence = 0.75f;
  return action;
}

ai::Observation makeObservation(float gameTime, int currentNode) {
  ai::Observation observation {};
  observation.gameTime = gameTime;
  observation.bot.alive = true;
  observation.bot.currentNode = currentNode;
  return observation;
}

ai::ActionResult makeCompletedResult(ai::ActionType action) {
  ai::ActionResult result {};
  result.action = action;
  result.type = ai::ActionResultType::Completed;
  return result;
}

} // namespace

AI_TEST(testTrainingRecorderStartsEmpty) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };

  expect(recorder.buffer().empty(), "training recorder starts empty");
  expect(recorder.buffer().size() == 0, "training recorder starts with zero transitions");
  expect(recorder.episodeId() == 0, "training recorder starts before the first episode");
  expect(!recorder.hasPendingAction(), "training recorder starts without a pending action");
}

AI_TEST(testTrainingRecorderEpisodeLifecycle) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };

  recorder.beginEpisode();
  expect(recorder.episodeId() == 1, "beginEpisode starts episode one");

  recorder.beginEpisode();
  expect(recorder.episodeId() == 2, "beginEpisode advances the episode id");
  expect(recorder.buffer().empty(), "starting a new episode does not erase completed transitions");
}

AI_TEST(testTrainingRecorderCompletesTransition) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto observation = makeObservation(10.0f, 40);
  const auto action = makeMoveAction(41);

  expect(recorder.startAction(observation, action), "recorder accepts a new action");
  expect(recorder.hasPendingAction(), "accepted action becomes pending");

  ai::ActionResult accepted {};
  accepted.action = action.type;
  accepted.type = ai::ActionResultType::Accepted;

  const auto nonTerminal = recorder.finishAction(makeObservation(10.1f, 40), accepted, 0.25f);
  expect(nonTerminal == ai::TrainingRecordResult::NonTerminalResult, "non-terminal result does not emit a transition");
  expect(recorder.hasPendingAction(), "non-terminal result keeps the action pending");
  expect(recorder.buffer().empty(), "non-terminal result leaves the transition buffer unchanged");

  ai::ActionResult completed {};
  completed.action = action.type;
  completed.type = ai::ActionResultType::Completed;
  completed.elapsedTime = 0.75f;

  const auto recorded = recorder.finishAction(makeObservation(10.8f, 41), completed, 1.5f);
  expect(recorded == ai::TrainingRecordResult::Recorded, "terminal result records a transition");
  expect(!recorder.hasPendingAction(), "recorded transition clears the pending action");
  expect(recorder.buffer().size() == 1, "recorded transition increments buffer size");

  const auto &transition = recorder.buffer().at(0);
  expect(transition.episodeId == 1, "transition stores its episode id");
  expect(transition.observation.gameTime == 10.0f, "transition stores the starting observation");
  expect(transition.action.targetNode == 41, "transition stores the selected action");
  expect(transition.reward == 1.5f, "transition stores the supplied reward");
  expect(transition.nextObservation.gameTime == 10.8f, "transition stores the terminal observation");
  expect(transition.result.type == ai::ActionResultType::Completed, "transition stores the terminal action result");
}

AI_TEST(testTrainingRecorderRejectsInvalidCompletion) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto action = makeMoveAction(41);
  expect(recorder.startAction(makeObservation(1.0f, 40), action), "recorder accepts the test action");

  ai::ActionResult mismatch {};
  mismatch.action = ai::ActionType::Reload;
  mismatch.type = ai::ActionResultType::Completed;

  expect(recorder.finishAction(makeObservation(1.1f, 40), mismatch, 0.0f) == ai::TrainingRecordResult::ActionMismatch,
         "mismatched result is rejected");
  expect(recorder.hasPendingAction(), "action remains pending after a mismatched result");

  expect(recorder.finishAction(makeObservation(1.1f, 40), {}, 0.0f) == ai::TrainingRecordResult::NonTerminalResult,
         "non-terminal result is rejected");
  expect(recorder.hasPendingAction(), "action remains pending after a non-terminal result");

  recorder.discardPendingAction();
  expect(!recorder.hasPendingAction(), "discardPendingAction clears the pending action");
  expect(recorder.finishAction(makeObservation(1.2f, 40), mismatch, 0.0f) == ai::TrainingRecordResult::NoPendingAction,
         "completion without a pending action is reported");
}

AI_TEST(testTrainingRecorderRejectsNoneAction) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  expect(!recorder.startAction(makeObservation(1.0f, 40), {}), "None action is not recorded as a training action");
  expect(!recorder.hasPendingAction(), "None action does not create pending state");
}

AI_TEST(testTrainingRecorderResetDoesNotEraseSharedSamples) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto action = makeMoveAction(41);
  expect(recorder.startAction(makeObservation(1.0f, 40), action), "recorder accepts an action");

  expect(recorder.finishAction(makeObservation(1.5f, 41), makeCompletedResult(action.type), 1.0f) == ai::TrainingRecordResult::Recorded,
         "recorder records a transition");

  recorder.reset();

  expect(buffer.size() == 1, "recorder reset keeps shared samples");
  expect(recorder.episodeId() == 0, "recorder reset clears its own episode state");
}

AI_TEST(testTrainingBufferCanBeResetIndependently) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder first { buffer };
  ai::TrainingRecorder second { buffer };

  first.beginEpisode();
  const auto action = makeMoveAction(41);
  expect(first.startAction(makeObservation(1.0f, 40), action), "first recorder accepts an action");
  expect(first.finishAction(makeObservation(1.5f, 41), makeCompletedResult(action.type), 1.0f) == ai::TrainingRecordResult::Recorded,
         "first recorder records a transition");

  second.beginEpisode();
  expect(buffer.size() == 1, "second recorder sees the shared sample");

  buffer.reset();

  expect(buffer.empty(), "buffer reset clears completed samples");
  expect(first.episodeId() != 0, "buffer reset does not mutate first recorder state");
  expect(second.episodeId() != 0, "buffer reset does not mutate second recorder state");
}

AI_TEST(testTrainingBufferReportsFullCapacity) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto action = makeMoveAction(41);
  ai::ActionResult completed {};
  completed.action = action.type;
  completed.type = ai::ActionResultType::Completed;

  for (size_t i = 0; i < ai::kTrainingTransitionCapacity; ++i) {
    expect(recorder.startAction(makeObservation(static_cast<float>(i), static_cast<int>(i)), action),
           "recorder accepts each action before the buffer is full");
    expect(recorder.finishAction(makeObservation(static_cast<float>(i) + 0.5f, static_cast<int>(i) + 1), completed, 0.0f) ==
               ai::TrainingRecordResult::Recorded,
           "recorder records each transition before the buffer is full");
  }

  expect(recorder.buffer().isFull(), "recorder reports a full transition buffer");
  expect(recorder.buffer().size() == ai::kTrainingTransitionCapacity, "recorder reaches its configured transition capacity");

  expect(recorder.startAction(makeObservation(999.0f, 999), action), "recorder can keep a pending action after the buffer fills");
  expect(recorder.finishAction(makeObservation(999.5f, 1000), completed, 0.0f) == ai::TrainingRecordResult::BufferFull,
         "full buffer refuses to silently drop a transition");
  expect(recorder.hasPendingAction(), "full buffer keeps the pending transition available for a future flush");
}
