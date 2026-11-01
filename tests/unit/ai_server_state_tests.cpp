//
// AiPB - server state tracking contract tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_server_state.h>

#include <limits>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::BotSelfState makeSelfState() {
  io::BotSelfState result {};
  result.origin = { 11.0f, -22.0f, 33.0f };
  result.velocity = { 100.0f, 5.0f, -2.0f };
  result.viewAngles = { 0.0f, 180.0f, 0.0f };
  result.health = 100.0f;
  result.armor = 50.0f;
  result.maxSpeed = 250.0f;
  result.alive = true;
  result.onGround = true;
  return result;
}

} // namespace

AI_TEST(testServerStateCapturesOneFramePerBotCallback) {
  io::ServerStateCache cache {};
  io::ServerBotSnapshot snapshot {};
  expect(!cache.read(1, snapshot), "no uninitialized snapshot");
  expect(cache.capture(1, 42, 2, 10.0f, 0.01f, makeSelfState()), "first capture accepted");
  expect(cache.read(1, snapshot), "captured snapshot exists");
  expect(snapshot.stamp.botEntityIndex == 2, "zero-based bot index maps to one-based entity index");
  expect(snapshot.stamp.sessionId == 1 && snapshot.stamp.frameId == 1, "first frame starts first session");
  expect(snapshot.self.origin.x == 11.0f && snapshot.self.velocity.y == 5.0f, "self physical state copied");
  expect(snapshot.self.onGround && !snapshot.self.crouching, "stance flags copied");
  expect(cache.capture(1, 42, 3, 10.01f, 0.01f, makeSelfState()), "new round frame accepted");
  expect(cache.read(1, snapshot), "next frame available");
  expect(snapshot.stamp.sessionId == 1 && snapshot.stamp.frameId == 2 && snapshot.stamp.roundId == 3,
         "round transition retains bot identity and advances frame");
}

AI_TEST(testServerStateIdentityAndMapInvalidation) {
  io::ServerStateCache cache {};
  io::ServerBotSnapshot snapshot {};
  expect(cache.capture(0, 101, 1, 1.0f, 0.01f, makeSelfState()), "slot 0 captured");
  expect(cache.capture(1, 202, 1, 1.0f, 0.01f, makeSelfState()), "slot 1 captured");
  expect(cache.capture(0, 303, 1, 1.01f, 0.01f, makeSelfState()), "identity replacement accepted");
  expect(cache.read(0, snapshot) && snapshot.stamp.sessionId == 3 && snapshot.stamp.frameId == 1,
         "identity change allocates fresh global session");
  cache.release(0);
  expect(!cache.read(0, snapshot) && cache.read(1, snapshot), "disconnect clears only the selected slot");
  expect(cache.capture(0, 303, 1, 1.02f, 0.01f, makeSelfState()), "reconnected identity captured");
  expect(cache.read(0, snapshot) && snapshot.stamp.sessionId == 4, "reused pointer after disconnect has new session");
  cache.reset();
  expect(!cache.read(0, snapshot) && !cache.read(1, snapshot), "map shutdown removes all snapshots");
  expect(cache.capture(0, 303, 0, 0.0f, 0.0f, makeSelfState()), "new map starts at zero time");
  expect(cache.read(0, snapshot) && snapshot.stamp.sessionId == 5 && snapshot.stamp.frameId == 1,
         "map recreation never reuses a session ID");
}

AI_TEST(testServerStateRejectsUnsafeFrames) {
  io::ServerStateCache cache {};
  io::ServerBotSnapshot snapshot {};
  auto self = makeSelfState();
  expect(!cache.capture(-1, 101, 0, 1.0f, 0.01f, self), "negative slot rejected");
  expect(!cache.capture(io::kServerBotSlots, 101, 0, 1.0f, 0.01f, self), "out of range slot rejected");
  expect(!cache.capture(1, 0, 0, 1.0f, 0.01f, self), "zero identity rejected");
  expect(!cache.capture(1, 101, 0, -1.0f, 0.01f, self), "negative game time rejected");
  expect(!cache.read(1, snapshot), "invalid frame never visible");
  expect(cache.capture(1, 101, 1, 10.0f, 0.01f, self), "valid initial frame");
  expect(!cache.capture(1, 101, 1, 9.0f, 0.01f, self), "clock regression rejected");
  expect(!cache.read(1, snapshot), "clock regression invalidates stale snapshot");
  expect(cache.capture(1, 101, 1, 11.0f, 0.01f, self), "recovery gets new session");
  expect(cache.read(1, snapshot) && snapshot.stamp.frameId == 1 && snapshot.stamp.sessionId == 2,
         "recovery is distinguishable from old session");
  self.origin.x = std::numeric_limits<float>::quiet_NaN();
  expect(!cache.capture(1, 101, 1, 11.01f, 0.01f, self), "nonfinite position rejected");
  expect(!cache.read(1, snapshot), "invalid physics clears last snapshot");
  self = makeSelfState();
  self.maxSpeed = -2.0f;
  expect(!cache.capture(1, 101, 1, 11.02f, 0.01f, self), "invalid max speed rejected");
}

AI_TEST(testServerStateReadIsCopyNotLiveEngineReference) {
  io::ServerStateCache cache {};
  io::ServerBotSnapshot copy {};
  expect(cache.capture(2, 99, 1, 3.0f, 0.015f, makeSelfState()), "capture before read");
  expect(cache.read(2, copy), "snapshot copied");
  copy.self.origin.x = 10000.0f;
  io::ServerBotSnapshot fresh {};
  expect(cache.read(2, fresh) && fresh.self.origin.x == 11.0f, "caller cannot mutate internal state");
  cache.release(2);
  expect(!cache.read(2, fresh), "no stale snapshot after release");
}
