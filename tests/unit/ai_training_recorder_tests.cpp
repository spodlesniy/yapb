//
// AiPB - training transition recorder unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cstdio>

#include <ai/ai_training_recorder.h>
#include <ai/ai_training_sample.h>
#include <ai/ai_training_dataset.h>

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

AI_TEST(testTrainingRecorderEndsEpisode) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto action = makeMoveAction(41);
  expect(recorder.startAction(makeObservation(2.0f, 40), action), "recorder starts an action before episode end");

  recorder.endEpisode();

  expect(recorder.episodeId() == 0, "endEpisode clears the active episode id");
  expect(!recorder.hasPendingAction(), "endEpisode discards the pending action");
  expect(buffer.empty(), "endEpisode preserves the completed transition buffer");
}

AI_TEST(testTrainingBufferCountsBufferedEpisodes) {
  ai::TrainingBuffer buffer {};
  const auto firstEpisode = buffer.beginEpisode();
  const auto secondEpisode = buffer.beginEpisode();
  const auto action = makeMoveAction(41);

  expect(buffer.append(firstEpisode, makeObservation(1.0f, 40), action, 1.0f,
                       makeObservation(1.5f, 41), makeCompletedResult(action.type)),
         "episode count test appends the first transition");
  expect(buffer.append(secondEpisode, makeObservation(2.0f, 41), action, 1.0f,
                       makeObservation(2.5f, 42), makeCompletedResult(action.type)),
         "episode count test appends the second transition");
  expect(buffer.append(firstEpisode, makeObservation(3.0f, 42), action, 1.0f,
                       makeObservation(3.5f, 43), makeCompletedResult(action.type)),
         "episode count test appends another transition from the first episode");

  expect(buffer.episodeCount() == 2, "training buffer counts unique buffered episode ids");
}

AI_TEST(testTrainingBufferExposesReadOnlyData) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto action = makeMoveAction(41);
  expect(recorder.startAction(makeObservation(3.0f, 40), action), "recorder accepts an action");

  expect(recorder.finishAction(makeObservation(3.5f, 41), makeCompletedResult(action.type), 1.25f) ==
             ai::TrainingRecordResult::Recorded,
         "recorder records a transition");

  const auto *data = buffer.data();
  expect(data != nullptr, "training buffer exposes its storage address");
  expect(data == &buffer.at(0), "data points at the first recorded transition");
  expect(data[0].reward == 1.25f, "data exposes the recorded transition contents");
}

