//
// AiPB - model training sample encoder implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

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

} // namespace ai
