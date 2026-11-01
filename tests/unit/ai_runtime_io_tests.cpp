//
// AiPB - engine-neutral runtime I/O contract tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_runtime_io.h>

#include <limits>

using ai::test::expect;
namespace io = ai::runtime_io;

AI_TEST(testRuntimeIoLifecycle) {
  expect(io::canTransition(io::SessionState::Created, io::SessionState::Ready), "created can become ready");
  expect(io::canTransition(io::SessionState::Active, io::SessionState::Interrupted), "active can be interrupted");
  expect(io::canTransition(io::SessionState::Interrupted, io::SessionState::Ready), "interrupted can recover");
  expect(io::canTransition(io::SessionState::Active, io::SessionState::Ready), "active can stop normally");
  expect(!io::canTransition(io::SessionState::Created, io::SessionState::Active), "unready bot cannot start");
  expect(!io::canTransition(io::SessionState::Ready, io::SessionState::Ready), "same state is not a transition");
  expect(!io::canTransition(io::SessionState::Destroyed, io::SessionState::Ready), "destroyed bot cannot recover");
}

AI_TEST(testRuntimeIoFrameProvenance) {
  io::FrameStamp first {};
  first.sessionId = 1;
  first.frameId = 1;
  first.roundId = 1;
  first.botEntityIndex = 2;
  first.gameTime = 10.0f;
  first.frameSeconds = 0.01f;
  expect(io::validateFrame(first) == io::FrameValidation::Valid, "first frame valid");

  io::FrameStamp next = first;
  next.frameId = 2;
  next.gameTime = 10.01f;
  expect(io::validateFrame(next, &first) == io::FrameValidation::Valid, "monotonic frame valid");
  expect(io::validateFrame(first, &next) == io::FrameValidation::Stale, "old frame rejected");
  expect(io::validateFrame(next, &next) == io::FrameValidation::Stale, "duplicate frame rejected");
  next.botEntityIndex = 3;
  expect(io::validateFrame(next, &first) == io::FrameValidation::Stale, "reused bot slot requires new session");
  next = first;
  next.frameId = 2;
  next.roundId = 0;
  expect(io::validateFrame(next, &first) == io::FrameValidation::Stale, "regressed round rejected");
  next = first;
  next.frameId = 2;
  next.gameTime = 9.0f;
  expect(io::validateFrame(next, &first) == io::FrameValidation::Stale, "time regression rejected");

  next = first;
  next.sessionId = 2;
  next.frameId = 1;
  next.roundId = 0;
  next.gameTime = 0.0f;
  next.botEntityIndex = 3;
  expect(io::validateFrame(next, &first) == io::FrameValidation::Valid, "new session may reset game time and slot");
  next.schemaVersion = 99;
  expect(io::validateFrame(next) == io::FrameValidation::UnsupportedSchema, "schema rejected");
  next = first;
  next.sessionId = 0;
  expect(io::validateFrame(next) == io::FrameValidation::InvalidIdentity, "missing session rejected");
  next = first;
  next.gameTime = std::numeric_limits<float>::quiet_NaN();
  expect(io::validateFrame(next) == io::FrameValidation::InvalidClock, "nonfinite game time rejected");
  next = first;
  next.frameSeconds = -1.0f;
  expect(io::validateFrame(next) == io::FrameValidation::InvalidClock, "negative frame duration rejected");
}

AI_TEST(testRuntimeIoQueryCorrelation) {
  io::WorldQueryRequest request {};
  request.requestId = 7;
  request.sessionId = 3;
  request.observationFrameId = 101;
  request.kind = io::QueryKind::TraceHull;
  io::WorldQueryResult reply {};
  reply.requestId = 7;
  reply.sessionId = 3;
  reply.observationFrameId = 101;
  reply.status = io::QueryStatus::Completed;
  reply.fraction = 1.0f;
  expect(io::isCompleted(request, reply), "completed correlated trace accepted");
  reply.observationFrameId = 100;
  expect(!io::matchesRequest(request, reply), "previous-frame reply rejected");
  reply.observationFrameId = 101;
  reply.sessionId = 2;
  expect(!io::matchesRequest(request, reply), "previous-session reply rejected");
  reply.sessionId = 3;
  reply.requestId = 8;
  expect(!io::matchesRequest(request, reply), "unrelated request reply rejected");
  reply.requestId = 7;
  reply.status = io::QueryStatus::BudgetExceeded;
  expect(io::matchesRequest(request, reply), "budget rejection still matches request");
  expect(!io::isCompleted(request, reply), "budget rejection is not a successful trace");
  reply.status = io::QueryStatus::Completed;
  reply.fraction = std::numeric_limits<float>::infinity();
  expect(!io::isCompleted(request, reply), "invalid trace fraction rejected");
  reply.fraction = -0.01f;
  expect(!io::isCompleted(request, reply), "out-of-range fraction rejected");
  request.schemaVersion = 2;
  expect(!io::matchesRequest(request, reply), "unknown request version rejected");
  request.schemaVersion = io::kSchemaVersion;
  request.requestId = 0;
  reply.requestId = 0;
  expect(!io::matchesRequest(request, reply), "zero request ID rejected");
}

AI_TEST(testRuntimeIoEmptyRecordsAreNotEvidence) {
  const io::FrameStamp stamp {};
  const io::WorldQueryRequest request {};
  const io::WorldQueryResult reply {};
  const io::WorldEvent event {};
  const io::ActionFeedback feedback {};
  expect(io::validateFrame(stamp) == io::FrameValidation::InvalidIdentity, "default frame is invalid");
  expect(!io::matchesRequest(request, reply), "empty request and reply do not match");
  expect(event.evidence == io::Evidence::Unknown, "event provenance defaults unknown");
  expect(reply.status == io::QueryStatus::Unknown, "query defaults unknown");
  expect(feedback.result.type == ai::ActionResultType::None, "feedback defaults to no outcome");
}
