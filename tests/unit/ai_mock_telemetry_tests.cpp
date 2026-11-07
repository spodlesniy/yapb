//
// AiPB - bounded read-only Mock trace and replay tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_mock_telemetry.h>

using ai::test::expect;
namespace io = ai::runtime_io;
namespace mock = ai::mock;

namespace {

io::ServerBotSnapshot state(uint64_t frame, float time, float x = 0.0f) {
  io::ServerBotSnapshot snapshot {};
  snapshot.stamp.sessionId = 3;
  snapshot.stamp.frameId = frame;
  snapshot.stamp.roundId = 2;
  snapshot.stamp.botEntityIndex = 2;
  snapshot.stamp.gameTime = time;
  snapshot.stamp.frameSeconds = 0.05f;
  snapshot.self.origin = { x, 0.0f, 132.0f };
  snapshot.self.velocity = { 100.0f, 0.0f, 0.0f };
  snapshot.self.maxSpeed = 250.0f;
  snapshot.self.health = 100.0f;
  snapshot.self.alive = true;
  snapshot.self.onGround = true;
  return snapshot;
}

ai::ActionResult outcome(ai::ActionResultType type) {
  return { ai::ActionType::MoveToPosition, type, 0.05f };
}

io::ActionFeedback feedback(uint64_t observedFrame, uint64_t decisionFrame,
                            ai::ActionResultType type) {
  io::ActionFeedback value {};
  value.sessionId = 3;
  value.decisionFrameId = decisionFrame;
  value.observedFrameId = observedFrame;
  value.result = outcome(type);
  return value;
}

} // namespace

