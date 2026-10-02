//
// AiPB - model training sample encoder implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_inference_action_encoder.h>
#include <ai/ai_training_sample.h>

namespace ai {

TrainingSampleEncodeResult encodeTrainingTransition(const TrainingTransition &transition) {
  TrainingSampleEncodeResult result {};

  if (transition.episodeId == 0) {
    result.error = TrainingSampleEncodeError::InvalidEpisode;
    return result;
  }

  const auto encodedAction = encodeInferenceAction(transition.action);
  if (!encodedAction.isValid()) {
    result.error = TrainingSampleEncodeError::UnsupportedAction;
    return result;
  }

  result.sample.episodeId = transition.episodeId;
  result.sample.observation = encodeInferenceFeatures(transition.observation);
  result.sample.action = encodedAction.output;
  result.sample.reward = transition.reward;
  result.sample.nextObservation = encodeInferenceFeatures(transition.nextObservation);
  result.sample.result = transition.result.type;
  result.sample.elapsedTime = transition.result.elapsedTime;
  result.sample.terminal = transition.result.isTerminal();

  return result;
}


TrainingBatchEncodeResult encodeTrainingBuffer(const TrainingBuffer &buffer, TrainingSample *samples, size_t capacity) {
  TrainingBatchEncodeResult result {};
  const auto count = buffer.size();

  if (count == 0) {
    return result;
  }

  if (samples == nullptr) {
    result.error = TrainingSampleEncodeError::NullOutputBuffer;
    return result;
  }

  if (capacity < count) {
    result.error = TrainingSampleEncodeError::OutputBufferTooSmall;
    return result;
  }

  for (size_t i = 0; i < count; ++i) {
    const auto encoded = encodeTrainingTransition(buffer.at(i));

    if (!encoded.isValid()) {
      result.failedIndex = i;
      result.error = encoded.error;
      return result;
    }

    samples[i] = encoded.sample;
    result.count = i + 1;
  }

  return result;
}

} // namespace ai
