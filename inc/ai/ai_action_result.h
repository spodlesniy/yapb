//
// AiPB - AI action result abstraction.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action.h>

#include <cstdint>

namespace ai {

enum class ActionResultType : uint8_t {
  None,
  Accepted,
  Completed,
  Rejected,
  Invalid,
  Failed,
  Interrupted,
};

struct ActionResult {
  ActionType action { ActionType::None };
  ActionResultType type { ActionResultType::None };
  float elapsedTime {};

  bool isTerminal() const {
    return type == ActionResultType::Completed || type == ActionResultType::Rejected || type == ActionResultType::Invalid ||
      type == ActionResultType::Failed || type == ActionResultType::Interrupted;
  }
};

} // namespace ai
