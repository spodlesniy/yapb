//
// AiPB - perception permission and event snapshot tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_perception_events.h>

#include <limits>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::ServerBotSnapshot snapshot(uint64_t session, uint64_t frame, uint64_t round, int slot) {
  io::ServerBotSnapshot result {};
  result.stamp.sessionId = session;
  result.stamp.frameId = frame;
  result.stamp.roundId = round;
  result.stamp.botEntityIndex = slot + 1;
  result.stamp.gameTime = static_cast<float>(frame);
  result.stamp.frameSeconds = 0.01f;
  result.self.health = 100.0f;
  result.self.maxSpeed = 250.0f;
  return result;
}

bool containsEvent(const io::PerceptionFrame &frame, io::EventKind kind) {
  for (uint8_t i = 0; i < frame.eventCount; ++i) {
    if (frame.events[i].event.kind == kind) {
      return true;
    }
  }
  return false;
}

const io::PerceptionEvent *eventOf(const io::PerceptionFrame &frame, io::EventKind kind) {
  for (uint8_t i = 0; i < frame.eventCount; ++i) {
    if (frame.events[i].event.kind == kind) {
      return &frame.events[i];
    }
  }
  return nullptr;
}

} // namespace

AI_TEST(testPerceptionRejectsHiddenLiveCoordinates) {
  io::PerceptionFrameCache cache {};
  io::PerceptionSignals signals {};
  signals.enemyEntityIndex = 7;
  signals.seenPosition = { 777.0f, 888.0f, 999.0f };
  expect(cache.update(0, snapshot(1, 1, 1, 0), signals), "unknown opponent not rejected as self frame");

  io::PerceptionFrame frame {};
  expect(cache.read(0, frame), "read sanitized frame");
  expect(!frame.visibleEnemy.valid, "hidden enemy is not exported");
  expect(!containsEvent(frame, io::EventKind::EntityAppeared), "hidden enemy does not generate sight event");

  signals.visibleEnemy = true;
  expect(cache.update(0, snapshot(1, 2, 1, 0), signals), "confirmed sight accepted");
  expect(cache.read(0, frame) && frame.visibleEnemy.valid, "visible enemy available");
  expect(frame.visibleEnemy.entityIndex == 7 && frame.visibleEnemy.position.x == 777.0f,
         "only confirmed seen position exported");
  const auto *appeared = eventOf(frame, io::EventKind::EntityAppeared);
  expect(appeared && appeared->hasPosition && appeared->event.evidence == io::Evidence::Seen,
         "sighting carries a seen position");
  expect(appeared && appeared->event.stamp.frameId == 2, "event stamped with its observed frame");

  signals.visibleEnemy = false;
  signals.seenPosition.x = 123456.0f;
  expect(cache.update(0, snapshot(1, 3, 1, 0), signals), "sighting loss accepted");
  expect(cache.read(0, frame) && !frame.visibleEnemy.valid, "last seen enemy not treated as currently visible");
  const auto *lost = eventOf(frame, io::EventKind::EntityDisappeared);
  expect(lost && !lost->hasPosition && lost->event.position.x == 0.0f,
         "disappearance never exposes a hidden live location");
  expect(lost && lost->event.subjectEntityIndex == 7, "disappearance refers only to previously seen enemy");
}

AI_TEST(testPerceptionSoundEventsAreAnonymousAndDeduplicated) {
  io::PerceptionFrameCache cache {};
  io::PerceptionSignals signals {};
  signals.heardSound = true;
  signals.enemyEntityIndex = 19;
  signals.seenPosition = { 999.0f, 999.0f, 999.0f };
  signals.soundPosition = { 12.0f, 20.0f, 40.0f };
  io::PerceptionFrame frame {};
  expect(cache.update(1, snapshot(1, 1, 3, 1), signals), "audible signal accepted");
  expect(cache.read(1, frame), "read audible frame");
  const auto *sound = eventOf(frame, io::EventKind::Sound);
  expect(sound && sound->hasPosition && sound->event.position.x == 12.0f, "hearing uses sound origin");
  expect(sound && sound->event.subjectEntityIndex == -1 && sound->event.evidence == io::Evidence::Heard,
         "hearing cannot reveal hidden source identity");
  expect(!frame.visibleEnemy.valid, "sound never grants live sight");

  expect(cache.update(1, snapshot(1, 2, 3, 1), signals), "continued sound accepted");
  expect(cache.read(1, frame) && !containsEvent(frame, io::EventKind::Sound),
         "continuous hearing does not generate duplicate event each frame");
  signals.heardSound = false;
  expect(cache.update(1, snapshot(1, 3, 3, 1), signals), "hearing ended");
  signals.heardSound = true;
  expect(cache.update(1, snapshot(1, 4, 3, 1), signals), "new hearing period accepted");
  expect(cache.read(1, frame) && containsEvent(frame, io::EventKind::Sound), "new hearing period produces event");
}

