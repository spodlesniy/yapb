//
// AiPB - stamped Model I/O bridge tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"
#include "ai_test_tools.h"

#include <limits>

#include <ai/ai_action_runtime.h>
#include <ai/ai_inference_policy.h>
#include <ai/ai_model_io.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::ServerBotSnapshot makeSelf(uint64_t session, uint64_t frame, float time) {
  io::ServerBotSnapshot value {};
  value.stamp.sessionId = session;
  value.stamp.frameId = frame;
  value.stamp.roundId = 7;
  value.stamp.botEntityIndex = 2;
  value.stamp.gameTime = time;
  value.stamp.frameSeconds = 0.02f;
  value.self.origin = { 100.0f, 200.0f, 32.0f };
  value.self.velocity = { 10.0f, 0.0f, 0.0f };
  value.self.viewAngles = { 0.0f, 30.0f, 0.0f };
  value.self.maxSpeed = 250.0f;
  value.self.health = 100.0f;
  value.self.alive = true;
  value.self.onGround = true;
  return value;
}

io::PerceptionFrame makePerception(const io::ServerBotSnapshot &self) {
  io::PerceptionFrame value {};
  value.self = self;
  value.eventCount = 1;
  value.events[0].event.stamp = self.stamp;
  value.events[0].event.kind = io::EventKind::Sound;
  value.events[0].event.evidence = io::Evidence::Heard;
  value.events[0].event.subjectEntityIndex = -1;
  value.events[0].event.position = { 140.0f, 200.0f, 32.0f };
  value.events[0].hasPosition = true;
  return value;
}

io::ControlFrame makeOwnership(const io::ServerBotSnapshot &self) {
  io::ControlFrame value {};
  value.stamp = self.stamp;
  value.owner = io::ControlOwner::Neural;
  return value;
}

ai::Observation makeObservation(float time) {
  ai::Observation value {};
  value.gameTime = time;
  value.bot.alive = true;
  value.bot.health = 100.0f;
  value.playerCount = 1;
  value.players[0].valid = true;
  value.players[0].enemy = true;
  value.players[0].visible = false;
  value.players[0].relativeOrigin = { 9999.0f, 0.0f, 0.0f };
  value.players[0].health = 100.0f;
  return value;
}

class TestBackend final : public io::ModelIOBackend {
public:
  enum class Mode { Valid, WrongRequest, WrongSession, WrongFrame, SchemaMismatch, InvalidAction, NoDecision };
  mutable int calls {};
  mutable io::ModelIORequest last {};
  Mode mode { Mode::Valid };

  io::ModelIOResponse infer(const io::ModelIORequest &request) const override {
    ++calls;
    last = request;
    io::ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = ai::InferenceStatus::Success;
    response.inference.output.actionId = static_cast<uint8_t>(ai::InferenceActionId::MoveToPosition);
    response.inference.output.targetPosition = { 48.0f, 64.0f, 32.0f };
    response.inference.output.confidence = 0.9f;
    switch (mode) {
    case Mode::Valid: break;
    case Mode::WrongRequest: ++response.requestId; break;
    case Mode::WrongSession: ++response.sessionId; break;
    case Mode::WrongFrame: ++response.observationFrameId; break;
    case Mode::SchemaMismatch: ++response.inference.output.schemaVersion; break;
    case Mode::InvalidAction: response.inference.output.targetPosition.x =
        std::numeric_limits<float>::quiet_NaN(); break;
    case Mode::NoDecision: response.inference.status = ai::InferenceStatus::NoDecision; break;
    }
    return response;
  }
};

} // namespace

