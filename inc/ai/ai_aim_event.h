//
// AiPB - bounded aim and target-switch diagnostics (not model transitions).
// SPDX-License-Identifier: MIT
//
#pragma once

#include <cmath>
#include <cstdint>
#include <ai/ai_observation.h>

namespace ai {

enum class AimEventType : uint8_t { TargetAcquired, TargetSwitched, TargetLost, RapidTurn };
enum class AimEventReason : uint8_t {
  TargetAcquired, TargetChanged, TargetLost, Blind, FlashAvoidance,
  Override, Grenade, Enemy, Entity, LastEnemy, PredictPath,
  Camp, Navigation, Unattributed
};

constexpr const char *aimEventName(AimEventType type) {
  switch (type) {
  case AimEventType::TargetAcquired: return "target_acquired";
  case AimEventType::TargetSwitched: return "target_switched";
  case AimEventType::TargetLost: return "target_lost";
  case AimEventType::RapidTurn: return "rapid_aim_turn";
  }
  return "unknown";
}

constexpr const char *aimEventReasonName(AimEventReason reason) {
  switch (reason) {
  case AimEventReason::TargetAcquired: return "target_acquired";
  case AimEventReason::TargetChanged: return "target_changed";
  case AimEventReason::TargetLost: return "target_lost";
  case AimEventReason::Blind: return "flash_blind";
  case AimEventReason::FlashAvoidance: return "flash_avoidance";
  case AimEventReason::Override: return "aim_override";
  case AimEventReason::Grenade: return "aim_grenade";
  case AimEventReason::Enemy: return "aim_enemy";
  case AimEventReason::Entity: return "aim_entity";
  case AimEventReason::LastEnemy: return "aim_last_enemy";
  case AimEventReason::PredictPath: return "aim_predict_path";
  case AimEventReason::Camp: return "aim_camp";
  case AimEventReason::Navigation: return "aim_navigation";
  case AimEventReason::Unattributed: return "unattributed";
  }
  return "unknown";
}

// Mirrors the prioritization in Bot::setAimDirection(). These are observed
// aim flags, not proof that an enemy caused a specific head turn.
constexpr AimEventReason aimReasonForState(bool blinded, bool flashAvoidance, bool overrideAim, bool grenade,
    bool enemy, bool entity, bool lastEnemy, bool predictPath, bool camp, bool navigation) {
  return blinded ? AimEventReason::Blind
       : flashAvoidance ? AimEventReason::FlashAvoidance
       : overrideAim ? AimEventReason::Override
       : grenade ? AimEventReason::Grenade
       : enemy ? AimEventReason::Enemy
       : entity ? AimEventReason::Entity
       : lastEnemy ? AimEventReason::LastEnemy
       : predictPath ? AimEventReason::PredictPath
       : camp ? AimEventReason::Camp
       : navigation ? AimEventReason::Navigation
       : AimEventReason::Unattributed;
}

// Normalize differences even when source angles accumulated multiple turns.
inline float aimAngleDifference(float current, float previous) {
  float delta = std::fmod(current - previous, 360.0f);
  if (delta > 180.0f) delta -= 360.0f;
  if (delta < -180.0f) delta += 360.0f;
  return delta;
}

constexpr float kAimRapidTurnThresholdDegrees = 60.0f;
constexpr float kAimRapidTurnMaxInterval = 0.30f;
constexpr float kAimRapidTurnReportInterval = 0.70f;
constexpr float kAimTargetChangeReportInterval = 0.20f;

struct AimDiagnosticDecision {
  bool targetChanged {}, rapidTurn {};
  int32_t previousTargetId { -1 }, targetId { -1 };
  float elapsed {}, yawDelta {}, pitchDelta {};
};

class AimDiagnosticTracker final {
  bool m_initialized {};
  int32_t m_lastTargetId { -1 };
  float m_lastTime {}, m_lastYaw {}, m_lastPitch {};
  float m_nextTargetReport {}, m_nextTurnReport {};
public:
  void reset() { *this = AimDiagnosticTracker {}; }

  AimDiagnosticDecision observe(float now, float yaw, float pitch, int32_t targetId) {
    AimDiagnosticDecision result {};
    result.previousTargetId = m_lastTargetId;
    result.targetId = targetId;
    if (m_initialized) {
      result.elapsed = now - m_lastTime;
      result.yawDelta = aimAngleDifference(yaw, m_lastYaw);
      result.pitchDelta = aimAngleDifference(pitch, m_lastPitch);
      const float absYaw = result.yawDelta < 0.0f ? -result.yawDelta : result.yawDelta;
      const float absPitch = result.pitchDelta < 0.0f ? -result.pitchDelta : result.pitchDelta;
      result.targetChanged = targetId != m_lastTargetId && now >= m_nextTargetReport;
      result.rapidTurn = result.elapsed > 0.0f
          && result.elapsed <= kAimRapidTurnMaxInterval
          && (absYaw >= kAimRapidTurnThresholdDegrees || absPitch >= kAimRapidTurnThresholdDegrees)
          && now >= m_nextTurnReport;
      if (result.targetChanged) m_nextTargetReport = now + kAimTargetChangeReportInterval;
      if (result.rapidTurn) m_nextTurnReport = now + kAimRapidTurnReportInterval;
    }
    m_initialized = true;
    m_lastTime = now;
    m_lastYaw = yaw;
    m_lastPitch = pitch;
    m_lastTargetId = targetId;
    return result;
  }
};

struct AimEvent {
  AimEventType type { AimEventType::RapidTurn };
  AimEventReason reason { AimEventReason::Unattributed };
  float gameTime {}, roundStartTime {}, elapsed {};
  uint32_t roundId {}, aimFlags {};
  uint64_t episodeId {};
  int32_t botId { -1 }, team { -1 }, task { -1 }, aiAction { -1 };
  int32_t previousTargetId { -1 }, targetId { -1 };
  float viewYaw {}, viewPitch {}, yawDelta {}, pitchDelta {}, blindTimeRemaining {};
  bool targetVisible {};
  Vec3 position {};
};

} // namespace ai