AI_TEST(testPerceptionRoundEventsArePublicAndBounded) {
  io::PerceptionFrameCache cache {};
  io::PerceptionSignals signals {};
  io::PerceptionFrame frame {};
  expect(cache.update(0, snapshot(1, 1, 2, 0), signals), "first round sample accepted");
  expect(cache.read(0, frame), "round event readable");
  const auto *round = eventOf(frame, io::EventKind::RoundStarted);
  expect(round && round->event.evidence == io::Evidence::PublicEvent && !round->hasPosition,
         "public start event has no hidden position");

  expect(cache.update(0, snapshot(1, 2, 2, 0), signals), "same round accepted");
  expect(cache.read(0, frame) && frame.eventCount == 0, "no repeated round event");
  signals.roundOver = true;
  expect(cache.update(0, snapshot(1, 3, 2, 0), signals), "round over accepted");
  expect(cache.read(0, frame) && containsEvent(frame, io::EventKind::RoundEnded), "round-end edge captured");
  signals.roundOver = false;
  signals.visibleEnemy = true;
  signals.enemyEntityIndex = 8;
  signals.seenPosition = { 1.0f, 2.0f, 3.0f };
  signals.heardSound = true;
  signals.soundPosition = { 4.0f, 5.0f, 6.0f };
  expect(cache.update(0, snapshot(1, 4, 3, 0), signals), "simultaneous events accepted");
  expect(cache.read(0, frame) && frame.eventCount == 3, "round sight and sound events bounded");
  expect(frame.eventCount <= io::kPerceptionEventsPerFrame, "frame never exceeds fixed capacity");
}

AI_TEST(testPerceptionRejectsInvalidAndStaleSamples) {
  io::PerceptionFrameCache cache {};
  io::PerceptionSignals signals {};
  signals.visibleEnemy = true;
  signals.enemyEntityIndex = 5;
  signals.seenPosition.x = std::numeric_limits<float>::quiet_NaN();
  io::PerceptionFrame frame {};
  expect(cache.update(0, snapshot(10, 1, 1, 0), signals), "bad sight signal does not invalidate own self");
  expect(cache.read(0, frame) && !frame.visibleEnemy.valid, "nonfinite sight redacted");
  signals.seenPosition = { 10.0f, 11.0f, 12.0f };
  expect(cache.update(0, snapshot(10, 2, 1, 0), signals), "valid sight");
  expect(!cache.update(0, snapshot(10, 2, 1, 0), signals), "duplicate frame rejected");
  expect(!cache.read(0, frame), "stale frame invalidates cached perception");
  expect(cache.update(0, snapshot(11, 1, 0, 0), signals), "new session accepted");
  expect(cache.read(0, frame) && frame.visibleEnemy.valid, "new session resets perception state");
  expect(!cache.update(1, snapshot(12, 1, 0, 0), signals), "wrong bot slot cannot be served");
  expect(!cache.read(1, frame), "wrong-slot data not retained");
}

AI_TEST(testPerceptionIsolationAndExplicitTeardown) {
  io::PerceptionFrameCache cache {};
  io::PerceptionSignals signals {};
  signals.visibleEnemy = true;
  signals.enemyEntityIndex = 9;
  signals.seenPosition = { 25.0f, 30.0f, 35.0f };
  io::PerceptionFrame frame {};
  expect(cache.update(0, snapshot(1, 1, 1, 0), signals), "bot one updated");
  signals.visibleEnemy = false;
  expect(cache.update(1, snapshot(2, 1, 1, 1), signals), "bot two updated");
  expect(cache.read(0, frame) && frame.visibleEnemy.valid, "bot one sight persists within frame");
  frame.visibleEnemy.position.x = 100000.0f;
  expect(cache.read(0, frame) && frame.visibleEnemy.position.x == 25.0f,
         "perception read is a copy, not mutable cache state");
  cache.release(0);
  expect(!cache.read(0, frame) && cache.read(1, frame), "disconnect clears only one bot");
  cache.reset();
  expect(!cache.read(1, frame), "map reset clears all bot perceptions");
}

AI_TEST(testPerceptionCannotInventSelfEventsWithoutState) {
  io::PerceptionFrameCache cache {};
  io::PerceptionSignals signals {};
  io::PerceptionFrame frame {};
  auto bad = snapshot(0, 0, 1, 0);
  expect(!cache.update(0, bad, signals), "unidentified snapshot rejected");
  expect(!cache.read(0, frame), "unidentified snapshot never becomes perception");
  expect(!cache.update(-1, snapshot(1, 1, 1, 0), signals), "out-of-range negative bot slot rejected");
  expect(!cache.update(io::kServerBotSlots, snapshot(1, 1, 1, 0), signals),
         "out-of-range positive bot slot rejected");
}
