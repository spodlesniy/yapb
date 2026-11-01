//
// AiPB - world query admission and trace sanitization tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_world_query_gate.h>

#include <limits>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::ServerBotSnapshot makeSnapshot(uint64_t session = 1, uint64_t frame = 1, int slot = 0) {
  io::ServerBotSnapshot snapshot {};
  snapshot.stamp.sessionId = session;
  snapshot.stamp.frameId = frame;
  snapshot.stamp.botEntityIndex = slot + 1;
  snapshot.stamp.gameTime = static_cast<float>(frame);
  snapshot.stamp.frameSeconds = 0.01f;
  snapshot.self.origin = { 10.0f, 20.0f, 30.0f };
  snapshot.self.health = 100.0f;
  snapshot.self.maxSpeed = 250.0f;
  return snapshot;
}

io::WorldQueryRequest makeRequest(uint64_t id = 1, uint64_t session = 1, uint64_t frame = 1) {
  io::WorldQueryRequest request {};
  request.requestId = id;
  request.sessionId = session;
  request.observationFrameId = frame;
  request.kind = io::QueryKind::TraceLine;
  request.start = { 10.0f, 20.0f, 30.0f };
  request.end = { 30.0f, 20.0f, 30.0f };
  return request;
}

} // namespace

AI_TEST(testWorldQueryRejectsStaleSessionFrameAndSlot) {
  io::WorldQueryGate gate {};
  auto sample = makeSnapshot();
  auto request = makeRequest();
  expect(gate.admit(-1, sample, request) == io::QueryStatus::Rejected, "negative slot rejected");
  expect(gate.admit(io::kServerBotSlots, sample, request) == io::QueryStatus::Rejected, "large slot rejected");
  expect(gate.admit(1, sample, request) == io::QueryStatus::Stale, "other bot slot denied");
  request.sessionId = 2;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Stale, "different session denied");
  request.sessionId = 1;
  request.observationFrameId = 2;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Stale, "different frame denied");
  request.observationFrameId = 1;
  sample.stamp.sessionId = 0;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Stale, "invalid snapshot denied");
  sample = makeSnapshot();
  expect(gate.admit(0, sample, request) == io::QueryStatus::Completed, "valid same-frame query admitted");
  expect(gate.admit(0, makeSnapshot(2, 1), makeRequest(2, 2, 1)) == io::QueryStatus::Completed,
         "next bot session creates new request namespace");
  expect(gate.admit(0, sample, makeRequest(3)) == io::QueryStatus::Stale, "old session cannot regain access");
}

AI_TEST(testWorldQueryRejectsUnsupportedAndUnsafeGeometry) {
  io::WorldQueryGate gate {};
  auto sample = makeSnapshot();
  auto request = makeRequest();
  request.schemaVersion = 999;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Unsupported, "future schema unsupported");
  request.schemaVersion = io::kSchemaVersion;
  request.kind = io::QueryKind::FindEntities;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Unsupported, "entity enumeration unsupported");
  request.kind = io::QueryKind::PointContents;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Unsupported, "point contents deferred");
  request.kind = io::QueryKind::TraceLine;
  request.hull = 1;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Rejected, "line hull argument rejected");
  request.kind = io::QueryKind::TraceHull;
  request.hull = -1;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Rejected, "invalid hull rejected");
  request.hull = io::kWorldQueryCrouchingHull;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Completed, "crouching hull accepted");
  request = makeRequest(2);
  request.start.x = 75.0f;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Rejected, "far start rejected");
  request = makeRequest(2);
  request.end.x = 395.0f;
  expect(gate.admit(0, sample, request) == io::QueryStatus::Rejected, "long trace rejected");
  request = makeRequest(2);
  request.end.x = std::numeric_limits<float>::quiet_NaN();
  expect(gate.admit(0, sample, request) == io::QueryStatus::Rejected, "nonfinite end rejected");
}

