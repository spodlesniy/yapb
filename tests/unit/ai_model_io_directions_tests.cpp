//
// AiPB - trace-authorized external steering tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <limits>

#include <ai/ai_model_io_directions.h>
#include <ai/ai_model_io_external_trial_backend.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

class DirectionSource final : public io::ModelIOBackend {
public:
  ai::InferenceActionOutput output {};
  ai::InferenceStatus status { ai::InferenceStatus::Success };

  io::ModelIOResponse infer(const io::ModelIORequest &request) const override {
    io::ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = status;
    response.inference.output = output;
    return response;
  }
};

io::ModelIORequest frame(uint64_t id = 10) {
  io::ModelIORequest request {};
  request.requestId = id;
  request.stamp.sessionId = 9;
  request.stamp.frameId = id;
  request.stamp.botEntityIndex = 1;
  request.stamp.gameTime = static_cast<float>(id) * 0.01f;
  request.stamp.frameSeconds = 0.01f;
  request.self.alive = true;
  request.self.onGround = true;
  request.self.maxSpeed = 250.0f;
  request.self.health = 100.0f;
  return request;
}

void move(DirectionSource &source, const ai::Vec3 &target) {
  source.status = ai::InferenceStatus::Success;
  source.output = {};
  source.output.actionId = static_cast<uint8_t>(ai::InferenceActionId::MoveToPosition);
  source.output.targetPosition = target;
  source.output.confidence = 1.0f;
}

} // namespace

AI_TEST(testModelIODirectionTargetsUseLocalYaw) {
  const ai::Vec3 origin { 100.0f, 200.0f, 32.0f };
  const auto forward = io::modelIODirectionTarget(origin, 0.0f, 0);
  const auto left = io::modelIODirectionTarget(origin, 0.0f, 1);
  const auto right = io::modelIODirectionTarget(origin, 0.0f, 2);
  expect(forward.x > 195.9f && forward.y == 200.0f,
         "forward target extends 96 units along view yaw");
  expect(left.y > 295.9f && right.y < 104.1f,
         "left and right are perpendicular to the same observed yaw");
  expect(forward.z == 32.0f && left.z == 32.0f && right.z == 32.0f,
         "all candidates stay at the observed standing height");
}

AI_TEST(testModelIODirectionsRequireTraceAuthorization) {
  io::ModelIODirectionCandidates choices {};
  const ai::Vec3 left { 100.0f, 296.0f, 32.0f };
  const ai::Vec3 right { 100.0f, 104.0f, 32.0f };
  expect(choices.match(left) == -1 && choices.first() == -1,
         "nominal geometry alone cannot authorize motion");
  expect(!choices.allow(3, left) && !choices.allow(1,
         { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f }),
         "invalid direction and nonfinite endpoint rejected");
  expect(choices.allow(1, left) && !choices.allow(1, left) &&
         choices.allow(2, right) && choices.count() == 2,
         "only unique engine-traced candidates can be admitted");
  expect(choices.match(left) == 1 && choices.match(right) == 2 &&
         choices.match({ 196.0f, 200.0f, 32.0f }) == -1,
         "blocked forward direction is never admitted by matching");
  expect(choices.match({ 100.0f, 296.8f, 32.0f }) == -1,
         "target outside float-rounding tolerance is rejected");
}

AI_TEST(testModelIOExternalSteerAcceptsOnlyTracedDirection) {
  DirectionSource source {};
  io::ModelIOExternalTrialBackend trial(source);
  io::ModelIODirectionCandidates choices {};
  const ai::Vec3 forward { 196.0f, 200.0f, 32.0f };
  const ai::Vec3 left { 100.0f, 296.0f, 32.0f };
  expect(!trial.armCandidates(choices), "empty safety allowlist denied");
  expect(choices.allow(0, forward) && choices.allow(1, left),
         "engine trace admits forward and left");
  expect(trial.armCandidates(choices), "bounded steering trial armed");
  move(source, { 100.3f, 296.1f, 32.0f });
  const auto decision = trial.infer(frame());
  expect(decision.inference.status == ai::InferenceStatus::Success &&
         trial.chosenDirection() == 1 && trial.decided() &&
         decision.inference.output.targetPosition.x == 100.0f &&
         decision.inference.output.targetPosition.y == 296.0f,
         "remote left choice maps to exact engine-authorized endpoint");
  source.status = ai::InferenceStatus::NoDecision;
  source.output = {};
  auto feedback = frame(12);
  feedback.hasFeedback = true;
  feedback.feedback.sessionId = 9;
  feedback.feedback.decisionFrameId = 10;
  feedback.feedback.observedFrameId = 11;
  feedback.feedback.result.action = ai::ActionType::MoveToPosition;
  feedback.feedback.result.type = ai::ActionResultType::Completed;
  expect(trial.infer(feedback).inference.status == ai::InferenceStatus::NoDecision &&
         trial.terminalFeedbackObserved() &&
         trial.feedbackResult() == ai::ActionResultType::Completed,
         "verified steering keeps original terminal-feedback correlation");
  trial.reset();
  expect(!trial.decided() && trial.chosenDirection() == -1,
         "reset discards previous choice");
}

AI_TEST(testModelIOExternalSteerRejectsBlockedAndUnauthorizedTargets) {
  DirectionSource source {};
  io::ModelIODirectionCandidates choices {};
  expect(choices.allow(1, { 100.0f, 296.0f, 32.0f }),
         "only left direction clears geometry");
  io::ModelIOExternalTrialBackend trial(source);
  expect(trial.armCandidates(choices), "arm with restricted allowlist");
  move(source, { 196.0f, 200.0f, 32.0f });
  expect(trial.infer(frame()).inference.status == ai::InferenceStatus::Error &&
         !trial.decided(), "untraced forward decision fails closed");
  move(source, { 100.0f, 104.0f, 32.0f });
  expect(trial.infer(frame()).inference.status == ai::InferenceStatus::Error &&
         !trial.decided(), "blocked right direction fails closed");
  move(source, { 100.0f, 296.0f, 32.0f });
  source.output.actionId = static_cast<uint8_t>(ai::InferenceActionId::Fire);
  expect(trial.infer(frame()).inference.status == ai::InferenceStatus::Error &&
         !trial.decided(), "semantic attack action not authorized");
  move(source, { 100.0f, 296.0f, 32.0f });
  expect(trial.infer(frame()).inference.status == ai::InferenceStatus::Success &&
         trial.chosenDirection() == 1, "valid turn still accepted after rejection");
  move(source, { 100.0f, 296.0f, 32.0f });
  expect(trial.infer(frame(11)).inference.status == ai::InferenceStatus::Error,
         "second movement decision remains forbidden");
}
