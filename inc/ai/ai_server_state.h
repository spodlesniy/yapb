//
// AiPB - read-only server state snapshot and session tracking.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_runtime_io.h>

#include <cmath>
#include <cstdint>
#include <limits>

namespace ai::runtime_io {

constexpr int kServerBotSlots = 32;

struct BotSelfState {
  Vec3 origin {};
  Vec3 velocity {};
  Vec3 viewAngles {};
  float health {};
  float armor {};
  float maxSpeed {};
  bool alive {};
  bool onGround {};
  bool crouching {};
};

struct ServerBotSnapshot {
  FrameStamp stamp {};
  BotSelfState self {};
};

inline bool finiteVector(const Vec3 &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline bool validSelfState(const BotSelfState &self) {
  return finiteVector(self.origin) && finiteVector(self.velocity) && finiteVector(self.viewAngles) &&
         std::isfinite(self.health) && std::isfinite(self.armor) && std::isfinite(self.maxSpeed) &&
         self.armor >= 0.0f && self.maxSpeed >= 0.0f;
}

// All operations occur on the GoldSrc thread; no entity pointers are retained.
class ServerStateCache final {
private:
  struct Slot {
    uint64_t identity {};
    ServerBotSnapshot snapshot {};
    bool valid {};
  };

  Slot m_slots[kServerBotSlots] {};
  uint64_t m_sessionCounter {};

  static bool validSlot(int slot) {
    return slot >= 0 && slot < kServerBotSlots;
  }

public:
  bool capture(int slot, uint64_t identity, uint64_t roundId, float gameTime, float frameSeconds,
               const BotSelfState &self) {
    if (!validSlot(slot)) {
      return false;
    }

    auto &stored = m_slots[slot];
    if (identity == 0 || !validSelfState(self)) {
      stored = {};
      return false;
    }

    const bool newIdentity = !stored.valid || stored.identity != identity;
    if ((newIdentity && m_sessionCounter == std::numeric_limits<uint64_t>::max()) ||
        (!newIdentity && stored.snapshot.stamp.frameId == std::numeric_limits<uint64_t>::max())) {
      stored = {};
      return false;
    }

    FrameStamp stamp {};
    stamp.sessionId = newIdentity ? m_sessionCounter + 1 : stored.snapshot.stamp.sessionId;
    stamp.frameId = newIdentity ? 1 : stored.snapshot.stamp.frameId + 1;
    stamp.roundId = roundId;
    stamp.gameTime = gameTime;
    stamp.frameSeconds = frameSeconds;
    stamp.botEntityIndex = slot + 1;

    if (validateFrame(stamp, newIdentity ? nullptr : &stored.snapshot.stamp) != FrameValidation::Valid) {
      stored = {};
      return false;
    }

    if (newIdentity) {
      m_sessionCounter = stamp.sessionId;
    }
    stored.snapshot.stamp = stamp;
    stored.snapshot.self = self;
    stored.identity = identity;
    stored.valid = true;
    return true;
  }

  bool read(int slot, ServerBotSnapshot &snapshot) const {
    if (!validSlot(slot) || !m_slots[slot].valid) {
      return false;
    }
    snapshot = m_slots[slot].snapshot;
    return true;
  }

  void release(int slot) {
    if (validSlot(slot)) {
      m_slots[slot] = {};
    }
  }

  void reset() {
    for (auto &slot : m_slots) {
      slot = {};
    }
  }
};

} // namespace ai::runtime_io
