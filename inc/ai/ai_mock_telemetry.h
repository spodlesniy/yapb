//
// AiPB - bounded self-only Mock telemetry and deterministic audit replay.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <ai/ai_mock_motion.h>
#include <ai/ai_mock_policy.h>
#include <ai/ai_server_state.h>

namespace ai::mock {

constexpr size_t kMockTraceCapacity = 512;
constexpr float kMockTraceDefaultIntervalSeconds = 0.025f;
constexpr float kMockTraceMinIntervalSeconds = 0.020f;
constexpr float kMockTraceMaxIntervalSeconds = 0.050f;

enum class ReplayStatus : uint8_t {
  Pass,
  Empty,
  Incomplete,
  Cancelled,
  Overflow,
  InvalidFrame,
  InvalidAction,
  MissingFeedback,
  InvalidFeedback,
  InvalidCompletion,
};

inline const char *replayStatusName(ReplayStatus status) {
  switch (status) {
  case ReplayStatus::Pass: return "PASS";
  case ReplayStatus::Empty: return "Empty";
  case ReplayStatus::Incomplete: return "Incomplete";
  case ReplayStatus::Cancelled: return "Cancelled";
  case ReplayStatus::Overflow: return "Overflow";
  case ReplayStatus::InvalidFrame: return "InvalidFrame";
  case ReplayStatus::InvalidAction: return "InvalidAction";
  case ReplayStatus::MissingFeedback: return "MissingFeedback";
  case ReplayStatus::InvalidFeedback: return "InvalidFeedback";
  case ReplayStatus::InvalidCompletion: return "InvalidCompletion";
  }
  return "Unknown";
}

struct TraceRecord {
  runtime_io::ServerBotSnapshot observed {};
  ActionResult result {};
  runtime_io::ActionFeedback feedback {};
  bool hasFeedback {};
};

struct ReplayReport {
  ReplayStatus status { ReplayStatus::Empty };
  size_t frames {};
  uint64_t observedFrames {};
  size_t skippedFrames {};
  size_t capacitySkipped {};
  float sampleIntervalSeconds {};
  uint64_t decisionFrameId {};
  uint64_t terminalFrameId {};
  float elapsedSeconds {};
  float displacement {};
  float remainingDistance {};
  ActionResultType terminalResult { ActionResultType::None };
};

// Replay verifies the recorded policy decision, lifecycle, result, and self
// coordinates. It never executes HLDS movement or simulates GoldSrc physics.
class MockTrace final {
private:
  TraceRecord m_records[kMockTraceCapacity] {};
  size_t m_count {};
  uint64_t m_observedFrames {};
  size_t m_capacitySkipped {};
  float m_sampleInterval { kMockTraceDefaultIntervalSeconds };
  runtime_io::FrameStamp m_lastObservedStamp {};
  ActionResultType m_lastObservedResult { ActionResultType::None };
  Vec3 m_target {};
  runtime_io::QueryStatus m_preflight { runtime_io::QueryStatus::Unknown };
  bool m_finished {};
  bool m_cancelled {};
  bool m_overflow {};
  bool m_invalid {};

  static float distance(const Vec3 &a, const Vec3 &b) {
    return mockDistanceToTarget(a, b);
  }

public:
  // Configuration is per MockTrace instance and persists across run resets.
  bool setSampleIntervalSeconds(float seconds) {
    if (!std::isfinite(seconds) || seconds < kMockTraceMinIntervalSeconds ||
        seconds > kMockTraceMaxIntervalSeconds || (m_count != 0 && !m_finished)) {
      return false;
    }
    m_sampleInterval = seconds;
    return true;
  }

  float sampleIntervalSeconds() const {
    return m_sampleInterval;
  }

  uint64_t observedFrames() const {
    return m_observedFrames;
  }

  size_t capacitySkipped() const {
    return m_capacitySkipped;
  }

