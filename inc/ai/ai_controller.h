//
// AiPB - AI controller abstraction.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>

#include <ai/ai_policy.h>

namespace ai {

enum class ControlMode : uint8_t { Legacy, Neural, Training };

// Selects the active policy without exposing GoldSrc or Bot internals to it.
// Runtime integration is handled by BotRuntime; Legacy mode remains
// authoritative until an explicit Neural mode is selected.
class Controller final {
private:
  ControlMode m_mode { ControlMode::Legacy };
  const Policy *m_policy {};

public:
  explicit Controller(ControlMode mode = ControlMode::Legacy);

  void setMode(ControlMode mode) {
    m_mode = mode;
  }

  ControlMode getMode() const {
    return m_mode;
  }

  void setPolicy(const Policy *policy);

  const Policy *getPolicy() const {
    return m_policy;
  }

  Action decide(const Observation &observation) const;
};

} // namespace ai
