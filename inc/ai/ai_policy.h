//
// YaPB - AI policy abstraction.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>
#include <ai/ai_observation.h>

namespace ai {

// A policy maps the current game observation to a high-level bot intent.
//
// Implementations may be deterministic (legacy YaPB), neural (inference),
// or training-oriented. The interface intentionally has no dependency on
// GoldSrc so policies can also be exercised by an external trainer.
class Policy {
public:
   virtual ~Policy () = default;

   virtual Action decide (const Observation &observation) const = 0;
};

} // namespace ai
