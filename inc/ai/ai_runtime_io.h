//
// AiPB - engine-neutral runtime I/O boundary contracts.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_result.h>
#include <ai/ai_observation.h>

#include <cmath>
#include <cstdint>

namespace ai::runtime_io {

constexpr uint32_t kSchemaVersion = 1;

enum class SessionState : uint8_t {
  Created,
  Ready,
  Active,
  Interrupted,
  Destroyed,
};

constexpr bool canTransition(SessionState from, SessionState to) {
  switch (from) {
  case SessionState::Created:
    return to == SessionState::Ready || to == SessionState::Destroyed;
  case SessionState::Ready:
    return to == SessionState::Active || to == SessionState::Destroyed;
  case SessionState::Active:
    return to == SessionState::Ready || to == SessionState::Interrupted || to == SessionState::Destroyed;
  case SessionState::Interrupted:
    return to == SessionState::Ready || to == SessionState::Destroyed;
  case SessionState::Destroyed:
    return false;
  }
  return false;
}

// Session IDs must change when a map or bot identity is replaced.
// Frame IDs are strictly increasing within a session.
struct FrameStamp {
  uint32_t schemaVersion { kSchemaVersion };
  uint64_t sessionId {};
  uint64_t frameId {};
  uint64_t roundId {};
  float gameTime {};
  float frameSeconds {};
  int32_t botEntityIndex { -1 };
};

enum class FrameValidation : uint8_t {
  Valid,
  UnsupportedSchema,
  InvalidIdentity,
  InvalidClock,
  Stale,
};

inline FrameValidation validateFrame(const FrameStamp &current, const FrameStamp *previous = nullptr) {
  if (current.schemaVersion != kSchemaVersion) {
    return FrameValidation::UnsupportedSchema;
  }
  if (current.sessionId == 0 || current.frameId == 0 || current.botEntityIndex <= 0) {
    return FrameValidation::InvalidIdentity;
  }
  if (!std::isfinite(current.gameTime) || current.gameTime < 0.0f ||
      !std::isfinite(current.frameSeconds) || current.frameSeconds < 0.0f) {
    return FrameValidation::InvalidClock;
  }
  if (previous != nullptr) {
    if (current.sessionId < previous->sessionId ||
        (current.sessionId == previous->sessionId &&
         (current.frameId <= previous->frameId || current.botEntityIndex != previous->botEntityIndex ||
          current.gameTime < previous->gameTime || current.roundId < previous->roundId))) {
      return FrameValidation::Stale;
    }
  }
  return FrameValidation::Valid;
}

enum class Evidence : uint8_t {
  Unknown,
  Self,
  PublicEvent,
  Seen,
  Heard,
  Remembered,
  Inferred,
};

enum class EventKind : uint8_t {
  Unknown,
  Sound,
  Damage,
  EntityAppeared,
  EntityDisappeared,
  RoundStarted,
  RoundEnded,
  MapChanged,
};

struct WorldEvent {
  FrameStamp stamp {};
  EventKind kind { EventKind::Unknown };
  Evidence evidence { Evidence::Unknown };
  int32_t subjectEntityIndex { -1 };
  Vec3 position {};
};

enum class QueryKind : uint8_t {
  TraceLine,
  TraceHull,
  PointContents,
  FindEntities,
  PotentialVisibility,
  PotentialAudibility,
};

enum class QueryStatus : uint8_t {
  Unknown,
  Completed,
  Rejected,
  BudgetExceeded,
  Stale,
  Unsupported,
  Error,
};

struct WorldQueryRequest {
  uint32_t schemaVersion { kSchemaVersion };
  uint64_t requestId {};
  uint64_t sessionId {};
  uint64_t observationFrameId {};
  QueryKind kind { QueryKind::TraceLine };
  Vec3 start {};
  Vec3 end {};
  int32_t hull {};
};

struct WorldQueryResult {
  uint32_t schemaVersion { kSchemaVersion };
  uint64_t requestId {};
  uint64_t sessionId {};
  uint64_t observationFrameId {};
  QueryStatus status { QueryStatus::Unknown };
  float fraction {};
  Vec3 hitPosition {};
  Vec3 planeNormal {};
  int32_t hitEntityIndex { -1 };
  int32_t contents {};
  bool startSolid {};
};

// A reply from a different decision frame or session must never be reused.
inline bool matchesRequest(const WorldQueryRequest &request, const WorldQueryResult &result) {
  return request.schemaVersion == kSchemaVersion && result.schemaVersion == kSchemaVersion &&
         request.requestId != 0 && request.sessionId != 0 && request.observationFrameId != 0 &&
         request.requestId == result.requestId && request.sessionId == result.sessionId &&
         request.observationFrameId == result.observationFrameId;
}

inline bool isCompleted(const WorldQueryRequest &request, const WorldQueryResult &result) {
  return matchesRequest(request, result) && result.status == QueryStatus::Completed &&
         std::isfinite(result.fraction) && result.fraction >= 0.0f && result.fraction <= 1.0f;
}

struct ActionFeedback {
  uint64_t sessionId {};
  uint64_t decisionFrameId {};
  uint64_t observedFrameId {};
  ActionResult result {};
};

} // namespace ai::runtime_io