AI_TEST(testTrainingRecorderRequiresActiveEpisode) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };

  const auto action = makeMoveAction(41);
  expect(!recorder.startAction(makeObservation(1.0f, 40), action),
         "recorder rejects actions when no training episode is active");
  expect(!recorder.hasPendingAction(), "rejected action does not create pending state");
  expect(buffer.empty(), "rejected action does not modify the training buffer");
}

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

  ai::ActionResult nonTerminal {};
  nonTerminal.action = action.type;
  nonTerminal.type = ai::ActionResultType::Accepted;

  expect(recorder.finishAction(makeObservation(1.1f, 40), nonTerminal, 0.0f) == ai::TrainingRecordResult::NonTerminalResult,
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

AI_TEST(testTrainingBufferCanBeClearedWithoutResettingEpisodeIds) {
  ai::TrainingBuffer buffer {};

  const auto firstEpisode = buffer.beginEpisode();
  expect(firstEpisode == 1, "first buffer episode starts at one");
  expect(buffer.append(firstEpisode, makeObservation(1.0f, 40), makeMoveAction(41), 1.0f,
                     makeObservation(1.5f, 41), makeCompletedResult(ai::ActionType::MoveToNode)),
         "buffer accepts the first sample");

  buffer.clear();

  expect(buffer.empty(), "clear removes completed transitions");
  const auto secondEpisode = buffer.beginEpisode();
  expect(secondEpisode == 2, "clear preserves the episode id sequence");
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
  expect(recorder.buffer().droppedTransitions() == 0, "full capacity has not dropped a transition yet");

  expect(recorder.startAction(makeObservation(999.0f, 999), action), "recorder can keep a pending action after the buffer fills");
  expect(recorder.finishAction(makeObservation(999.5f, 1000), completed, 0.0f) == ai::TrainingRecordResult::BufferFull,
         "full buffer refuses to silently drop a transition");
  expect(recorder.buffer().droppedTransitions() == 1, "full buffer records the dropped transition count");
  expect(recorder.hasPendingAction(), "full buffer keeps the pending transition available for a future flush");
}

AI_TEST(testTrainingSampleEncoding) {
  ai::TrainingTransition transition {};
  transition.episodeId = 7;
  transition.observation = makeObservation(12.0f, 40);
  transition.observation.roundTimeRemaining = 300.0f;
  transition.action = makeMoveAction(41);
  transition.reward = 1.5f;
  transition.nextObservation = makeObservation(13.0f, 41);
  transition.nextObservation.roundTimeRemaining = 240.0f;
  transition.result = makeCompletedResult(transition.action.type);
  transition.result.elapsedTime = 1.0f;

  const auto encoded = ai::encodeTrainingTransition(transition);

  expect(encoded.isValid(), "completed transition encodes into a training sample");
  expect(encoded.sample.hasSupportedSchema(), "training sample uses supported inference schemas");
  expect(encoded.sample.episodeId == 7, "training sample preserves the episode id");
  expect(encoded.sample.action.actionId == static_cast<uint8_t>(ai::InferenceActionId::MoveToNode),
         "training sample encodes the action with the stable model id");
  expect(encoded.sample.observation.at(static_cast<size_t>(ai::InferenceFeature::Core::RoundTimeRemaining)) == 0.5f,
         "training sample contains encoded observation features");
  expect(encoded.sample.action.targetNode == 41, "training sample preserves the action target");
  expect(encoded.sample.reward == 1.5f, "training sample preserves reward");
  expect(encoded.sample.nextObservation.at(static_cast<size_t>(ai::InferenceFeature::Core::RoundTimeRemaining)) == 0.4f,
         "training sample contains encoded next-observation features");
  expect(encoded.sample.result == ai::ActionResultType::Completed, "training sample preserves the terminal result type");
  expect(encoded.sample.elapsedTime == 1.0f, "training sample preserves action elapsed time");
  expect(encoded.sample.terminal, "terminal action result is marked terminal");
}

AI_TEST(testTrainingSampleEncodingRejectsInvalidEpisode) {
  ai::TrainingTransition transition {};
  transition.action = makeMoveAction(41);
  transition.result = makeCompletedResult(transition.action.type);

  const auto encoded = ai::encodeTrainingTransition(transition);

  expect(encoded.error == ai::TrainingSampleEncodeError::InvalidEpisode,
         "training sample encoder rejects transitions without an episode");
}

AI_TEST(testTrainingSampleEncodingRejectsUnsupportedAction) {
  ai::TrainingTransition transition {};
  transition.episodeId = 1;
  transition.action.type = ai::ActionType::Count;
  transition.result = makeCompletedResult(transition.action.type);

  const auto encoded = ai::encodeTrainingTransition(transition);

  expect(encoded.error == ai::TrainingSampleEncodeError::UnsupportedAction,
         "training sample encoder rejects unsupported actions");
}

AI_TEST(testTrainingSampleEncodingPreservesNonZeroRewardAndTerminalState) {
  ai::TrainingTransition transition {};
  transition.episodeId = 3;
  transition.observation = makeObservation(20.0f, 50);
  transition.action = makeMoveAction(51);
  transition.reward = -1.0f;
  transition.nextObservation = makeObservation(21.0f, 50);

  ai::ActionResult failed {};
  failed.action = transition.action.type;
  failed.type = ai::ActionResultType::Failed;
  transition.result = failed;

  const auto encoded = ai::encodeTrainingTransition(transition);

  expect(encoded.isValid(), "failed transition still produces a training sample");
  expect(encoded.sample.reward == -1.0f, "negative reward is preserved");
  expect(encoded.sample.result == ai::ActionResultType::Failed, "failed result metadata is preserved");
  expect(encoded.sample.terminal, "failed result is marked terminal");
}

AI_TEST(testTrainingBufferBatchEncoding) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto firstAction = makeMoveAction(41);
  expect(recorder.startAction(makeObservation(1.0f, 40), firstAction), "batch test starts the first action");
  expect(recorder.finishAction(makeObservation(1.5f, 41), makeCompletedResult(firstAction.type), 1.0f) ==
             ai::TrainingRecordResult::Recorded,
         "batch test records the first transition");

  const auto secondAction = makeMoveAction(42);
  expect(recorder.startAction(makeObservation(2.0f, 41), secondAction), "batch test starts the second action");
  expect(recorder.finishAction(makeObservation(2.5f, 42), makeCompletedResult(secondAction.type), -1.0f) ==
             ai::TrainingRecordResult::Recorded,
         "batch test records the second transition");

  ai::TrainingSample samples[2] {};
  const auto encoded = ai::encodeTrainingBuffer(buffer, samples, 2);

  expect(encoded.isValid(), "training buffer encodes as a complete batch");
  expect(encoded.count == 2, "batch encoder returns the number of encoded samples");
  expect(encoded.failedIndex == 0, "successful batch does not report a failed index");
  expect(samples[0].episodeId == 1, "first sample preserves the episode id");
  expect(samples[0].action.targetNode == 41, "first sample preserves its action");
  expect(samples[0].reward == 1.0f, "first sample preserves its reward");
  expect(samples[1].action.targetNode == 42, "second sample preserves its action");
  expect(samples[1].reward == -1.0f, "second sample preserves its reward");
}

