//
// AiPB - deterministic Model I/O HLDS backend unit tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"
#include "ai_test_tools.h"

#include <limits>

#include <ai/ai_action_runtime.h>
#include <ai/ai_inference_policy.h>
#include <ai/ai_model_io_test_backend.h>
#include <ai/ai_mock_motion.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::ServerBotSnapshot trialSnapshot(uint64_t frame, float time) {
  io::ServerBotSnapshot value {};
  value.stamp.sessionId = 7;
  value.stamp.frameId = frame;
  value.stamp.botEntityIndex = 2;
  value.stamp.roundId = 1;
  value.stamp.frameSeconds = 0.02f;
  value.stamp.gameTime = time;
  value.self.alive = true;
  value.self.onGround = true;
  value.self.maxSpeed = 250.0f;
  value.self.health = 100.0f;
  return value;
}

io::PerceptionFrame trialPerception(const io::ServerBotSnapshot &self) {
  io::PerceptionFrame value {};
  value.self = self;
  return value;
}

io::ControlFrame trialOwner(const io::ServerBotSnapshot &self) {
  io::ControlFrame value {};
  value.stamp = self.stamp;
  value.owner = io::ControlOwner::Neural;
  return value;
}

ai::Observation trialObservation(float time) {
  ai::Observation observation {};
  observation.gameTime = time;
  observation.bot.alive = true;
  return observation;
}

} // namespace

AI_TEST(testModelIOTestBackendDecisionAndTerminalFeedback) {
  io::ModelIOTestBackend backend {};
  expect(backend.arm({ 96.0f, 0.0f, 0.0f }), "backend must be explicitly armed");
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  ai::InferencePolicy policy { &adapter };
  ai::test::TestExecutor executor {};
  ai::ActionRuntime runtime { executor };
  runtime.setMode(ai::ControlMode::Neural);
  runtime.setPolicy(&policy);

  const auto first = trialSnapshot(10, 1.0f);
  expect(adapter.bindFrame(first, trialPerception(first), trialOwner(first)),
         "real-frame envelope accepted");
  const auto accepted = runtime.step(trialObservation(1.0f));
  expect(accepted.action == ai::ActionType::MoveToPosition &&
         accepted.type == ai::ActionResultType::Accepted,
         "model output passes through Policy and ActionRuntime to executor");
  expect(executor.lastAction().targetPosition.x == 96.0f,
         "runtime target exactly matches backend target");
  expect(backend.decided() && backend.requests() == 1 &&
         backend.decisionFrame() == 10 && backend.decisionRequestId() != 0,
         "backend receives original stamped decision");

  const auto second = trialSnapshot(11, 1.1f);
  auto control = trialOwner(second);
  control.hasFeedback = true;
  control.feedback.sessionId = 7;
  control.feedback.decisionFrameId = 10;
  control.feedback.observedFrameId = 10;
  control.feedback.result.action = ai::ActionType::MoveToPosition;
  control.feedback.result.type = ai::ActionResultType::Completed;
  expect(adapter.bindFrame(second, trialPerception(second), control),
         "terminal feedback bound to later frame");
  expect(policy.decide(trialObservation(1.1f)).type == ai::ActionType::None,
         "terminal feedback results in no second model action");
  expect(backend.terminalFeedbackObserved() &&
         backend.feedbackFrame() == 10 &&
         backend.feedbackResult() == ai::ActionResultType::Completed,
         "backend confirms terminal D319 feedback for original decision");
  expect(backend.requests() == 2, "one decision request plus one feedback request");
}

AI_TEST(testModelIOTestBackendRejectsWrongDecisionFeedback) {
  io::ModelIOTestBackend backend {};
  expect(backend.arm({ 96.0f, 0.0f, 0.0f }), "armed for test");
  io::ModelIORequest first {};
  first.requestId = 1;
  first.stamp = trialSnapshot(10, 1.0f).stamp;
  first.self = trialSnapshot(10, 1.0f).self;
  const auto decision = backend.infer(first);
  expect(decision.inference.status == ai::InferenceStatus::Success,
         "first request makes one deterministic decision");

  auto later = first;
  later.requestId = 2;
  later.stamp.frameId = 11;
  later.hasFeedback = true;
  later.feedback.sessionId = 7;
  later.feedback.decisionFrameId = 9;
  later.feedback.observedFrameId = 10;
  later.feedback.result.action = ai::ActionType::MoveToPosition;
  later.feedback.result.type = ai::ActionResultType::Completed;
  backend.infer(later);
  expect(!backend.terminalFeedbackObserved(), "foreign decision cannot complete test");
  later.requestId = 3;
  later.feedback.decisionFrameId = 10;
  later.feedback.observedFrameId = 11;
  backend.infer(later);
  expect(!backend.terminalFeedbackObserved(), "future feedback cannot complete test");
  later.requestId = 4;
  later.feedback.observedFrameId = 10;
  later.feedback.result.type = ai::ActionResultType::Accepted;
  backend.infer(later);
  expect(!backend.terminalFeedbackObserved(), "nonterminal feedback insufficient");
  later.requestId = 5;
  later.feedback.result.type = ai::ActionResultType::Completed;
  backend.infer(later);
  expect(backend.terminalFeedbackObserved(), "valid correlated terminal feedback accepted");
  expect(backend.requests() == 5, "all requests counted deterministically");
}

AI_TEST(testModelIOTestBackendRequiresArmAndHonorsReset) {
  io::ModelIOTestBackend backend {};
  io::ModelIORequest input {};
  input.requestId = 1;
  input.stamp = trialSnapshot(10, 1.0f).stamp;
  input.self = trialSnapshot(10, 1.0f).self;
  expect(backend.infer(input).inference.status == ai::InferenceStatus::InvalidInput,
         "unarmed backend never controls bot");
  expect(!backend.arm({ std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f }),
         "nonfinite target rejected");
  expect(backend.arm({ 40.0f, 0.0f, 0.0f }), "finite target accepted");
  expect(!backend.arm({ 80.0f, 0.0f, 0.0f }), "no concurrent arm");
  backend.reset();
  expect(!backend.decided() && !backend.terminalFeedbackObserved() &&
         backend.requests() == 0, "reset removes all prior decision and feedback state");
  expect(backend.arm({ 80.0f, 0.0f, 0.0f }), "re-arm accepted after reset");
  expect(backend.infer(input).inference.status == ai::InferenceStatus::Success,
         "new run produces fresh decision");
}
