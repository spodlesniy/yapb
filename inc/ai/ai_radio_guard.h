// AiPB - NeedBackup radio enqueue cadence.
// SPDX-License-Identifier: MIT
#pragma once

namespace ai {

// DefuseBomb can request backup every frame. Bound enqueue attempts so
// stale radio queue entries do not spill over after the C4 has been defused.
constexpr float kNeedBackupRadioRepeatSeconds = 15.0f;

class NeedBackupRadioGate final {
  float m_nextAllowed {};
public:
  void reset() { m_nextAllowed = 0.0f; }
  bool allow(float now) {
    if (now < m_nextAllowed) return false;
    m_nextAllowed = now + kNeedBackupRadioRepeatSeconds;
    return true;
  }
};

} // namespace ai