  void reset() {
    m_count = 0;
    m_observedFrames = 0;
    m_capacitySkipped = 0;
    m_lastObservedStamp = {};
    m_lastObservedResult = ActionResultType::None;
    m_target = {};
    m_preflight = runtime_io::QueryStatus::Unknown;
    m_finished = false;
    m_cancelled = false;
    m_overflow = false;
    m_invalid = false;
  }

  bool begin(const runtime_io::ServerBotSnapshot &self, const Vec3 &target,
             runtime_io::QueryStatus preflight) {
    reset();
    if (preflight != runtime_io::QueryStatus::Completed ||
        runtime_io::validateFrame(self.stamp) != runtime_io::FrameValidation::Valid ||
        !runtime_io::validSelfState(self.self) || !self.self.alive ||
        !runtime_io::finiteVector(target)) {
      return false;
    }
    m_target = target;
    m_preflight = preflight;
    m_records[0] = {};
    m_records[0].observed = self;
    m_count = 1;
    m_observedFrames = 1;
    m_lastObservedStamp = self.stamp;
    return true;
  }

  bool append(const runtime_io::ServerBotSnapshot &self, const ActionResult &result,
              const runtime_io::ActionFeedback *feedback) {
    if (m_count == 0 || m_finished || m_invalid || m_overflow) {
      return false;
    }
    const auto &first = m_records[0].observed.stamp;
    if (self.stamp.sessionId != first.sessionId ||
        self.stamp.roundId != first.roundId ||
        self.stamp.botEntityIndex != first.botEntityIndex ||
        runtime_io::validateFrame(self.stamp, &m_lastObservedStamp) != runtime_io::FrameValidation::Valid ||
        !runtime_io::validSelfState(self.self)) {
      m_invalid = true;
      return false;
    }

    ++m_observedFrames;
    m_lastObservedStamp = self.stamp;
    const bool firstDecision = m_lastObservedResult == ActionResultType::None;
    const bool transition = result.type != m_lastObservedResult;
    const bool terminal = result.isTerminal();
    m_lastObservedResult = result.type;

    // Validate every observed action and feedback, including unsampled frames.
    if (result.action != ActionType::MoveToPosition ||
        (result.type != ActionResultType::Accepted && !terminal) ||
        feedback == nullptr || feedback->sessionId != self.stamp.sessionId ||
        feedback->observedFrameId != self.stamp.frameId ||
        feedback->result.action != result.action ||
        feedback->result.type != result.type ||
        !std::isfinite(feedback->result.elapsedTime) ||
        !std::isfinite(result.elapsedTime) ||
        std::abs(feedback->result.elapsedTime - result.elapsedTime) > 0.0001f) {
      // Keep missing feedback in the first decision record to diagnose replay.
      if (!firstDecision || feedback != nullptr || result.action != ActionType::MoveToPosition) {
        m_invalid = true;
        return false;
      }
    }

    const auto &latestStored = m_records[m_count - 1].observed.stamp;
    const bool intervalDue = self.stamp.gameTime - latestStored.gameTime >= m_sampleInterval;
    const bool store = firstDecision || transition || terminal || intervalDue;
    if (!store) {
      return true;
    }

    // Reserve one record for the terminal result even on high-FPS servers.
    if (m_count >= kMockTraceCapacity - (terminal ? 0u : 1u)) {
      if (!terminal) {
        ++m_capacitySkipped;
        return true;
      }
      if (m_count == kMockTraceCapacity) {
        m_overflow = true;
        return false;
      }
    }

    TraceRecord next {};
    next.observed = self;
    next.result = result;
    if (feedback != nullptr) {
      next.feedback = *feedback;
      next.hasFeedback = true;
    }
    m_records[m_count++] = next;
    if (terminal) {
      m_finished = true;
    }
    return true;
  }

  void cancel() {
    if (m_count != 0 && !m_finished) {
      m_cancelled = true;
      m_finished = true;
    }
  }

  size_t size() const {
    return m_count;
  }

  const TraceRecord *record(size_t index) const {
    return index < m_count ? &m_records[index] : nullptr;
  }

  Vec3 target() const {
    return m_target;
  }

  runtime_io::QueryStatus preflight() const {
    return m_preflight;
  }