AI_TEST(testTrainingBufferBatchEncodingRejectsSmallOutputBuffer) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto action = makeMoveAction(41);
  expect(recorder.startAction(makeObservation(1.0f, 40), action), "small-buffer test starts an action");
  expect(recorder.finishAction(makeObservation(1.5f, 41), makeCompletedResult(action.type), 1.0f) ==
             ai::TrainingRecordResult::Recorded,
         "small-buffer test records a transition");

  ai::TrainingSample samples[1] {};
  const auto encoded = ai::encodeTrainingBuffer(buffer, samples, 0);

  expect(encoded.error == ai::TrainingSampleEncodeError::OutputBufferTooSmall,
         "batch encoder rejects an undersized output buffer");
  expect(encoded.count == 0, "small output buffer produces no encoded samples");
}

AI_TEST(testTrainingBufferBatchEncodingRejectsNullOutputBuffer) {
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  recorder.beginEpisode();

  const auto action = makeMoveAction(41);
  expect(recorder.startAction(makeObservation(1.0f, 40), action), "null-buffer test starts an action");
  expect(recorder.finishAction(makeObservation(1.5f, 41), makeCompletedResult(action.type), 1.0f) ==
             ai::TrainingRecordResult::Recorded,
         "null-buffer test records a transition");

  const auto encoded = ai::encodeTrainingBuffer(buffer, nullptr, buffer.size());

  expect(encoded.error == ai::TrainingSampleEncodeError::NullOutputBuffer,
         "batch encoder rejects a null output buffer");
  expect(encoded.count == 0, "null output buffer produces no encoded samples");
}

AI_TEST(testTrainingBufferBatchEncodingStopsOnInvalidTransition) {
  ai::TrainingBuffer buffer {};
  const auto episode = buffer.beginEpisode();

  expect(buffer.append(episode, makeObservation(1.0f, 40), makeMoveAction(41), 1.0f,
                       makeObservation(1.5f, 41), makeCompletedResult(ai::ActionType::MoveToNode)),
         "invalid-batch test appends a valid transition");

  ai::TrainingTransition unsupported {};
  unsupported.episodeId = episode;
  unsupported.action.type = ai::ActionType::Count;
  unsupported.result = makeCompletedResult(unsupported.action.type);
  expect(buffer.append(unsupported.episodeId, unsupported.observation, unsupported.action, unsupported.reward,
                       unsupported.nextObservation, unsupported.result),
         "invalid-batch test appends an unsupported transition");

  ai::TrainingSample samples[2] {};
  const auto encoded = ai::encodeTrainingBuffer(buffer, samples, 2);

  expect(encoded.error == ai::TrainingSampleEncodeError::UnsupportedAction,
         "batch encoder reports the first transition encoding error");
  expect(encoded.failedIndex == 1, "batch encoder reports the failed transition index");
  expect(encoded.count == 1, "batch encoder reports successfully encoded samples before failure");
  expect(samples[0].action.targetNode == 41, "batch encoder keeps the successfully encoded prefix");
}

AI_TEST(testTrainingDatasetWriter) {
  const char *path = "aipb-training-test.jsonl";

  ai::TrainingBuffer buffer {};
  const auto episode = buffer.beginEpisode();
  const auto action = makeMoveAction(41);
  expect(buffer.append(episode, makeObservation(1.0f, 40), action, 1.0f,
                       makeObservation(1.5f, 41), makeCompletedResult(action.type)),
         "dataset writer test appends a transition");

  const auto written = ai::writeTrainingDataset(buffer, path);

  expect(written.isValid(), "dataset writer writes a valid buffer");
  expect(written.count == 1, "dataset writer reports the number of written samples");

  std::FILE *file = std::fopen(path, "rb");
  expect(file != nullptr, "dataset writer creates the output file");

  if (file != nullptr) {
    std::fseek(file, 0, SEEK_END);
    const auto length = std::ftell(file);
    std::fclose(file);
    expect(length > 0, "dataset writer produces non-empty output");
  }

  std::remove(path);
}

AI_TEST(testTrainingDatasetWriterRejectsInvalidPath) {
  ai::TrainingBuffer buffer {};
  const auto written = ai::writeTrainingDataset(buffer, nullptr);

  expect(written.error == ai::TrainingDatasetWriteError::InvalidPath,
         "dataset writer rejects a null output path");
}

AI_TEST(testTrainingDatasetWriterRejectsUnsupportedTransition) {
  ai::TrainingBuffer buffer {};
  const auto episode = buffer.beginEpisode();

  ai::Action action {};
  action.type = ai::ActionType::Count;
  ai::ActionResult result {};
  result.action = action.type;
  result.type = ai::ActionResultType::Completed;

  expect(buffer.append(episode, makeObservation(1.0f, 40), action, 0.0f,
                       makeObservation(1.5f, 40), result),
         "dataset writer test appends an unsupported transition");

  const char *path = "aipb-training-invalid-test.jsonl";
  const auto written = ai::writeTrainingDataset(buffer, path);

  expect(written.error == ai::TrainingDatasetWriteError::UnsupportedAction,
         "dataset writer reports unsupported action encoding");
  expect(written.count == 0, "dataset writer reports no samples after the first encoding failure");

  std::remove(path);
}
