//
// AiPB - bounded wall-clock budget for operator-armed Model I/O sessions.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>
#include <limits>

namespace ai::runtime_io {

constexpr uint32_t kModelIOSessionMinSeconds = 5;
constexpr uint32_t kModelIOSessionMaxSeconds = 30;

class ModelIOSessionBudget final {
private:
  uint64_t m_started {};
  uint64_t m_deadline {};
  uint32_t m_seconds {};
  bool m_active {};

public:
  bool arm(uint32_t seconds, uint64_t monotonicMs) {
    if (m_active || seconds < kModelIOSessionMinSeconds ||
        seconds > kModelIOSessionMaxSeconds ||
        monotonicMs > std::numeric_limits<uint64_t>::max() -
            static_cast<uint64_t>(seconds) * 1000) {
      return false;
    }
    m_started = monotonicMs;
    m_deadline = monotonicMs + static_cast<uint64_t>(seconds) * 1000;
    m_seconds = seconds;
    m_active = true;
    return true;
  }

  bool expired(uint64_t monotonicMs) const {
    return !m_active || monotonicMs < m_started || monotonicMs >= m_deadline;
  }

  uint64_t elapsed(uint64_t monotonicMs) const {
    return m_active && monotonicMs >= m_started
        ? monotonicMs - m_started : 0;
  }

  bool active() const { return m_active; }
  uint32_t seconds() const { return m_seconds; }

  void reset() {
    m_started = 0;
    m_deadline = 0;
    m_seconds = 0;
    m_active = false;
  }
};

} // namespace ai::runtime_io