AI_TEST(testWorldQueryPerFrameBudgetAndReplay) {
  io::WorldQueryGate gate {};
  const auto sample = makeSnapshot();
  for (uint64_t id = 1; id <= io::kWorldQueriesPerBotFrame; ++id) {
    expect(gate.admit(0, sample, makeRequest(id)) == io::QueryStatus::Completed, "in-budget query accepted");
  }
  expect(gate.admit(0, sample, makeRequest(5)) == io::QueryStatus::BudgetExceeded, "budget enforced");
  expect(gate.admit(0, sample, makeRequest(2)) == io::QueryStatus::Rejected, "replay rejected");
  expect(gate.admit(1, makeSnapshot(2, 1, 1), makeRequest(1, 2, 1)) == io::QueryStatus::Completed,
         "other bot has independent budget");
  expect(gate.admit(0, makeSnapshot(1, 2), makeRequest(5, 1, 2)) == io::QueryStatus::Completed,
         "new frame refreshes query budget");
  expect(gate.admit(0, makeSnapshot(1, 2), makeRequest(4, 1, 2)) == io::QueryStatus::Rejected,
         "request IDs monotonic across frames");
  expect(gate.admit(0, makeSnapshot(1, 1), makeRequest(6)) == io::QueryStatus::Stale,
         "older frame cannot regain budget");
  gate.release(0);
  expect(gate.admit(0, makeSnapshot(3, 1), makeRequest(1, 3, 1)) == io::QueryStatus::Completed,
         "new bot session succeeds after release");
  gate.reset();
  expect(gate.admit(0, makeSnapshot(4, 1), makeRequest(1, 4, 1)) == io::QueryStatus::Completed,
         "fresh map session succeeds");
}

AI_TEST(testWorldQuerySanitizesTraceResults) {
  const auto request = makeRequest(9, 7, 3);
  io::GeometryTraceSample trace {};
  trace.fraction = 0.5f;
  trace.endPosition = { 20.0f, 20.0f, 30.0f };
  trace.planeNormal = { 0.0f, 0.0f, 1.0f };
  auto reply = io::worldTraceResult(request, trace);
  expect(io::isCompleted(request, reply), "valid trace has correlated completed status");
  expect(reply.hitEntityIndex == -1 && reply.contents == 0, "no entity or contents leaks");
  trace.hitPlayer = true;
  reply = io::worldTraceResult(request, trace);
  expect(reply.status == io::QueryStatus::Error && reply.hitPosition.x == 0.0f,
         "player contact is rejected without coordinates");
  trace.hitPlayer = false;
  trace.fraction = std::numeric_limits<float>::quiet_NaN();
  expect(io::worldTraceResult(request, trace).status == io::QueryStatus::Error,
         "bad fraction not treated as evidence");
  trace.fraction = 0.5f;
  trace.planeNormal.x = std::numeric_limits<float>::infinity();
  expect(io::worldTraceResult(request, trace).status == io::QueryStatus::Error,
         "nonfinite normal rejected");
  reply = io::rejectedWorldQuery(request, io::QueryStatus::Stale);
  expect(io::matchesRequest(request, reply) && !io::isCompleted(request, reply),
         "failure remains correlated but untrusted");
}

AI_TEST(testWorldQueryBoundsAndInvalidSelfState) {
  io::WorldQueryGate gate {};
  auto request = makeRequest();
  request.start = { 74.0f, 20.0f, 30.0f };
  request.end = { 458.0f, 20.0f, 30.0f };
  expect(gate.admit(0, makeSnapshot(), request) == io::QueryStatus::Completed,
         "exact boundaries are allowed");
  request.requestId = 2;
  request.end.x = 458.01f;
  expect(gate.admit(0, makeSnapshot(), request) == io::QueryStatus::Rejected,
         "beyond max range rejected");
  auto bad = makeSnapshot(2, 1, 1);
  bad.self.velocity.y = std::numeric_limits<float>::quiet_NaN();
  expect(gate.admit(1, bad, makeRequest(1, 2, 1)) == io::QueryStatus::Stale,
         "nonfinite bot state cannot authorize geometry query");
}
