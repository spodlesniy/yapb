//
// AiPB - engine-neutral per-bot control ownership and action feedback.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_server_state.h>

namespace ai::runtime_io {

enum class ControlOwner : uint8_t {
  None,
  Legacy,
  Neural,
  Training,
  Explorer,
};

// Read-only copy of who may command this bot in the current engine frame.
struct ControlFrame {
  FrameStamp stamp {};
  ControlOwner owner { ControlOwner::None };
  bool hasPendingAction {};
  bool hasFeedback {};
  ActionFeedback feedback {};
};

// Ownership records describe the existing execution path, not a second action scheduler.
class ExecutionOwnershipLedger final {
private:
  struct Slot {
    ControlFrame frame {};
    ActionType pendingType { ActionType::None };
    uint64_t decisionFrameId {};
    bool valid {};
  };
  Slot m_slots[kServerBotSlots] {};

  static bool validSlot(int slot) {
    return slot >= 0 && slot < kServerBotSlots;
  }

  static bool policyOwner(ControlOwner owner) {
    return owner == ControlOwner::Neural || owner == ControlOwner::Training;
  }

public:
  bool observe(int slot, const FrameStamp &stamp, ControlOwner owner) {
    if (!validSlot(slot) || owner == ControlOwner::None ||
        stamp.botEntityIndex != slot + 1 || validateFrame(stamp) != FrameValidation::Valid) {
      return false;
    }
    auto &stored = m_slots[slot];
    const bool newSession = !stored.valid || stored.frame.stamp.sessionId != stamp.sessionId;
    if (!newSession && (stamp.frameId < stored.frame.stamp.frameId ||
                        stamp.gameTime < stored.frame.stamp.gameTime ||
                        stamp.roundId < stored.frame.stamp.roundId)) {
      return false;
    }
    if (stored.valid && stamp.sessionId < stored.frame.stamp.sessionId) {
      return false;
    }
    if (newSession) {
      stored = {};
      stored.valid = true;
    }
    else if (stored.frame.hasPendingAction && stored.frame.owner != owner) {
      // A live action must be cancelled before Explorer or another policy takes control.
      return false;
    }
    stored.frame.stamp = stamp;
    stored.frame.owner = owner;
    return true;
  }

  bool record(int slot, const FrameStamp &observed, ControlOwner source, const ActionResult &result) {
    if (!validSlot(slot) || !policyOwner(source) || result.action == ActionType::None ||
        (result.type != ActionResultType::Accepted && !result.isTerminal()) ||
        !std::isfinite(result.elapsedTime) || result.elapsedTime < 0.0f ||
        observed.botEntityIndex != slot + 1 || validateFrame(observed) != FrameValidation::Valid) {
      return false;
    }
    auto &stored = m_slots[slot];
    if (!stored.valid || stored.frame.owner != source ||
        stored.frame.stamp.sessionId != observed.sessionId ||
        observed.frameId < stored.frame.stamp.frameId ||
        observed.gameTime < stored.frame.stamp.gameTime ||
        (stored.frame.hasPendingAction && stored.pendingType != result.action)) {
      return false;
    }
    if (!stored.frame.hasPendingAction && result.type == ActionResultType::Accepted) {
      stored.decisionFrameId = observed.frameId;
      stored.pendingType = result.action;
      stored.frame.hasPendingAction = true;
    }

    ActionFeedback feedback {};
    feedback.sessionId = observed.sessionId;
    feedback.decisionFrameId = stored.frame.hasPendingAction ? stored.decisionFrameId : observed.frameId;
    feedback.observedFrameId = observed.frameId;
    feedback.result = result;
    stored.frame.feedback = feedback;
    stored.frame.hasFeedback = true;
    if (result.isTerminal()) {
      stored.frame.hasPendingAction = false;
      stored.pendingType = ActionType::None;
      stored.decisionFrameId = 0;
    }
    return true;
  }

  bool read(int slot, ControlFrame &frame) const {
    if (!validSlot(slot) || !m_slots[slot].valid) {
      return false;
    }
    frame = m_slots[slot].frame;
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
