//
// AiPB - bounded progress ledger for manually armed Model I/O sequences.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace ai::runtime_io {

constexpr uint8_t kModelIOSequenceMinLegs = 2;
constexpr uint8_t kModelIOSequenceMaxLegs = 3;

// Tracks verified independent movement legs without owning an engine entity,
// policy, backend or socket. A leg counts only after physical completion and
// full D324 provider/fallback recovery.
class ModelIOSequenceProgress final {
private:
  uint8_t m_total {};
  uint8_t m_completed {};
  uint64_t m_session {};
  uint32_t m_requests {};
  float m_distance {};

public:
  void reset() {
    m_total = 0;
    m_completed = 0;
    m_session = 0;
    m_requests = 0;
    m_distance = 0.0f;
  }

  bool arm(uint8_t legs) {
    if (legs < kModelIOSequenceMinLegs || legs > kModelIOSequenceMaxLegs) {
      return false;
    }
    reset();
    m_total = legs;
    return true;
  }

  bool bindSession(uint64_t session) {
    if (m_total == 0 || session == 0) {
      return false;
    }
    if (m_session == 0) {
      m_session = session;
    }
    return m_session == session;
  }

  bool completeLeg(uint64_t session, bool reached, bool recovered,
                   uint32_t requests, float moved) {
    if (m_total == 0 || m_session == 0 || m_session != session ||
        m_completed >= m_total || !reached || !recovered || requests < 2 ||
        !std::isfinite(moved) || moved < 0.0f ||
        !std::isfinite(m_distance + moved) ||
        m_requests > std::numeric_limits<uint32_t>::max() - requests) {
      return false;
    }
    ++m_completed;
    m_requests += requests;
    m_distance += moved;
    return true;
  }

  bool hasNext() const { return m_total > 0 && m_completed < m_total; }
  bool finished() const { return m_total > 0 && m_completed == m_total; }
  uint8_t total() const { return m_total; }
  uint8_t completed() const { return m_completed; }
  uint64_t session() const { return m_session; }
  uint32_t requests() const { return m_requests; }
  float distance() const { return m_distance; }
};

} // namespace ai::runtime_io
