//
// AiPB - AI model action encoder.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_inference_action.h>

#include <cstdint>

namespace ai {

enum class InferenceActionEncodeError : uint8_t {
  None,
  UnsupportedAction,
};

struct InferenceActionEncodeResult {
  InferenceActionOutput output {};
  InferenceActionEncodeError error { InferenceActionEncodeError::None };

  bool isValid() const {
    return error == InferenceActionEncodeError::None;
  }
};

InferenceActionEncodeResult encodeInferenceAction(const Action &action);

} // namespace ai
