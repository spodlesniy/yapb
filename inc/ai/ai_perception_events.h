//
// AiPB - bounded, engine-neutral perception and event snapshots.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_server_state.h>

#include <cstdint>

namespace ai::runtime_io {

constexpr uint8_t kPerceptionEventsPerFrame = 4;

struct PerceptionSignals {
  bool visibleEnemy {};
  int32_t enemyEntityIndex { -1 };
  Vec3 seenPosition {};
  bool heardSound {};
  Vec3 soundPosition {};
  bool roundOver {};
};

struct VisibleEnemy {
  int32_t entityIndex { -1 };
  Vec3 position {};
  bool valid {};
};

struct PerceptionEvent {
  WorldEvent event {};
  bool hasPosition {};
};

struct PerceptionFrame {
  ServerBotSnapshot self {};
  VisibleEnemy visibleEnemy {};
  PerceptionEvent events[kPerceptionEventsPerFrame] {};
  uint8_t eventCount {};
};

// Only authorized signals can enter this model-facing cache.
// The game adapter is responsible for validating visibility and audible origin.
class PerceptionFrameCache final {
private:
  struct Slot {
    PerceptionFrame frame {};
    int32_t lastVisibleEnemy { -1 };
    bool hearing {};
    bool roundOver {};
    bool valid {};
  };

  Slot m_slots[kServerBotSlots] {};

  static void addEvent(PerceptionFrame &frame, EventKind kind, Evidence evidence, int32_t subject,
                       bool hasPosition = false, Vec3 position = {}) {
    if (frame.eventCount >= kPerceptionEventsPerFrame) {
      return;
    }

    auto &entry = frame.events[frame.eventCount++];
    entry.event.stamp = frame.self.stamp;
    entry.event.kind = kind;
    entry.event.evidence = evidence;
    entry.event.subjectEntityIndex = subject;
    entry.hasPosition = hasPosition;
    entry.event.position = hasPosition ? position : Vec3 {};
  }

public:
  bool update(int slotIndex, const ServerBotSnapshot &snapshot, const PerceptionSignals &signals) {
    if (slotIndex < 0 || slotIndex >= kServerBotSlots) {
      return false;
    }

    auto &slot = m_slots[slotIndex];
    const bool freshSession = !slot.valid || slot.frame.self.stamp.sessionId != snapshot.stamp.sessionId;
    const FrameStamp *previous = freshSession ? nullptr : &slot.frame.self.stamp;

    if (snapshot.stamp.botEntityIndex != slotIndex + 1 ||
        validateFrame(snapshot.stamp, previous) != FrameValidation::Valid ||
        !validSelfState(snapshot.self) ||
        (!freshSession && snapshot.stamp.sessionId < slot.frame.self.stamp.sessionId)) {
      slot = {};
      return false;
    }

    const bool seen = signals.visibleEnemy && signals.enemyEntityIndex > 0 &&
                      signals.enemyEntityIndex != snapshot.stamp.botEntityIndex &&
                      finiteVector(signals.seenPosition);
    const bool heard = signals.heardSound && finiteVector(signals.soundPosition);
    const int32_t visibleIndex = seen ? signals.enemyEntityIndex : -1;

    PerceptionFrame next {};
    next.self = snapshot;
    if (seen) {
      next.visibleEnemy = { visibleIndex, signals.seenPosition, true };
    }

    if (snapshot.stamp.roundId != 0 &&
        (freshSession || snapshot.stamp.roundId != slot.frame.self.stamp.roundId ||
         signals.roundOver != slot.roundOver)) {
      addEvent(next, signals.roundOver ? EventKind::RoundEnded : EventKind::RoundStarted,
               Evidence::PublicEvent, -1);
    }

    if (!freshSession && slot.lastVisibleEnemy != -1 && slot.lastVisibleEnemy != visibleIndex) {
      // Never attach a hidden entity's live position to a disappearance event.
      addEvent(next, EventKind::EntityDisappeared, Evidence::Remembered, slot.lastVisibleEnemy);
    }
    if (seen && (freshSession || slot.lastVisibleEnemy != visibleIndex)) {
      addEvent(next, EventKind::EntityAppeared, Evidence::Seen, visibleIndex, true, signals.seenPosition);
    }
    if (heard && (freshSession || !slot.hearing)) {
      // A sound is an anonymous audible location, not a live enemy position.
      addEvent(next, EventKind::Sound, Evidence::Heard, -1, true, signals.soundPosition);
    }

    slot.frame = next;
    slot.lastVisibleEnemy = visibleIndex;
    slot.hearing = heard;
    slot.roundOver = signals.roundOver;
    slot.valid = true;
    return true;
  }

  bool read(int slotIndex, PerceptionFrame &frame) const {
    if (slotIndex < 0 || slotIndex >= kServerBotSlots || !m_slots[slotIndex].valid) {
      return false;
    }
    frame = m_slots[slotIndex].frame;
    return true;
  }

  void release(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < kServerBotSlots) {
      m_slots[slotIndex] = {};
    }
  }

  void reset() {
    for (auto &slot : m_slots) {
      slot = {};
    }
  }
};

} // namespace ai::runtime_io
