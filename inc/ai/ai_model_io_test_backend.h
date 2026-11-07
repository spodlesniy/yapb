//
// AiPB - deterministic one-shot Model I/O backend for HLDS integration tests.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_model_io.h>

namespace ai::runtime_io {

// This backend is explicitly armed by a server-only trial. It is not a
// replacement for the configured model, and performs no engine operations.
class ModelIOTestBackend final : public ModelIOBackend {
private:
  Vec3 m_target {};
  bool m_armed {};
  mutable bool m_decided {};
  mutable bool m_terminalFeedback {};
  mutable uint32_t m_requests {};
  mutable uint64_t m_sessionId {};
  mutable uint64_t m_decisionFrame {};
  mutable uint64_t m_decisionRequestId {};
  mutable uint64_t m_feedbackFrame {};
  mutable ActionResultType m_feedbackResult { ActionResultType::None };

public:
  void reset() {
    m_target = {};
    m_armed = false;
    m_decided = false;
    m_terminalFeedback = false;
    m_requests = 0;
    m_sessionId = 0;
    m_decisionFrame = 0;
    m_decisionRequestId = 0;
    m_feedbackFrame = 0;
    m_feedbackResult = ActionResultType::None;
  }

  bool arm(const Vec3 &target) {
    if (m_armed || !finiteVector(target)) {
      return false;
    }
    reset();
    m_target = target;
    m_armed = true;
    return true;
  }

  ModelIOResponse infer(const ModelIORequest &request) const override {
    ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = InferenceStatus::NoDecision;

    ++m_requests;
    if (!m_armed || request.schemaVersion != kModelIOSchemaVersion ||
        request.requestId == 0 ||
        validateFrame(request.stamp) != FrameValidation::Valid ||
        !validSelfState(request.self) || !request.self.alive ||
        !request.features.hasSupportedSchema() ||
        request.eventCount > kPerceptionEventsPerFrame) {
      response.inference.status = InferenceStatus::InvalidInput;
      return response;
    }

    if (!m_decided) {
      m_decided = true;
      m_sessionId = request.stamp.sessionId;
      m_decisionFrame = request.stamp.frameId;
      m_decisionRequestId = request.requestId;
      response.inference.status = InferenceStatus::Success;
      response.inference.output.actionId = static_cast<uint8_t>(InferenceActionId::MoveToPosition);
      response.inference.output.targetPosition = m_target;
      response.inference.output.confidence = 1.0f;
      return response;
    }

    // The next model request after the action terminates must carry D319's
    // feedback referencing the first decision, not just a local success flag.
    if (request.stamp.sessionId == m_sessionId && request.stamp.frameId > m_decisionFrame &&
        request.hasFeedback && request.feedback.sessionId == m_sessionId &&
        request.feedback.decisionFrameId == m_decisionFrame &&
        request.feedback.observedFrameId >= m_decisionFrame &&
        request.feedback.observedFrameId < request.stamp.frameId &&
        request.feedback.result.action == ActionType::MoveToPosition &&
        request.feedback.result.isTerminal()) {
      m_terminalFeedback = true;
      m_feedbackFrame = request.feedback.observedFrameId;
      m_feedbackResult = request.feedback.result.type;
    }
    return response;
  }

  bool decided() const {
    return m_decided;
  }
  bool terminalFeedbackObserved() const {
    return m_terminalFeedback;
  }
  uint32_t requests() const {
    return m_requests;
  }
  uint64_t sessionId() const {
    return m_sessionId;
  }
  uint64_t decisionFrame() const {
    return m_decisionFrame;
  }
  uint64_t decisionRequestId() const {
    return m_decisionRequestId;
  }
  uint64_t feedbackFrame() const {
    return m_feedbackFrame;
  }
  ActionResultType feedbackResult() const {
    return m_feedbackResult;
  }
};

} // namespace ai::runtime_io