AI_TEST(testModelIOValidActionAndSanitizedRequest) {
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  const auto self = makeSelf(3, 10, 1.0f);
  const auto perception = makePerception(self);
  const auto control = makeOwnership(self);
  expect(adapter.bindFrame(self, perception, control), "current Neural owner and perception accepted");

  ai::InferencePolicy policy { &adapter };
  const auto action = policy.decide(makeObservation(1.0f));
  expect(action.type == ai::ActionType::MoveToPosition &&
         action.targetPosition.x == 48.0f, "independent response decoded by existing policy");
  expect(backend.calls == 1, "backend called once for decision frame");
  expect(backend.last.schemaVersion == io::kModelIOSchemaVersion &&
         backend.last.requestId == 1 && backend.last.stamp.frameId == 10, "request version and identity");
  expect(backend.last.eventCount == 1 &&
         backend.last.events[0].event.evidence == io::Evidence::Heard &&
         backend.last.events[0].event.subjectEntityIndex == -1, "anonymous sound is authorized");
  expect(backend.last.self.origin.x == 100.0f &&
         backend.last.features.hasSupportedSchema(), "self and existing feature encoder forwarded");
  const size_t relativeX = ai::kCoreFeatureCount +
      static_cast<size_t>(ai::InferenceFeature::Player::RelativeX);
  expect(backend.last.features.values[relativeX] == 0.0f,
         "hidden enemy coordinates are masked by existing inference encoder");
  expect(policy.decide(makeObservation(1.0f)).type == ai::ActionType::None,
         "one frame cannot issue multiple model requests");
  expect(backend.calls == 1, "no duplicate inference request");
}

AI_TEST(testModelIORejectsBadResponseIdentityAndOutput) {
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  ai::InferencePolicy policy { &adapter };
  const TestBackend::Mode bad[] {
    TestBackend::Mode::WrongRequest, TestBackend::Mode::WrongSession,
    TestBackend::Mode::WrongFrame, TestBackend::Mode::SchemaMismatch,
    TestBackend::Mode::InvalidAction, TestBackend::Mode::NoDecision,
  };
  for (int i = 0; i < 6; ++i) {
    const auto self = makeSelf(3, static_cast<uint64_t>(11 + i), 1.1f + i * 0.1f);
    backend.mode = bad[i];
    expect(adapter.bindFrame(self, makePerception(self), makeOwnership(self)),
           "new current frame accepted");
    expect(policy.decide(makeObservation(self.stamp.gameTime)).type == ai::ActionType::None,
           "stale/corrupt/invalid output never becomes action");
  }
  expect(backend.calls == 6, "each test attempted exactly one model call");
}


AI_TEST(testModelIOPerceptionAllowlistBlocksHiddenData) {
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  ai::InferencePolicy policy { &adapter };
  const auto self = makeSelf(20, 30, 3.0f);
  const auto owner = makeOwnership(self);
  auto perception = makePerception(self);

  perception.events[0].event.kind = io::EventKind::Damage;
  expect(!adapter.bindFrame(self, perception, owner), "unsupported event kind rejected");
  expect(policy.decide(makeObservation(3.0f)).type == ai::ActionType::None,
         "rejected perception cannot dispatch a backend request");
  expect(backend.calls == 0, "no backend call after unsafe event");

  perception = makePerception(self);
  perception.events[0].event.subjectEntityIndex = 4;
  expect(!io::validModelIOPerception(perception, self.stamp),
         "anonymous sound cannot reveal emitter identity");
  perception = makePerception(self);
  perception.events[0].event.kind = io::EventKind::RoundEnded;
  perception.events[0].event.evidence = io::Evidence::PublicEvent;
  expect(!io::validModelIOPerception(perception, self.stamp),
         "public event cannot contain sound position");
  perception.events[0].hasPosition = false;
  expect(!io::validModelIOPerception(perception, self.stamp),
         "absent position cannot retain hidden coordinates");
  perception.events[0].event.position = {};
  expect(io::validModelIOPerception(perception, self.stamp),
         "position-free public round event accepted");

  perception = makePerception(self);
  perception.events[0].event.kind = io::EventKind::EntityDisappeared;
  perception.events[0].event.evidence = io::Evidence::Remembered;
  perception.events[0].event.subjectEntityIndex = 4;
  expect(!io::validModelIOPerception(perception, self.stamp),
         "disappearance must not include a hidden last-known position");
  perception.events[0].hasPosition = false;
  perception.events[0].event.position = {};
  expect(io::validModelIOPerception(perception, self.stamp),
         "position-free disappearance with remembered identity accepted");

  perception = makePerception(self);
  perception.visibleEnemy.position = { 5.0f, 6.0f, 7.0f };
  expect(!io::validModelIOPerception(perception, self.stamp),
         "invalid sighting must not carry stale coordinates");
  perception.visibleEnemy.valid = true;
  perception.visibleEnemy.entityIndex = 4;
  perception.events[0].event.kind = io::EventKind::EntityAppeared;
  perception.events[0].event.evidence = io::Evidence::Seen;
  perception.events[0].event.subjectEntityIndex = 4;
  perception.events[0].event.position = perception.visibleEnemy.position;
  expect(io::validModelIOPerception(perception, self.stamp),
         "fresh visible target and matching appearance accepted");
  perception.events[0].event.position.x += 1.0f;
  expect(!io::validModelIOPerception(perception, self.stamp),
         "appearance cannot disagree with authorized sighting");
  perception.events[0].event.position = perception.visibleEnemy.position;
  perception.events[0].event.subjectEntityIndex = 5;
  expect(!io::validModelIOPerception(perception, self.stamp),
         "appearance cannot identify a different unseen entity");
  perception.events[0].event.subjectEntityIndex = 4;
  expect(adapter.bindFrame(self, perception, owner),
         "valid sighting still passes through normal Model I/O adapter");
  expect(policy.decide(makeObservation(3.0f)).type == ai::ActionType::MoveToPosition,
         "valid perception permits one backend decision");
  expect(backend.calls == 1, "exactly one authorized model request");
}

