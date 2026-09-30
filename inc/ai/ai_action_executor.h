//
// AiPB - AI action executor interface.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_result.h>

namespace ai {

// Translates an engine-independent AI action into the active runtime.
// Concrete implementations own the GoldSrc/YaPB integration; the interface
// itself has no dependency on engine types.
class ActionExecutor {
public:
  virtual ActionResult execute(const Action &action, const Observation &observation) const = 0;
};

} // namespace ai
