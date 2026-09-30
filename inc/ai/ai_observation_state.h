//
// AiPB - AI observation runtime state.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>

namespace ai {

class ObservationState final {
private:
  uint64_t m_sequence {};
  bool m_valid {};

public:
  void invalidate() {
    m_valid = false;
  }

  void markUpdated() {
    ++m_sequence;
    m_valid = true;
  }

  bool isValid() const {
    return m_valid;
  }

  uint64_t sequence() const {
    return m_sequence;
  }
};

} // namespace ai