  ReplayReport replay() const {
    ReplayReport report {};
    report.frames = m_count;
    report.observedFrames = m_observedFrames;
    report.skippedFrames = m_observedFrames >= m_count ? static_cast<size_t>(m_observedFrames - m_count) : 0;
    report.capacitySkipped = m_capacitySkipped;
    report.sampleIntervalSeconds = m_sampleInterval;
    if (m_count == 0) {
      return report;
    }
    const auto &initial = m_records[0].observed;
    const auto &final = m_records[m_count - 1].observed;
    report.elapsedSeconds = final.stamp.gameTime - initial.stamp.gameTime;
    report.displacement = distance(initial.self.origin, final.self.origin);
    report.remainingDistance = distance(final.self.origin, m_target);
    if (m_overflow) {
      report.status = ReplayStatus::Overflow;
      return report;
    }
    if (m_invalid) {
      report.status = ReplayStatus::InvalidFrame;
      return report;
    }
    if (m_cancelled) {
      report.status = ReplayStatus::Cancelled;
      return report;
    }
    if (!m_finished || m_count < 2) {
      report.status = ReplayStatus::Incomplete;
      return report;
    }

    MockMovePolicy policy {};
    if (m_preflight != runtime_io::QueryStatus::Completed || !policy.arm(m_target)) {
      report.status = ReplayStatus::InvalidAction;
      return report;
    }
    const auto &startStamp = initial.stamp;
    uint64_t decisionFrame {};
    for (size_t i = 1; i < m_count; ++i) {
      const auto &previous = m_records[i - 1].observed.stamp;
      const auto &current = m_records[i];
      const auto &stamp = current.observed.stamp;
      if (stamp.sessionId != startStamp.sessionId ||
          stamp.botEntityIndex != startStamp.botEntityIndex ||
          stamp.roundId != startStamp.roundId ||
          runtime_io::validateFrame(stamp, &previous) != runtime_io::FrameValidation::Valid ||
          !runtime_io::validSelfState(current.observed.self)) {
        report.status = ReplayStatus::InvalidFrame;
        return report;
      }
      if (i == 1) {
        Observation observation {};
        observation.bot.alive = current.observed.self.alive;
        const auto decision = policy.decide(observation);
        if (decision.type != ActionType::MoveToPosition ||
            decision.targetType != TargetType::Position ||
            decision.targetPosition.x != m_target.x ||
            decision.targetPosition.y != m_target.y ||
            decision.targetPosition.z != m_target.z) {
          report.status = ReplayStatus::InvalidAction;
          return report;
        }
        decisionFrame = stamp.frameId;
      }
      if (current.result.action != ActionType::MoveToPosition ||
          (current.result.type != ActionResultType::Accepted && !current.result.isTerminal()) ||
          (current.result.isTerminal() != (i == m_count - 1))) {
        report.status = ReplayStatus::InvalidAction;
        return report;
      }
      if (!current.hasFeedback) {
        report.status = ReplayStatus::MissingFeedback;
        return report;
      }
      const auto &feedback = current.feedback;
      if (feedback.sessionId != stamp.sessionId ||
          feedback.decisionFrameId != decisionFrame ||
          feedback.observedFrameId != stamp.frameId ||
          feedback.result.action != current.result.action ||
          feedback.result.type != current.result.type ||
          !std::isfinite(feedback.result.elapsedTime) ||
          !std::isfinite(current.result.elapsedTime) ||
          std::abs(feedback.result.elapsedTime - current.result.elapsedTime) > 0.0001f) {
        report.status = ReplayStatus::InvalidFeedback;
        return report;
      }
    }

    report.decisionFrameId = decisionFrame;
    report.terminalFrameId = final.stamp.frameId;
    report.terminalResult = m_records[m_count - 1].result.type;
    if (report.terminalResult == ActionResultType::Completed &&
        !mockReachedTarget(final.self.origin, m_target)) {
      report.status = ReplayStatus::InvalidCompletion;
      return report;
    }
    report.status = ReplayStatus::Pass;
    return report;
  }
};

} // namespace ai::mock
