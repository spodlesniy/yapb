//
// AiPB - engine-independent, bounded world geometry query admission.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstdint>

#include <ai/ai_server_state.h>

namespace ai::runtime_io {

constexpr uint8_t kWorldQueriesPerBotFrame = 4;
constexpr float kWorldQueryStartRadius = 64.0f;
constexpr float kWorldQueryMaxDistance = 384.0f;
constexpr int32_t kWorldQueryStandingHull = 0;
constexpr int32_t kWorldQueryCrouchingHull = 1;

inline double queryDistanceSquared(const Vec3 &a, const Vec3 &b) {
  const double x = static_cast<double>(a.x) - b.x;
  const double y = static_cast<double>(a.y) - b.y;
  const double z = static_cast<double>(a.z) - b.z;
  return x * x + y * y + z * z;
}

struct GeometryTraceSample {
  float fraction {};
  Vec3 endPosition {};
  Vec3 planeNormal {};
  bool startSolid {};
  bool hitPlayer {};
};

// The identity fields are echoed even on failure; no engine data is returned then.
inline WorldQueryResult rejectedWorldQuery(const WorldQueryRequest &request, QueryStatus status) {
  WorldQueryResult result {};
  result.schemaVersion = kSchemaVersion;
  result.requestId = request.requestId;
  result.sessionId = request.sessionId;
  result.observationFrameId = request.observationFrameId;
  result.status = status;
  return result;
}

// No player identity or contact location is ever returned by this geometry endpoint.
inline WorldQueryResult worldTraceResult(const WorldQueryRequest &request, const GeometryTraceSample &trace) {
  auto result = rejectedWorldQuery(request, QueryStatus::Error);
  if (trace.hitPlayer || !std::isfinite(trace.fraction) || trace.fraction < 0.0f || trace.fraction > 1.0f ||
      !finiteVector(trace.endPosition) || !finiteVector(trace.planeNormal)) {
    return result;
  }
  result.status = QueryStatus::Completed;
  result.fraction = trace.fraction;
  result.hitPosition = trace.endPosition;
  result.planeNormal = trace.planeNormal;
  result.startSolid = trace.startSolid;
  return result;
}

class WorldQueryGate final {
private:
  struct Slot {
    uint64_t sessionId {};
    uint64_t frameId {};
    uint64_t lastRequestId {};
    uint8_t used {};
  };

  Slot m_slots[kServerBotSlots] {};

public:
  // A successful admission consumes a trace budget even if the engine subsequently errors.
  QueryStatus admit(int slot, const ServerBotSnapshot &snapshot, const WorldQueryRequest &request) {
    if (slot < 0 || slot >= kServerBotSlots) {
      return QueryStatus::Rejected;
    }
    const auto &stamp = snapshot.stamp;
    if (stamp.botEntityIndex != slot + 1 || validateFrame(stamp) != FrameValidation::Valid ||
        !validSelfState(snapshot.self)) {
      return QueryStatus::Stale;
    }
    if (request.schemaVersion != kSchemaVersion) {
      return QueryStatus::Unsupported;
    }
    if (request.sessionId != stamp.sessionId || request.observationFrameId != stamp.frameId) {
      return QueryStatus::Stale;
    }
    if (request.requestId == 0) {
      return QueryStatus::Rejected;
    }
    if (request.kind != QueryKind::TraceLine && request.kind != QueryKind::TraceHull) {
      return QueryStatus::Unsupported;
    }
    if (request.kind == QueryKind::TraceLine && request.hull != 0) {
      return QueryStatus::Rejected;
    }
    if (request.kind == QueryKind::TraceHull &&
        request.hull != kWorldQueryStandingHull && request.hull != kWorldQueryCrouchingHull) {
      return QueryStatus::Rejected;
    }
    if (!finiteVector(request.start) || !finiteVector(request.end) ||
        queryDistanceSquared(snapshot.self.origin, request.start) >
            static_cast<double>(kWorldQueryStartRadius) * kWorldQueryStartRadius ||
        queryDistanceSquared(request.start, request.end) >
            static_cast<double>(kWorldQueryMaxDistance) * kWorldQueryMaxDistance) {
      return QueryStatus::Rejected;
    }

    auto &current = m_slots[slot];
    if (current.sessionId > stamp.sessionId ||
        (current.sessionId == stamp.sessionId && current.frameId > stamp.frameId)) {
      return QueryStatus::Stale;
    }
    if (current.sessionId != stamp.sessionId) {
      current = {};
      current.sessionId = stamp.sessionId;
    }
    if (current.frameId != stamp.frameId) {
      current.frameId = stamp.frameId;
      current.used = 0;
    }
    if (request.requestId <= current.lastRequestId) {
      return QueryStatus::Rejected;
    }
    if (current.used >= kWorldQueriesPerBotFrame) {
      return QueryStatus::BudgetExceeded;
    }

    ++current.used;
    current.lastRequestId = request.requestId;
    return QueryStatus::Completed;
  }

  void release(int slot) {
    if (slot >= 0 && slot < kServerBotSlots) {
      m_slots[slot] = {};
    }
  }

  void reset() {
    for (auto &slot : m_slots) {
      slot = {};
    }
  }
};

} // namespace ai::runtime_io
