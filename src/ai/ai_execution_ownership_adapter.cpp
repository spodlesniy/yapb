//
// AiPB - GoldSrc control ownership and action feedback adapter.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

#include <ai/ai_execution_ownership_adapter.h>
#include <ai/ai_server_state_adapter.h>
#include <ai/ai_yapb_action_execution_context.h>

namespace ai::runtime_io {
namespace {

ExecutionOwnershipLedger g_ownership {};

ControlOwner ownerForMode(ai::ControlMode mode) {
  switch (mode) {
  case ai::ControlMode::Legacy:
    return ControlOwner::Legacy;
  case ai::ControlMode::Neural:
    return ControlOwner::Neural;
  case ai::ControlMode::Training:
    return ControlOwner::Training;
  }
  return ControlOwner::None;
}

} // namespace

bool captureControlFrame(Bot &bot, bool explorerOwnsFrame) {
  ServerBotSnapshot snapshot {};
  if (!readServerBotSnapshot(bot.index(), snapshot)) {
    g_ownership.release(bot.index());
    return false;
  }

  if (explorerOwnsFrame && ai::YaPBActionExecutionContext::runtimeActive(bot)) {
    // The existing Explorer frame suppresses Bot::update; cancel any prior policy action.
    ai::YaPBActionExecutionContext::cancelRuntimeAction(bot);
    if (ai::YaPBActionExecutionContext::runtimeActive(bot)) {
      return false;
    }
  }

  const auto owner = explorerOwnsFrame
      ? ControlOwner::Explorer : ownerForMode(ai::YaPBActionExecutionContext::runtimeMode(bot));
  return g_ownership.observe(bot.index(), snapshot.stamp, owner);
}

void recordBotActionResult(const Bot &bot, const ActionResult &result) {
  ServerBotSnapshot snapshot {};
  if (!readServerBotSnapshot(bot.index(), snapshot)) {
    return;
  }
  const auto owner = ownerForMode(ai::YaPBActionExecutionContext::runtimeMode(bot));
  if (owner != ControlOwner::Neural && owner != ControlOwner::Training) {
    return;
  }
  // A mode change may occur in Bot::update after the frame hook observed Legacy mode.
  if (!g_ownership.observe(bot.index(), snapshot.stamp, owner)) {
    return;
  }
  g_ownership.record(bot.index(), snapshot.stamp, owner, result);
}

bool readControlFrame(int botSlot, ControlFrame &frame) {
  return g_ownership.read(botSlot, frame);
}

bool clearFinishedControlFeedback(int botSlot) {
  return g_ownership.clearFinishedFeedback(botSlot);
}

void releaseControlBot(int botSlot) {
  g_ownership.release(botSlot);
}

void resetControlFrames() {
  g_ownership.reset();
}

} // namespace ai::runtime_io