AI_TEST(testModelIORejectsStaleAndConflictingOwnership) {
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  const auto self = makeSelf(5, 11, 2.0f);
  expect(adapter.bindFrame(self, makePerception(self), makeOwnership(self)),
         "initial state bound");
  ai::InferencePolicy policy { &adapter };
  expect(policy.decide(makeObservation(2.0f)).type == ai::ActionType::MoveToPosition,
         "initial decision works");
  expect(!adapter.bindFrame(self, makePerception(self), makeOwnership(self)),
         "repeated frame rejected");
  expect(policy.decide(makeObservation(2.0f)).type == ai::ActionType::None,
         "failed bind invalidates previous frame");

  const auto next = makeSelf(5, 12, 2.1f);
  auto wrong = makeOwnership(next);
  wrong.owner = io::ControlOwner::Explorer;
  expect(!adapter.bindFrame(next, makePerception(next), wrong),
         "Explorer ownership cannot issue Neural request");
  expect(!adapter.bindFrame(next, makePerception(self), makeOwnership(next)),
         "stale perception frame rejected");
  expect(adapter.bindFrame(next, makePerception(next), makeOwnership(next)),
         "fresh coherent frame recovers without old request");
  expect(policy.decide(makeObservation(4.0f)).type == ai::ActionType::None,
         "stale observation clock rejected");
  expect(policy.decide(makeObservation(2.1f)).type == ai::ActionType::MoveToPosition,
         "correct observation may still issue the one request");

  adapter.invalidateFrame();
  expect(policy.decide(makeObservation(2.1f)).type == ai::ActionType::None,
         "explicit invalidation prevents new calls");
}

AI_TEST(testModelIONewBackendCanBindFeedbackFreeFrameAfterLedgerClear) {
  io::ExecutionOwnershipLedger ledger {};
  const auto prior = makeSelf(7, 20, 2.0f);
  expect(ledger.observe(1, prior.stamp, io::ControlOwner::Neural),
         "owner captured for prior action");
  ai::ActionResult accepted {};
  accepted.action = ai::ActionType::MoveToPosition;
  accepted.type = ai::ActionResultType::Accepted;
  accepted.elapsedTime = 0.1f;
  expect(ledger.record(1, prior.stamp, io::ControlOwner::Neural, accepted),
         "prior movement accepted");
  const auto terminal = makeSelf(7, 21, 2.1f);
  expect(ledger.observe(1, terminal.stamp, io::ControlOwner::Neural),
         "terminal engine frame observed");
  accepted.type = ai::ActionResultType::Completed;
  expect(ledger.record(1, terminal.stamp, io::ControlOwner::Neural, accepted),
         "terminal feedback retained");
  io::ControlFrame control {};
  expect(ledger.read(1, control) && control.hasFeedback,
         "old terminal feedback would be available without cleanup");
  expect(ledger.clearFinishedFeedback(1), "starting new leg clears old terminal result");
  const auto next = makeSelf(7, 22, 2.2f);
  expect(ledger.observe(1, next.stamp, io::ControlOwner::Neural) &&
         ledger.read(1, control) && !control.hasFeedback,
         "new leg receives no stale feedback");
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  expect(adapter.bindFrame(next, makePerception(next), control),
         "adapter binds fresh frame with previous history filtered out");
  const auto output = adapter.infer({ ai::kInferenceInputSchemaVersion,
                                    makeObservation(2.2f) });
  expect(output.isSuccess() && !backend.last.hasFeedback,
         "first fresh request cannot repeat a prior action's feedback");
}