AI_TEST(testMockReplayAcceptsMeasuredCompletionWithoutActuation) {
  mock::MockTrace trace {};
  expect(trace.begin(state(10, 1.0f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "authorized geometry preflight creates recording");
  const auto f11 = feedback(11, 11, ai::ActionResultType::Accepted);
  const auto f12 = feedback(12, 11, ai::ActionResultType::Accepted);
  const auto f13 = feedback(13, 11, ai::ActionResultType::Completed);
  expect(trace.append(state(11, 1.1f, 20.0f), outcome(ai::ActionResultType::Accepted), &f11),
         "first frame decision captured");
  expect(trace.append(state(12, 1.2f, 60.0f), outcome(ai::ActionResultType::Accepted), &f12),
         "progress captured");
  expect(trace.append(state(13, 1.3f, 86.0f), outcome(ai::ActionResultType::Completed), &f13),
         "terminal frame captured");
  const auto report = trace.replay();
  expect(report.status == mock::ReplayStatus::Pass, "deterministic audit matches recorded events");
  expect(report.frames == 4 && report.decisionFrameId == 11 && report.terminalFrameId == 13,
         "replay reports decision and terminal frames");
  expect(report.terminalResult == ai::ActionResultType::Completed, "actual outcome preserved");
  expect(report.displacement == 86.0f && report.remainingDistance == 10.0f,
         "physical displacement remains distinct from target error");
  expect(trace.record(0)->observed.stamp.frameId == 10, "initial self snapshot preserved");
  expect(trace.record(5) == nullptr, "out-of-range records are inaccessible");
  expect(trace.replay().status == mock::ReplayStatus::Pass,
         "replay is repeatable and does not mutate the trace");
}

AI_TEST(testMockReplayRejectsFalsePhysicalCompletion) {
  mock::MockTrace trace {};
  expect(trace.begin(state(10, 1.0f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "recording starts");
  const auto accepted = feedback(11, 11, ai::ActionResultType::Accepted);
  const auto completed = feedback(12, 11, ai::ActionResultType::Completed);
  expect(trace.append(state(11, 1.1f, 20.0f), outcome(ai::ActionResultType::Accepted), &accepted),
         "decision recorded");
  expect(trace.append(state(12, 1.2f, 57.11f), outcome(ai::ActionResultType::Completed), &completed),
         "terminal recorded");
  expect(trace.replay().status == mock::ReplayStatus::InvalidCompletion,
         "premature waypoint-like Completed never passes physical replay");
}

AI_TEST(testMockReplayRequiresFeedbackAndMatchingIdentity) {
  mock::MockTrace trace {};
  expect(trace.begin(state(1, 0.1f), { 20.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "recording starts");
  expect(trace.append(state(2, 0.2f, 10.0f), outcome(ai::ActionResultType::Accepted), nullptr),
         "absence of feedback recorded without fabricating evidence");
  const auto completed = feedback(3, 2, ai::ActionResultType::Completed);
  expect(trace.append(state(3, 0.3f, 20.0f), outcome(ai::ActionResultType::Completed), &completed),
         "terminal recorded");
  expect(trace.replay().status == mock::ReplayStatus::MissingFeedback,
         "missing D319 evidence cannot be replayed as success");

  trace.reset();
  expect(trace.begin(state(1, 0.1f), { 20.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "fresh trace begins");
  auto wrong = feedback(2, 99, ai::ActionResultType::Accepted);
  expect(trace.append(state(2, 0.2f, 10.0f), outcome(ai::ActionResultType::Accepted), &wrong),
         "wrong decision ID recorded");
  expect(trace.append(state(3, 0.3f, 20.0f), outcome(ai::ActionResultType::Completed), &completed),
         "terminal recorded");
  expect(trace.replay().status == mock::ReplayStatus::InvalidFeedback,
         "cross-decision feedback cannot pass replay");
}

AI_TEST(testMockReplayFailsClosedOnStaleFrameAndOverflow) {
  mock::MockTrace trace {};
  expect(!trace.begin(state(1, 0.1f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Rejected),
         "rejected geometry cannot authorize recording");
  expect(trace.replay().status == mock::ReplayStatus::Empty, "invalid begin has no trace");
  expect(trace.begin(state(1, 0.1f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "valid trace begins");
  auto accepted = feedback(2, 2, ai::ActionResultType::Accepted);
  expect(trace.append(state(2, 0.2f, 10.0f), outcome(ai::ActionResultType::Accepted), &accepted),
         "first observed action");
  expect(!trace.append(state(2, 0.2f, 10.0f), outcome(ai::ActionResultType::Accepted), &accepted),
         "duplicate frame rejected");
  expect(trace.replay().status == mock::ReplayStatus::InvalidFrame,
         "stale data invalidates replay instead of disappearing");
  trace.reset();
  expect(trace.begin(state(1, 0.1f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "new capture starts");
  for (size_t i = 1; i < mock::kMockTraceCapacity + 120; ++i) {
    const auto frame = static_cast<uint64_t>(i + 1);
    const auto current = feedback(frame, 2, ai::ActionResultType::Accepted);
    expect(trace.append(state(frame, static_cast<float>(frame) * 0.1f),
                        outcome(ai::ActionResultType::Accepted), &current),
           "continuous frames do not exhaust reserved terminal slot");
  }
  const auto finalFrame = static_cast<uint64_t>(mock::kMockTraceCapacity + 121);
  const auto completed = feedback(finalFrame, 2, ai::ActionResultType::Completed);
  expect(trace.append(state(finalFrame, static_cast<float>(finalFrame) * 0.1f, 90.0f),
                      outcome(ai::ActionResultType::Completed), &completed),
         "terminal frame remains available after trace saturation");
  const auto report = trace.replay();
  expect(report.status == mock::ReplayStatus::Pass, "reserved terminal frame remains replayable");
  expect(report.frames == mock::kMockTraceCapacity, "trace capacity cannot be exceeded");
  expect(report.capacitySkipped > 0 && report.skippedFrames > 0,
         "capacity pressure explicitly reported");
  expect(report.observedFrames > report.frames, "processed and saved counters differ");

}

AI_TEST(testMockReplayIncompleteAndCancelledNeverPass) {
  mock::MockTrace trace {};
  expect(trace.begin(state(10, 1.0f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "begin accepted");
  expect(trace.replay().status == mock::ReplayStatus::Incomplete, "unresolved action cannot pass");
  const auto f11 = feedback(11, 11, ai::ActionResultType::Accepted);
  expect(trace.append(state(11, 1.1f, 5.0f), outcome(ai::ActionResultType::Accepted), &f11),
         "action recorded");
  trace.cancel();
  expect(trace.replay().status == mock::ReplayStatus::Cancelled, "cancel recorded distinctly");
  expect(!trace.append(state(12, 1.2f), outcome(ai::ActionResultType::Completed), &f11),
         "no appends after cancellation");
}

AI_TEST(testMockTelemetryHighFpsSamplingPreservesDecisionAndTerminal) {
  mock::MockTrace trace {};
  expect(trace.setSampleIntervalSeconds(0.025f), "25 millisecond sampling supported");
  expect(trace.begin(state(1, 0.0f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "initial self state retained");
  for (uint64_t frame = 2; frame < 802; ++frame) {
    const float x = static_cast<float>(frame - 1) * 0.1f;
    const auto item = feedback(frame, 2, ai::ActionResultType::Accepted);
    expect(trace.append(state(frame, static_cast<float>(frame - 1) * 0.002f, x),
                        outcome(ai::ActionResultType::Accepted), &item),
           "high-frequency frame admitted even when not retained");
  }
  const auto terminal = feedback(802, 2, ai::ActionResultType::Completed);
  expect(trace.append(state(802, 1.602f, 88.0f),
                      outcome(ai::ActionResultType::Completed), &terminal),
         "terminal frame always recorded independently of interval");
  const auto report = trace.replay();
  expect(report.status == mock::ReplayStatus::Pass, "sampled replay passes");
  expect(report.observedFrames == 802 && report.frames < 100 && report.frames > 30,
         "800 high-frequency frames reduce to bounded recorded samples");
  expect(report.skippedFrames == report.observedFrames - report.frames, "skip accounting matches");
  expect(report.decisionFrameId == 2 && report.terminalFrameId == 802,
         "original frame IDs preserved across sampling");
  expect(report.capacitySkipped == 0, "normal high-FPS sampling does not saturate capacity");
  const auto *last = trace.record(trace.size() - 1);
  expect(last && last->result.type == ai::ActionResultType::Completed,
         "terminal result not lost while sampling");
}

AI_TEST(testMockTelemetryRejectsStaleUnsampledFrameAndInvalidInterval) {
  mock::MockTrace trace {};
  expect(!trace.setSampleIntervalSeconds(0.01f), "below 20 ms rejected");
  expect(!trace.setSampleIntervalSeconds(0.06f), "above 50 ms rejected");
  expect(trace.setSampleIntervalSeconds(0.05f), "50 ms accepted");
  expect(trace.begin(state(1, 1.0f), { 96.0f, 0.0f, 132.0f }, io::QueryStatus::Completed),
         "begin recording");
  expect(!trace.setSampleIntervalSeconds(0.02f), "cannot change rate during running capture");
  const auto first = feedback(2, 2, ai::ActionResultType::Accepted);
  expect(trace.append(state(2, 1.001f, 2.0f),
                      outcome(ai::ActionResultType::Accepted), &first), "first decision stored");
  const auto second = feedback(3, 2, ai::ActionResultType::Accepted);
  expect(trace.append(state(3, 1.002f, 4.0f),
                      outcome(ai::ActionResultType::Accepted), &second), "short interval skipped");
  expect(trace.size() == 2 && trace.observedFrames() == 3, "skipped sample counted");
  expect(!trace.append(state(3, 1.003f, 4.0f),
                       outcome(ai::ActionResultType::Accepted), &second),
         "repeated unsampled frame is still rejected");
  expect(trace.replay().status == mock::ReplayStatus::InvalidFrame,
         "invalid intermediate frame cannot be hidden by sampling");
  trace.reset();
  expect(trace.sampleIntervalSeconds() == 0.05f, "rate persists across run reset");
}
