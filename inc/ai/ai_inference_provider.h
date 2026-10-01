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

namespace ai {

// Backend-neutral inference contract.
//
// A provider owns the actual model/runtime interaction. It receives a semantic
// Observation and returns a high-level Action. It must not know about GoldSrc,
// Bot, tasks, navigation, or engine input.
class InferenceProvider {
public:
   virtual ~InferenceProvider () = default;

   virtual Action infer (const Observation &observation) const = 0;
};

} // namespace ai
