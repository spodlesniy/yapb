#pragma once

#include <algorithm>
#include <cmath>

#include <aipb/explorer/engine_bridge_trace_report.h>

namespace aipb::explorer::bridge {

enum class MovementClassification {
  Progress,
  BlockedCandidate,
  InsufficientEvidence
};

struct TraceMovementClassification {
  MovementClassification classification { MovementClassification::InsufficientEvidence };
  int eligibleFrames {};
  int evaluatedWindows {};
  int progressWindows {};
  int lowTravelWindows {};
  int directionMismatchWindows {};
  int longestLowTravelStreak {};
  int firstBlockedStep { -1 };
  double requestedUnits {};
  double observedUnits {};
  double projectedUnits {};
};

// Trace v2 contains observed positions, not engine contact or collision evidence.
inline bool classifyTraceMovement(
  const TraceBuffer &trace,
  TraceMovementClassification &result
) {
  TraceReport validated {};

  if (!summarizeTrace(trace, validated)) {
    return false;
  }

  constexpr int kWindowMs = 100;
  constexpr double kMinimumRequest = 12.0;
  constexpr double kLowTravelRatio = 0.15;
  constexpr double kRadiansPerDegree = 3.14159265358979323846 / 180.0;
  TraceMovementClassification next {};
  int windowStart = -1;
  int windowMs = 0;
  int consecutiveLow = 0;
  int lowStreakStart = -1;
  double windowRequested = 0.0;
  double windowObserved = 0.0;
  double windowProjected = 0.0;

  for (int i = 0; i < trace.count; ++i) {
    const TraceSample &sample = trace.samples[i];
    const float inputSpeed = requestedHorizontalSpeed(sample);
    const bool eligible = sample.before.on_ground && sample.after.on_ground
      && !sample.command.jump && !sample.command.duck && inputSpeed >= 100.0f;

    if (!eligible) {
      windowStart = -1;
      windowMs = 0;
      windowRequested = 0.0;
      windowObserved = 0.0;
      windowProjected = 0.0;
      consecutiveLow = 0;
      lowStreakStart = -1;
      continue;
    }

    if (windowStart < 0) {
      windowStart = i;
    }

    const double yawRadians = (sample.before.yaw + sample.command.yaw_delta)
      * kRadiansPerDegree;
    const double forward = sample.command.forward;
    const double strafe = sample.command.strafe;
    const double axesMagnitude = std::hypot(forward, strafe);
    const double directionX = (forward * std::cos(yawRadians)
      + strafe * std::sin(yawRadians)) / axesMagnitude;
    const double directionY = (forward * std::sin(yawRadians)
      - strafe * std::cos(yawRadians)) / axesMagnitude;
    const double dx = sample.after.x - sample.before.x;
    const double dy = sample.after.y - sample.before.y;
    ++next.eligibleFrames;
    windowMs += sample.msec;
    windowRequested += static_cast<double>(inputSpeed) * sample.msec / 1000.0;
    windowObserved += std::hypot(dx, dy);
    windowProjected += dx * directionX + dy * directionY;

    if (windowMs >= kWindowMs) {
      ++next.evaluatedWindows;
      next.requestedUnits += windowRequested;
      next.observedUnits += windowObserved;
      next.projectedUnits += windowProjected;

      if (windowRequested >= kMinimumRequest
        && windowObserved < kLowTravelRatio * windowRequested) {
        ++next.lowTravelWindows;

        if (consecutiveLow == 0) {
          lowStreakStart = windowStart;
        }

        ++consecutiveLow;
        next.longestLowTravelStreak = std::max(next.longestLowTravelStreak,
          consecutiveLow);

        if (consecutiveLow >= 2 && next.firstBlockedStep < 0) {
          next.firstBlockedStep = lowStreakStart;
        }
      }
      else {
        consecutiveLow = 0;
        lowStreakStart = -1;

        if (windowRequested >= kMinimumRequest
          && windowProjected >= kLowTravelRatio * windowRequested) {
          ++next.progressWindows;
        }
        else if (windowRequested >= kMinimumRequest) {
          ++next.directionMismatchWindows;
        }
      }

      windowStart = -1;
      windowMs = 0;
      windowRequested = 0.0;
      windowObserved = 0.0;
      windowProjected = 0.0;
    }
  }

  if (next.firstBlockedStep >= 0) {
    next.classification = MovementClassification::BlockedCandidate;
  }
  else if (next.progressWindows > 0) {
    next.classification = MovementClassification::Progress;
  }

  result = next;
  return true;
}

inline const char *movementClassificationName(MovementClassification classification) {
  switch (classification) {
  case MovementClassification::Progress:
    return "Progress";
  case MovementClassification::BlockedCandidate:
    return "BlockedCandidate";
  case MovementClassification::InsufficientEvidence:
    return "InsufficientEvidence";
  }

  return "InsufficientEvidence";
}

} // namespace aipb::explorer::bridge