AI_TEST(testModelIOFeedbackDeliveredOnlyOncePerResult) {
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  const auto first = makeSelf(6, 20, 3.0f);
  auto owner = makeOwnership(first);
  owner.hasFeedback = true;
  owner.feedback.sessionId = first.stamp.sessionId;
  owner.feedback.decisionFrameId = 19;
  owner.feedback.observedFrameId = 20;
  owner.feedback.result.action = ai::ActionType::MoveToPosition;
  owner.feedback.result.type = ai::ActionResultType::Accepted;
  owner.feedback.result.elapsedTime = 0.1f;
  expect(adapter.bindFrame(first, makePerception(first), owner), "feedback frame accepted");
  expect(adapter.infer({ ai::kInferenceInputSchemaVersion, makeObservation(3.0f) }).isSuccess(),
         "initial model decision succeeds");
  expect(backend.last.hasFeedback && backend.last.feedback.decisionFrameId == 19,
         "first feedback forwarded");
  const auto second = makeSelf(6, 21, 3.1f);
  owner.stamp = second.stamp;
  expect(adapter.bindFrame(second, makePerception(second), owner), "new frame accepted");
  expect(adapter.infer({ ai::kInferenceInputSchemaVersion, makeObservation(3.1f) }).isSuccess(),
         "next inference succeeds");
  expect(!backend.last.hasFeedback, "old feedback is not sent again");

  const auto newSession = makeSelf(7, 1, 0.1f);
  owner = makeOwnership(newSession);
  owner.hasFeedback = true;
  owner.feedback.sessionId = 7;
  owner.feedback.decisionFrameId = 1;
  owner.feedback.observedFrameId = 1;
  owner.feedback.result.action = ai::ActionType::MoveToPosition;
  owner.feedback.result.type = ai::ActionResultType::Completed;
  expect(adapter.bindFrame(newSession, makePerception(newSession), owner),
         "new session can rebase feedback watermark");
  expect(adapter.infer({ ai::kInferenceInputSchemaVersion, makeObservation(0.1f) }).isSuccess(),
         "new session inference succeeds");
  expect(backend.last.hasFeedback && backend.last.requestId == 3,
         "first feedback in new session is delivered and requests remain unique");
}

AI_TEST(testModelIORejectsMalformedPerceptionAndFeedback) {
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  const auto self = makeSelf(10, 8, 4.0f);
  auto perception = makePerception(self);
  perception.events[0].event.subjectEntityIndex = 18;
  expect(!adapter.bindFrame(self, perception, makeOwnership(self)),
         "sound source identity must stay anonymous");
  perception = makePerception(self);
  auto owner = makeOwnership(self);
  owner.hasFeedback = true;
  owner.feedback.sessionId = 9;
  owner.feedback.decisionFrameId = 1;
  owner.feedback.observedFrameId = 8;
  owner.feedback.result.action = ai::ActionType::MoveToPosition;
  owner.feedback.result.type = ai::ActionResultType::Accepted;
  expect(!adapter.bindFrame(self, perception, owner), "wrong feedback session rejected");
  expect(!adapter.infer({ ai::kInferenceInputSchemaVersion, makeObservation(4.0f) }).isSuccess(),
         "rejected frame is never sent to model");
}

AI_TEST(testModelIOBridgeUsesExistingActionRuntime) {
  TestBackend backend {};
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  const auto self = makeSelf(15, 3, 2.0f);
  expect(adapter.bindFrame(self, makePerception(self), makeOwnership(self)),
         "live Neural frame bound");

  ai::InferencePolicy policy { &adapter };
  ai::test::TestExecutor executor {};
  ai::ActionRuntime runtime { executor };
  runtime.setMode(ai::ControlMode::Neural);
  runtime.setPolicy(&policy);

  const auto result = runtime.step(makeObservation(2.0f));
  expect(result.type == ai::ActionResultType::Accepted &&
         result.action == ai::ActionType::MoveToPosition,
         "model output reaches existing action pipeline");
  expect(runtime.isActive() && executor.callCount() == 1,
         "runtime uses same executor, no parallel control loop");
  expect(executor.lastAction().targetPosition.x == 48.0f,
         "decoded target preserved through executor");
}
