// AiPB - defuse telemetry, separate from model training transitions.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <ai/ai_observation.h>

namespace ai {

// Diagnostic state is a Bot member, so its complete definition must be visible
// from yapb.h without relying on task-local navigation header include order.
constexpr float kPlantedBombApproachDiagnosticInterval = 5.0f;

class DefuseApproachDiagnosticGate final {
  float m_nextBlockedReport {}, m_nextFailureReport {};
public:
  void reset() { m_nextBlockedReport = m_nextFailureReport = 0.0f; }
  bool allow(float now, bool failure) {
    auto &next = failure ? m_nextFailureReport : m_nextBlockedReport;
    if (now < next) return false;
    next = now + kPlantedBombApproachDiagnosticInterval;
    return true;
  }
};

 
enum class DefuseEventType : uint8_t { Attempt, Start, Interrupted, Complete, ApproachBlocked, ApproachFailed };
enum class DefuseEventReason : uint8_t {
  None, BarTimeCleared, UseNotConfirmed, TaskEnded, RoundEnded, BotDied, BombExploded,
  DirectPathBlocked, NoReachableInteractionNode
};
enum class DefuseEvidence : uint8_t {
  UseButton, BarTimePositive, BarTimeZero, TaskLifecycle, UnconfirmedUseTimeout,
  RoundMessage, DeathMessage, BombDefusedMessage, AliveState, GameState,
  GeometryReachability, GraphRouteUnavailable
};

constexpr const char *defuseEventName(DefuseEventType v) {
  switch (v) {
  case DefuseEventType::Attempt: return "defuse_attempt";
  case DefuseEventType::Start: return "defuse_start";
  case DefuseEventType::Interrupted: return "defuse_interrupted";
  case DefuseEventType::Complete: return "defuse_complete";
  case DefuseEventType::ApproachBlocked: return "defuse_approach_blocked";
  case DefuseEventType::ApproachFailed: return "defuse_approach_failed";
  }
  return "unknown";
}
constexpr const char *defuseReasonName(DefuseEventReason v) {
  switch (v) {
  case DefuseEventReason::None: return "none";
  case DefuseEventReason::BarTimeCleared: return "bar_time_cleared";
  case DefuseEventReason::UseNotConfirmed: return "use_not_confirmed";
  case DefuseEventReason::TaskEnded: return "task_ended";
  case DefuseEventReason::RoundEnded: return "round_ended";
  case DefuseEventReason::BotDied: return "bot_died";
  case DefuseEventReason::BombExploded: return "bomb_exploded";
  case DefuseEventReason::DirectPathBlocked: return "direct_path_blocked";
  case DefuseEventReason::NoReachableInteractionNode: return "no_reachable_interaction_node";
  }
  return "unknown";
}
constexpr const char *defuseEvidenceName(DefuseEvidence v) {
  switch (v) {
  case DefuseEvidence::UseButton: return "in_use";
  case DefuseEvidence::BarTimePositive: return "bar_time_positive";
  case DefuseEvidence::BarTimeZero: return "bar_time_zero";
  case DefuseEvidence::TaskLifecycle: return "task_lifecycle";
  case DefuseEvidence::UnconfirmedUseTimeout: return "unconfirmed_use_timeout";
  case DefuseEvidence::RoundMessage: return "round_message";
  case DefuseEvidence::DeathMessage: return "death_message";
  case DefuseEvidence::AliveState: return "observed_dead";
  case DefuseEvidence::GameState: return "game_state";
  case DefuseEvidence::BombDefusedMessage: return "bomb_defused_text_message";
  case DefuseEvidence::GeometryReachability: return "geometry_reachability";
  case DefuseEvidence::GraphRouteUnavailable: return "graph_route_unavailable";
  }
  return "unknown";
}

struct DefuseEvent {
  DefuseEventType type { DefuseEventType::Attempt };
  DefuseEventReason reason { DefuseEventReason::None };
  DefuseEvidence evidence { DefuseEvidence::UseButton };
  float gameTime {}, roundStartTime {};
  uint32_t roundId {}, attemptId {};
  uint64_t episodeId {};
  int32_t botId { -1 }, team { -1 }, task { -1 }, aiAction { -1 };
  float bombTimeRemaining { -1.0f }, distanceToBomb { -1.0f }, attemptElapsed { -1.0f };
  Vec3 bombPosition {}, botPosition {};
  bool hasBombPosition {}, hasBotPosition {};
  bool hasDefuseKit {}, hasProgressBar {}, isDucking {};
};

// BarTime=0 may precede the authoritative #Bomb_Defused TextMsg.
// Delay interruption until the bomb is still planted after a short grace period.
class DefuseAttemptTracker final {
public:
  enum class Phase : uint8_t { Idle, Attempt, Progress, ProgressLost };
private:
  Phase m_phase { Phase::Idle };
  uint32_t m_attemptId {};
  float m_attemptStart {}, m_progressLostAt {};
  void nextAttempt(float now) {
    ++m_attemptId;
    if (m_attemptId == 0) ++m_attemptId;
    m_attemptStart = now;
  }
public:
  Phase phase() const { return m_phase; }
  bool active() const { return m_phase != Phase::Idle; }
  uint32_t attemptId() const { return m_attemptId; }
  float elapsed(float now) const {
    return active() && now >= m_attemptStart ? now - m_attemptStart : -1.0f;
  }
  bool beginUse(float now) {
    if (active()) return false;
    nextAttempt(now);
    m_phase = Phase::Attempt;
    return true;
  }
  bool confirmProgress(float now) {
    if (m_phase == Phase::Progress) return false;
    if (m_phase == Phase::ProgressLost) {
      m_phase = Phase::Progress; // transient BarTime, not a new start
      return false;
    }
    if (m_phase == Phase::Idle) nextAttempt(now); // do not invent a USE event
    m_phase = Phase::Progress;
    return true;
  }
  bool loseProgress(float now) {
    if (m_phase != Phase::Progress) return false;
    m_phase = Phase::ProgressLost;
    m_progressLostAt = now;
    return true;
  }
  bool progressLossExpired(float now) const {
    return m_phase == Phase::ProgressLost && now - m_progressLostAt >= 1.0f;
  }
  bool end() {
    if (!active()) return false;
    m_phase = Phase::Idle;
    return true;
  }
  void reset() {
    m_phase = Phase::Idle;
    m_attemptId = 0;
    m_attemptStart = 0.0f;
    m_progressLostAt = 0.0f;
  }
};

} // namespace ai
