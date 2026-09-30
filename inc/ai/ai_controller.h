//
// YaPB - AI controller abstraction.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>

#include <ai/ai_policy.h>

namespace ai {

enum class ControlMode : uint8_t {
   Legacy,
   Neural,
   Training
};

// Selects the active policy without exposing GoldSrc or Bot internals to it.
//
// Phase 1 intentionally keeps the controller detached from Bot::logic().
// This establishes the seam for later integration while guaranteeing that
// adding the AI subsystem cannot change current bot behavior.
class Controller final {
private:
   ControlMode m_mode { ControlMode::Legacy };
   const Policy *m_policy {};

public:
   explicit Controller (ControlMode mode = ControlMode::Legacy);

   void setMode (ControlMode mode) {
      m_mode = mode;
   }

   ControlMode getMode () const {
      return m_mode;
   }

   void setPolicy (const Policy *policy);

   const Policy *getPolicy () const {
      return m_policy;
   }

   Action decide (const Observation &observation) const;
};

} // namespace ai
