//
// AiPB - model training sample encoder.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>

#include <ai/ai_inference_action.h>
#include <ai/ai_inference_features.h>
#include <ai/ai_training_recorder.h>

namespace ai {

enum class TrainingSampleEncodeError : uint8_t {
  None,
  InvalidEpisode,
  UnsupportedAction,
};

struct TrainingSample {
  uint64_t episodeId {};
  InferenceFeatures observation {};
  InferenceActionOutput action {};
  float reward {};
  InferenceFeatures nextObservation {};
  ActionResultType result { ActionResultType::None };
  float elapsedTime {};
  bool terminal {};

  bool hasSupportedSchema() const {
    return observation.hasSupportedSchema() && action.hasSupportedSchema() && nextObservation.hasSupportedSchema();
  }
};

struct TrainingSampleEncodeResult {
  TrainingSample sample {};
  TrainingSampleEncodeError error { TrainingSampleEncodeError::None };

  bool isValid() const {
    return error == TrainingSampleEncodeError::None;
  }
};

TrainingSampleEncodeResult encodeTrainingTransition(const TrainingTransition &transition);

} // namespace ai
