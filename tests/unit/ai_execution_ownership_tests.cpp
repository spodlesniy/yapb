//
// AiPB - execution ownership and action feedback tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_execution_ownership.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::FrameStamp stamp(uint64_t session, uint64_t frame, int slot = 0) {
  io::FrameStamp result {};
  result.sessionId = session;
  result.frameId = frame;
  result.roundId = 1;
  result.botEntityIndex = slot + 1;
  result.gameTime = static_cast<float>(frame);
  result.frameSeconds = 0.01f;
  return result;
}

ai::ActionResult result(ai::ActionResultType type, ai::ActionType action = ai::ActionType::MoveToPosition) {
  ai::ActionResult r {};
  r.action = action;
  r.type = type;
  r.elapsedTime = 0.1f;
  return r;
}

} // namespace

AI_TEST(testExecutionOwnershipSwitchRequiresCancellation) {
  io::ExecutionOwnershipLedger ledger {};
  io::ControlFrame view {};
  expect(ledger.observe(0, stamp(1, 1), io::ControlOwner::Neural), "neural claims current frame");
  expect(ledger.record(0, stamp(1, 1), io::ControlOwner::Neural, result(ai::ActionResultType::Accepted)),
         "policy action begins");
  expect(!ledger.observe(0, stamp(1, 2), io::ControlOwner::Explorer),
         "explorer cannot replace a live policy action");
  expect(ledger.record(0, stamp(1, 2), io::ControlOwner::Neural, result(ai::ActionResultType::Interrupted)),
         "cancellation observed before explorer handoff");
  expect(ledger.observe(0, stamp(1, 2), io::ControlOwner::Explorer), "explorer can own after cancellation");
  expect(ledger.read(0, view) && view.owner == io::ControlOwner::Explorer && !view.hasPendingAction,
         "exactly one owner after handoff");
  expect(view.hasFeedback && view.feedback.decisionFrameId == 1 && view.feedback.observedFrameId == 2,
         "interruption linked to original decision");
  expect(!ledger.record(0, stamp(1, 2), io::ControlOwner::Neural, result(ai::ActionResultType::Accepted)),
         "previous controller cannot submit while explorer owns");
}

AI_TEST(testExecutionFeedbackTerminalAndOneFrameActions) {
  io::ExecutionOwnershipLedger ledger {};
  io::ControlFrame view {};
  expect(ledger.observe(1, stamp(2, 10, 1), io::ControlOwner::Training), "training frame");
  expect(ledger.record(1, stamp(2, 10, 1), io::ControlOwner::Training, result(ai::ActionResultType::Accepted)),
         "accepted result");
  expect(ledger.observe(1, stamp(2, 11, 1), io::ControlOwner::Training), "next frame still training");
  expect(ledger.record(1, stamp(2, 11, 1), io::ControlOwner::Training, result(ai::ActionResultType::Completed)),
         "terminal completion");
  expect(ledger.read(1, view) && view.feedback.decisionFrameId == 10 &&
         view.feedback.observedFrameId == 11 && !view.hasPendingAction,
         "terminal result preserves decision frame");
  expect(ledger.record(1, stamp(2, 11, 1), io::ControlOwner::Training, result(ai::ActionResultType::Failed)),
         "one-frame terminal action accepted");
  expect(ledger.read(1, view) && view.feedback.decisionFrameId == 11, "one-frame terminal anchored to current frame");
}

AI_TEST(testExecutionOwnershipFrameIdentityAndDisconnection) {
  io::ExecutionOwnershipLedger ledger {};
  io::ControlFrame view {};
  expect(!ledger.observe(0, stamp(0, 1), io::ControlOwner::Legacy), "missing session denied");
  expect(!ledger.observe(0, stamp(1, 1), io::ControlOwner::None), "missing owner denied");
  expect(!ledger.observe(1, stamp(1, 1), io::ControlOwner::Legacy), "wrong slot denied");
  expect(ledger.observe(0, stamp(5, 1), io::ControlOwner::Legacy), "legacy starts");
  expect(!ledger.observe(0, stamp(4, 2), io::ControlOwner::Legacy), "old session denied");
  expect(!ledger.observe(0, stamp(5, 0), io::ControlOwner::Legacy), "bad frame denied");
  expect(ledger.observe(0, stamp(5, 2), io::ControlOwner::Legacy), "next frame");
  expect(!ledger.observe(0, stamp(5, 1), io::ControlOwner::Legacy), "duplicate old frame denied");
  ledger.release(0);
  expect(!ledger.read(0, view), "disconnect invalidates ownership");
  expect(ledger.observe(0, stamp(6, 1), io::ControlOwner::Explorer), "new session after disconnect");
  ledger.reset();
  expect(!ledger.read(0, view), "map reset removes ownership");
}

AI_TEST(testExecutionOwnershipPerBotIsolation) {
  io::ExecutionOwnershipLedger ledger {};
  io::ControlFrame view {};
  expect(ledger.observe(0, stamp(1, 1), io::ControlOwner::Explorer), "explorer one");
  expect(ledger.observe(1, stamp(2, 1, 1), io::ControlOwner::Neural), "neural two");
  expect(ledger.read(0, view) && view.owner == io::ControlOwner::Explorer, "bot zero explorer");
  expect(ledger.read(1, view) && view.owner == io::ControlOwner::Neural, "bot one neural");
  expect(!ledger.record(0, stamp(1, 1), io::ControlOwner::Explorer, result(ai::ActionResultType::Completed)),
         "explorer does not masquerade as policy action");
  expect(!ledger.record(0, stamp(1, 1), io::ControlOwner::Legacy, result(ai::ActionResultType::Accepted)),
         "legacy does not forge neural results");
}

AI_TEST(testExecutionFeedbackRejectsInvalidOrWrongAction) {
  io::ExecutionOwnershipLedger ledger {};
  io::ControlFrame view {};
  expect(ledger.observe(0, stamp(1, 1), io::ControlOwner::Neural), "initial owner");
  auto accepted = result(ai::ActionResultType::Accepted);
  expect(ledger.record(0, stamp(1, 1), io::ControlOwner::Neural, accepted), "first action accepted");
  auto different = result(ai::ActionResultType::Completed, ai::ActionType::AttackTarget);
  expect(!ledger.record(0, stamp(1, 2), io::ControlOwner::Neural, different),
         "other action cannot complete active action");
  auto none = result(ai::ActionResultType::None);
  expect(!ledger.record(0, stamp(1, 2), io::ControlOwner::Neural, none), "none is not feedback");
  auto wrongSession = stamp(2, 2);
  expect(!ledger.record(0, wrongSession, io::ControlOwner::Neural, accepted), "other session cannot finish action");
  auto invalid = result(ai::ActionResultType::Completed);
  invalid.elapsedTime = -1.0f;
  expect(!ledger.record(0, stamp(1, 2), io::ControlOwner::Neural, invalid),
         "negative elapsed time rejected");
  expect(ledger.read(0, view) && view.hasPendingAction, "invalid updates do not cancel pending action");
}
