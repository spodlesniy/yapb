//
// AiPB - stamped model I/O bridge over the existing inference contract.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

#include <ai/ai_execution_ownership.h>
#include <ai/ai_inference_features.h>
#include <ai/ai_inference_provider.h>
#include <ai/ai_perception_events.h>

namespace ai::runtime_io {

constexpr uint32_t kModelIOSchemaVersion = 1;

// Only permitted D316 self state, D317 sensory evidence, and the existing
// sanitized inference features cross this boundary. No Bot/GoldSrc pointers.
struct ModelIORequest {
  uint32_t schemaVersion { kModelIOSchemaVersion };
  uint64_t requestId {};
  FrameStamp stamp {};
  BotSelfState self {};
  VisibleEnemy visibleEnemy {};
  PerceptionEvent events[kPerceptionEventsPerFrame] {};
  uint8_t eventCount {};
  bool hasFeedback {};
  ActionFeedback feedback {};
  InferenceFeatures features {};
  // D335 local affordances are only advertised in the opt-in wire-v2 request.
  uint8_t safeDirectionsMask {};
};

struct ModelIOResponse {
  // Wire v1 remains the default for existing trial modes.
  uint16_t wireVersion { 1 };
  uint32_t schemaVersion { kModelIOSchemaVersion };
  uint64_t requestId {};
  uint64_t sessionId {};
  uint64_t observationFrameId {};
  InferenceResult inference {};
};

// Synchronous, backend-neutral model endpoint. A future external model
// transport may implement this interface without touching ActionRuntime.
class ModelIOBackend {
public:
  virtual ~ModelIOBackend() = default;
  virtual ModelIOResponse infer(const ModelIORequest &request) const = 0;
};

inline bool sameModelIOFrame(const FrameStamp &a, const FrameStamp &b) {
  return a.schemaVersion == b.schemaVersion && a.sessionId == b.sessionId &&
      a.frameId == b.frameId && a.roundId == b.roundId &&
      a.botEntityIndex == b.botEntityIndex && a.gameTime == b.gameTime;
}

inline bool modelIOResponseMatches(const ModelIORequest &request, const ModelIOResponse &response) {
  return response.schemaVersion == kModelIOSchemaVersion &&
      request.schemaVersion == kModelIOSchemaVersion && request.requestId != 0 &&
      response.requestId == request.requestId &&
      response.sessionId == request.stamp.sessionId &&
      response.observationFrameId == request.stamp.frameId;
}

// Fail closed on any event not exported by D317 before an external backend
// can see it. An absent position must not carry a latent hidden coordinate.
inline bool validModelIOPerception(const PerceptionFrame &perception, const FrameStamp &stamp) {
  const auto &seen = perception.visibleEnemy;
  if (perception.eventCount > kPerceptionEventsPerFrame ||
      (seen.valid
          ? (seen.entityIndex <= 0 || seen.entityIndex == stamp.botEntityIndex ||
             !finiteVector(seen.position))
          : (seen.entityIndex != -1 || seen.position.x != 0.0f ||
             seen.position.y != 0.0f || seen.position.z != 0.0f))) {
    return false;
  }

  for (uint8_t i = 0; i < perception.eventCount; ++i) {
    const auto &entry = perception.events[i];
    const auto &event = entry.event;
    if (!sameModelIOFrame(event.stamp, stamp) || !finiteVector(event.position) ||
        (!entry.hasPosition && (event.position.x != 0.0f ||
          event.position.y != 0.0f || event.position.z != 0.0f))) {
      return false;
    }

    switch (event.kind) {
    case EventKind::RoundStarted:
    case EventKind::RoundEnded:
      if (event.evidence != Evidence::PublicEvent ||
          event.subjectEntityIndex != -1 || entry.hasPosition) {
        return false;
      }
      break;
    case EventKind::EntityDisappeared:
      if (event.evidence != Evidence::Remembered ||
          event.subjectEntityIndex <= 0 ||
          event.subjectEntityIndex == stamp.botEntityIndex || entry.hasPosition) {
        return false;
      }
      break;
    case EventKind::EntityAppeared:
      if (event.evidence != Evidence::Seen || !seen.valid ||
          event.subjectEntityIndex != seen.entityIndex || !entry.hasPosition ||
          event.position.x != seen.position.x || event.position.y != seen.position.y ||
          event.position.z != seen.position.z) {
        return false;
      }
      break;
    case EventKind::Sound:
      if (event.evidence != Evidence::Heard ||
          event.subjectEntityIndex != -1 || !entry.hasPosition) {
        return false;
      }
      break;
    default:
      return false;
    }
  }
  return true;
}

// Adapts a stamped independent model backend to the existing InferencePolicy.
// The latter remains responsible for action decoding and ActionValidator.
// All state is per-bot: do not share an instance across multiple bot runtimes.
class ModelIOAdapter final : public InferenceProvider {
private:
  const ModelIOBackend *m_backend {};
  ModelIORequest m_bound {};
  FrameStamp m_lastBound {};
  bool m_haveLastBound {};
  bool m_ready {};
  mutable bool m_issued {};
  mutable uint64_t m_nextRequestId {};
  mutable uint64_t m_lastFeedbackForwarded {};
  mutable uint64_t m_feedbackSession {};

