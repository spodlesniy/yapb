//
// AiPB - AI inference provider abstraction.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>
#include <ai/ai_observation.h>

#include <cstdint>

namespace ai {

constexpr uint32_t kInferenceSchemaVersion = 1;

struct InferenceInput {
   uint32_t schemaVersion { kInferenceSchemaVersion };
   Observation observation {};

   bool hasSupportedSchema () const {
      return schemaVersion == kInferenceSchemaVersion;
   }
};

enum class InferenceStatus : uint8_t {
   Success,
   NoDecision,
   InvalidInput,
   Error,
};

struct InferenceResult {
   uint32_t schemaVersion { kInferenceSchemaVersion };
   InferenceStatus status { InferenceStatus::NoDecision };
   Action action {};

   bool hasSupportedSchema () const {
      return schemaVersion == kInferenceSchemaVersion;
   }

   bool isSuccess () const {
      return status == InferenceStatus::Success;
   }
};

// Backend-neutral inference contract.
//
// A provider owns the actual model/runtime interaction. It receives a versioned
// semantic input and returns a versioned high-level action result. It must not
// know about GoldSrc, Bot, tasks, navigation, or engine input.
class InferenceProvider {
public:
   virtual ~InferenceProvider () = default;

   virtual InferenceResult infer (const InferenceInput &input) const = 0;
};

} // namespace ai
