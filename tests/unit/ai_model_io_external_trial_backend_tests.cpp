//
// AiPB - constrained external Model I/O single-bot trial tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_model_io_external_trial_backend.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

class ExternalSource final : public io::ModelIOBackend {
public:
  mutable unsigned calls {};
  ai::InferenceStatus status { ai::InferenceStatus::Success };
  ai::InferenceActionOutput output {};
  bool foreignSession {};

  io::ModelIOResponse infer(const io::ModelIORequest &request) const override {
    ++calls;
    io::ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = foreignSession ? request.stamp.sessionId + 1 : request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = status;
    response.inference.output = output;
    return response;
  }
};

io::ModelIORequest input(uint64_t frame = 10) {
  io::ModelIORequest request {};
  request.requestId = frame;
  request.stamp.sessionId = 7;
  request.stamp.frameId = frame;
  request.stamp.roundId = 1;
  request.stamp.botEntityIndex = 2;
  request.stamp.gameTime = static_cast<float>(frame) * 0.02f;
  request.stamp.frameSeconds = 0.02f;
  request.self.alive = true;
  request.self.onGround = true;
  request.self.maxSpeed = 250.0f;
  request.self.health = 100.0f;
  return request;
}

void movement(ExternalSource &source) {
  source.status = ai::InferenceStatus::Success;
  source.output.actionId = static_cast<uint8_t>(ai::InferenceActionId::MoveToPosition);
  source.output.targetPosition = { 106.0f, 20.0f, 30.0f };
  source.output.confidence = 1.0f;
}

} // namespace

AI_TEST(testExternalTrialRequiresAuthorizedTargetAndSingleDecision) {
  ExternalSource source {};
  movement(source);
  io::ModelIOExternalTrialBackend trial(source);
  const auto first = input();
  expect(trial.infer(first).inference.status == ai::InferenceStatus::Error &&
         source.calls == 0, "unarmed trial must not call external source");
  expect(trial.arm({ 106.0f, 20.0f, 30.0f }), "explicitly authorized traced target");
  expect(!trial.arm({ 106.0f, 20.0f, 30.0f }), "cannot arm twice");
  const auto decision = trial.infer(first);
  expect(decision.inference.status == ai::InferenceStatus::Success &&
         decision.inference.output.targetPosition.x == 106.0f &&
         trial.decided() && trial.decisionFrame() == 10 &&
         trial.requests() == 1, "validated first remote action admitted once");
  source.status = ai::InferenceStatus::NoDecision;
  source.output = {};
  const auto later = trial.infer(input(11));
  expect(later.inference.status == ai::InferenceStatus::NoDecision &&
         trial.requests() == 2 && source.calls == 2,
         "second decision cannot be issued after first target");
}

AI_TEST(testExternalTrialRejectsOtherActionsAndLocations) {
  ExternalSource source {};
  io::ModelIOExternalTrialBackend trial(source);
  movement(source);
  source.output.targetPosition.x = 120.0f;
  expect(trial.arm({ 106.0f, 20.0f, 30.0f }), "trial armed");
  expect(trial.infer(input()).inference.status == ai::InferenceStatus::Error &&
         !trial.decided(), "unsafe remote target rejected before motion");
  source.output.targetPosition.x = 106.0f;
  source.output.actionId = static_cast<uint8_t>(ai::InferenceActionId::Fire);
  expect(trial.infer(input()).inference.status == ai::InferenceStatus::Error &&
         !trial.decided(), "non-movement remote action rejected");
  movement(source);
  source.output.targetNode = 5;
  expect(trial.infer(input()).inference.status == ai::InferenceStatus::Error &&
         !trial.decided(), "hidden navigation target rejected");
  source.output.targetNode = -1;
  source.foreignSession = true;
  expect(trial.infer(input()).inference.status == ai::InferenceStatus::Error &&
         !trial.decided(), "foreign session rejected");
  source.foreignSession = false;
  expect(trial.infer(input()).inference.status == ai::InferenceStatus::Success &&
         trial.decided(), "correct source may recover before any action");
  trial.reset();
  expect(!trial.decided() && !trial.terminalFeedbackObserved() &&
         trial.requests() == 0,
         "reset clears per-session state");
}

AI_TEST(testExternalTrialRequiresTerminalFeedbackAfterNoDecision) {
  ExternalSource source {};
  movement(source);
  io::ModelIOExternalTrialBackend trial(source);
  expect(trial.arm({ 106.0f, 20.0f, 30.0f }), "trial armed");
  expect(trial.infer(input()).inference.status == ai::InferenceStatus::Success,
         "first external decision accepted");
  source.status = ai::InferenceStatus::NoDecision;
  source.output = {};
  auto later = input(13);
  later.hasFeedback = true;
  later.feedback.sessionId = 7;
  later.feedback.decisionFrameId = 10;
  later.feedback.observedFrameId = 12;
  later.feedback.result.action = ai::ActionType::MoveToPosition;
  later.feedback.result.type = ai::ActionResultType::Completed;
  expect(trial.infer(later).inference.status == ai::InferenceStatus::NoDecision &&
         trial.terminalFeedbackObserved() &&
         trial.feedbackFrame() == 12 &&
         trial.feedbackResult() == ai::ActionResultType::Completed,
         "one terminal result must be acknowledged by later remote request");
  source.status = ai::InferenceStatus::Success;
  movement(source);
  expect(trial.infer(input(14)).inference.status == ai::InferenceStatus::Error,
         "replayed second remote movement cannot take control");
}

AI_TEST(testExternalTrialAcceptsSameFrameTerminalFeedbackAndDiagnosesMissing) {
  ExternalSource source {};
  movement(source);
  io::ModelIOExternalTrialBackend trial(source);
  expect(trial.arm({ 106.0f, 20.0f, 30.0f }), "trial armed");
  expect(trial.infer(input(10)).inference.status == ai::InferenceStatus::Success,
         "first decision accepted");
  source.status = ai::InferenceStatus::NoDecision;
  source.output = {};
  auto later = input(11);
  expect(trial.infer(later).inference.status == ai::InferenceStatus::NoDecision &&
         !trial.terminalFeedbackObserved() && !trial.lastRequestHadFeedback(),
         "missing terminal result remains visible for diagnostics");
  later = input(12);
  later.hasFeedback = true;
  later.feedback.sessionId = 7;
  later.feedback.decisionFrameId = 10;
  later.feedback.observedFrameId = 10;
  later.feedback.result.action = ai::ActionType::MoveToPosition;
  later.feedback.result.type = ai::ActionResultType::Completed;
  expect(trial.infer(later).inference.status == ai::InferenceStatus::NoDecision &&
         trial.terminalFeedbackObserved() && trial.feedbackFrame() == 10 &&
         trial.lastRequestHadFeedback() && trial.lastFeedbackDecisionFrame() == 10 &&
         trial.lastFeedbackObservedFrame() == 10 &&
         trial.lastSourceStatus() == ai::InferenceStatus::NoDecision,
         "terminal result observed in decision frame is valid as in D323");
}