  static InferenceResult unavailable(InferenceStatus status) {
    InferenceResult result {};
    result.status = status;
    return result;
  }

public:
  void setBackend(const ModelIOBackend *backend) {
    if (m_backend == backend) {
      return;
    }
    reset();
    m_backend = backend;
  }

  const ModelIOBackend *getBackend() const {
    return m_backend;
  }

  // Preserve configured backend; invalidate per-session I/O identity.
  void invalidateFrame() {
    m_ready = false;
    m_issued = true;
  }

  void reset() {
    m_bound = ModelIORequest {};
    m_lastBound = {};
    m_haveLastBound = false;
    m_ready = false;
    m_issued = false;
    m_nextRequestId = 0;
    m_lastFeedbackForwarded = 0;
    m_feedbackSession = 0;
  }

  bool bindFrame(const ServerBotSnapshot &self, const PerceptionFrame &perception,
                 const ControlFrame &control) {
    m_ready = false;
    m_issued = false;
    const auto &stamp = self.stamp;
    if (m_backend == nullptr || validateFrame(stamp) != FrameValidation::Valid ||
        !validSelfState(self.self) || !sameModelIOFrame(perception.self.stamp, stamp) ||
        !sameModelIOFrame(control.stamp, stamp) || control.owner != ControlOwner::Neural ||
        !validModelIOPerception(perception, stamp) ||
        (m_haveLastBound && (stamp.sessionId < m_lastBound.sessionId ||
            (stamp.sessionId == m_lastBound.sessionId &&
             validateFrame(stamp, &m_lastBound) != FrameValidation::Valid)))) {
      return false;
    }

    // An old feedback may remain in D319's ledger across multiple frames:
    // deliver each observed action result at most once.
    if (m_feedbackSession != stamp.sessionId) {
      m_feedbackSession = stamp.sessionId;
      m_lastFeedbackForwarded = 0;
    }

    m_bound = ModelIORequest {};
    m_bound.stamp = stamp;
    m_bound.self = self.self;
    m_bound.visibleEnemy = perception.visibleEnemy;
    m_bound.eventCount = perception.eventCount;
    for (uint8_t i = 0; i < perception.eventCount; ++i) {
      m_bound.events[i] = perception.events[i];
    }
    if (control.hasFeedback) {
      const auto &feedback = control.feedback;
      if (feedback.sessionId != stamp.sessionId || feedback.decisionFrameId == 0 ||
          feedback.decisionFrameId > feedback.observedFrameId ||
          feedback.observedFrameId > stamp.frameId ||
          !std::isfinite(feedback.result.elapsedTime) ||
          (feedback.result.type != ActionResultType::Accepted && !feedback.result.isTerminal())) {
        return false;
      }
      if (feedback.observedFrameId > m_lastFeedbackForwarded) {
        m_bound.feedback = feedback;
        m_bound.hasFeedback = true;
      }
    }

    m_lastBound = stamp;
    m_haveLastBound = true;
    m_ready = true;
    return true;
  }

  InferenceResult infer(const InferenceInput &input) const override {
    if (m_backend == nullptr || !m_ready || m_issued) {
      return unavailable(InferenceStatus::NoDecision);
    }
    if (!input.hasSupportedSchema() ||
        !std::isfinite(input.observation.gameTime) ||
        std::abs(input.observation.gameTime - m_bound.stamp.gameTime) > 0.01f) {
      return unavailable(InferenceStatus::InvalidInput);
    }
    m_issued = true;
    if (m_nextRequestId == std::numeric_limits<uint64_t>::max()) {
      return unavailable(InferenceStatus::Error);
    }
    ModelIORequest request = m_bound;
    request.requestId = ++m_nextRequestId;
    request.features = encodeInferenceFeatures(input.observation);
    if (request.hasFeedback) {
      m_lastFeedbackForwarded = request.feedback.observedFrameId;
    }

    const auto response = m_backend->infer(request);
    if (!modelIOResponseMatches(request, response)) {
      return unavailable(InferenceStatus::Error);
    }
    switch (response.inference.status) {
    case InferenceStatus::Success:
      if (!response.inference.output.hasSupportedSchema()) {
        return unavailable(InferenceStatus::Error);
      }
      return response.inference;
    case InferenceStatus::NoDecision:
    case InferenceStatus::InvalidInput:
    case InferenceStatus::Error:
      return response.inference;
    }
    return unavailable(InferenceStatus::Error);
  }
};

} // namespace ai::runtime_io
