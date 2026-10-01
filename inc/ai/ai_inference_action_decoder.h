//
// AiPB - AI model action decoder.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>
#include <ai/ai_action_validator.h>
#include <ai/ai_inference_action.h>

#include <cstdint>

namespace ai {

enum class InferenceActionDecodeError : uint8_t {
   None,
   UnsupportedSchema,
   InvalidActionId,
   InvalidAction,
};

struct InferenceActionDecodeResult {
   Action action {};
   InferenceActionDecodeError error { InferenceActionDecodeError::None };
   ActionValidationError validationError { ActionValidationError::None };

   bool isValid () const {
      return error == InferenceActionDecodeError::None;
   }
};

InferenceActionDecodeResult decodeInferenceAction (
   const InferenceActionOutput &output,
   const Observation &observation
);

} // namespace ai
