//
// AiPB - constrained external Model I/O trial backend for one Neural bot.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstdint>

#include <ai/ai_model_io.h>

namespace ai::runtime_io {

// The external process is allowed to confirm one previously traced target,
// not choose arbitrary actions or destinations in the isolated HLDS trial.
class ModelIOExternalTrialBackend final : public ModelIOBackend {
private:
  const ModelIOBackend &m_source;
  Vec3 m_expected {};
  bool m_armed {};
  mutable bool m_decided {};
  mutable bool m_terminalFeedback {};
  mutable uint32_t m_requests {};
  mutable uint64_t m_sessionId {};
  mutable uint64_t m_decisionFrame {};
  mutable uint64_t m_feedbackFrame {};
  mutable ActionResultType m_feedbackResult { ActionResultType::None };

  static ModelIOResponse error(const ModelIORequest &request) {
    ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = InferenceStatus::Error;
    return response;
  }

  bool safeDecision(const InferenceActionOutput &action) const {
    constexpr float kPositionTolerance = 0.75f;
    return action.hasSupportedSchema() &&
        action.actionId == static_cast<uint8_t>(InferenceActionId::MoveToPosition) &&
        action.targetNode == -1 && action.targetPlayer == -1 &&
        action.weaponType == static_cast<uint8_t>(WeaponType::Unknown) &&
        action.grenadeType == static_cast<uint8_t>(GrenadeType::None) &&
        action.duration == 0.0f && std::isfinite(action.confidence) &&
        action.confidence > 0.0f && action.confidence <= 1.0f &&
        finiteVector(action.targetPosition) &&
        std::abs(action.targetPosition.x - m_expected.x) <= kPositionTolerance &&
        std::abs(action.targetPosition.y - m_expected.y) <= kPositionTolerance &&
        std::abs(action.targetPosition.z - m_expected.z) <= kPositionTolerance;
  }

public:
  explicit ModelIOExternalTrialBackend(const ModelIOBackend &source) : m_source(source) {}

  void reset() {
    m_expected = {};
    m_armed = false;
    m_decided = false;
    m_terminalFeedback = false;
    m_requests = 0;
    m_sessionId = 0;
    m_decisionFrame = 0;
    m_feedbackFrame = 0;
    m_feedbackResult = ActionResultType::None;
  }

  bool arm(const Vec3 &expectedTarget) {
    if (m_armed || !finiteVector(expectedTarget)) {
      return false;
    }
    reset();
    m_expected = expectedTarget;
    m_armed = true;
    return true;
  }

  ModelIOResponse infer(const ModelIORequest &request) const override {
    ++m_requests;
    if (!m_armed || request.requestId == 0 ||
        validateFrame(request.stamp) != FrameValidation::Valid ||
        !validSelfState(request.self) || !request.self.alive) {
      return error(request);
    }

    const auto response = m_source.infer(request);
    if (!modelIOResponseMatches(request, response)) {
      return error(request);
    }

    if (!m_decided) {
      if (response.inference.status != InferenceStatus::Success ||
          !safeDecision(response.inference.output)) {
        return error(request);
      }
      m_decided = true;
      m_sessionId = request.stamp.sessionId;
      m_decisionFrame = request.stamp.frameId;
      // Keep the original, traced destination authoritative even when
      // the remote side rounds its single-precision trigonometry.
      auto verified = response;
      verified.inference.output.targetPosition = m_expected;
      return verified;
    }

    if (request.stamp.sessionId != m_sessionId ||
        request.stamp.frameId <= m_decisionFrame ||
        response.inference.status != InferenceStatus::NoDecision ||
        response.inference.output.actionId != static_cast<uint8_t>(InferenceActionId::None)) {
      return error(request);
    }

    if (request.hasFeedback && request.feedback.sessionId == m_sessionId &&
        request.feedback.decisionFrameId == m_decisionFrame &&
        request.feedback.observedFrameId > m_decisionFrame &&
        request.feedback.observedFrameId < request.stamp.frameId &&
        request.feedback.result.action == ActionType::MoveToPosition &&
        request.feedback.result.isTerminal()) {
      m_terminalFeedback = true;
      m_feedbackFrame = request.feedback.observedFrameId;
      m_feedbackResult = request.feedback.result.type;
    }
    return response;
  }

  bool decided() const { return m_decided; }
  bool terminalFeedbackObserved() const { return m_terminalFeedback; }
  uint32_t requests() const { return m_requests; }
  uint64_t sessionId() const { return m_sessionId; }
  uint64_t decisionFrame() const { return m_decisionFrame; }
  uint64_t feedbackFrame() const { return m_feedbackFrame; }
  ActionResultType feedbackResult() const { return m_feedbackResult; }
};

} // namespace ai::runtime_io
