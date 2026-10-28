#pragma once

#include <algorithm>
#include <cmath>

#include <aipb/explorer/engine_bridge_trace_report.h>

namespace aipb::explorer::bridge {

enum class MovementClassification {
  Progress,
  BlockedCandidate,
  DeflectedCandidate,
  InsufficientEvidence
};

enum class MovementWindowOutcome {
  None,
  Progress,
  LowTravel,
  Deflected,
  DirectionMismatch
};

// A complete eligible observation window, with no inferred collision contact.
struct TraceWindowEvidence {
  int firstStep {};
  int lastStep {};
  int durationMs {};
  int stateStartStep { -1 };
  MovementClassification classification { MovementClassification::InsufficientEvidence };
  AipbExplorerBridgeState start {};
  AipbExplorerBridgeState end {};
  double headingDegrees {};
  double requestedUnits {};
  double observedUnits {};
  double projectedUnits {};
  double lateralUnits {};
};

using TraceWindowObserver = void (*)(void *, const TraceWindowEvidence &);

struct TraceMovementClassification {
  MovementClassification classification { MovementClassification::InsufficientEvidence };
  int eligibleFrames {};
  int evaluatedWindows {};
  int progressWindows {};
  int lowTravelWindows {};
  int directionMismatchWindows {};
  int deflectedWindows {};
  int longestLowTravelStreak {};
  int longestDeflectionStreak {};
  int firstBlockedStep { -1 };
  int firstDeflectedStep { -1 };
  int classificationStartStep { -1 };
  int lastWindowStep { -1 };
  MovementWindowOutcome lastWindow { MovementWindowOutcome::None };
  double requestedUnits {};
  double observedUnits {};
  double projectedUnits {};
  double lateralUnits {};
};

// Trace v2 contains observed positions, not engine contact or collision evidence.
inline bool classifyTraceMovement(
  const TraceBuffer &trace,
  TraceMovementClassification &result,
  TraceWindowObserver observer = nullptr,
  void *observerContext = nullptr
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
  int consecutiveDeflected = 0;
  int lowStreakStart = -1;
  int deflectionStreakStart = -1;
  int previousLateralSign = 0;
  double windowRequested = 0.0;
  double windowObserved = 0.0;
  double windowProjected = 0.0;
  double windowLateral = 0.0;
  double headingDegrees = 0.0;

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
      windowLateral = 0.0;
      consecutiveLow = 0;
      consecutiveDeflected = 0;
      lowStreakStart = -1;
      deflectionStreakStart = -1;
      previousLateralSign = 0;
      continue;
    }

    if (windowStart < 0) {
      windowStart = i;
      const double yawRadians = (sample.before.yaw + sample.command.yaw_delta)
        * kRadiansPerDegree;
      headingDegrees = std::atan2(
        sample.command.forward * std::sin(yawRadians)
          - sample.command.strafe * std::cos(yawRadians),
        sample.command.forward * std::cos(yawRadians)
          + sample.command.strafe * std::sin(yawRadians)) / kRadiansPerDegree;
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
    windowLateral += directionX * dy - directionY * dx;

    if (windowMs >= kWindowMs) {
      ++next.evaluatedWindows;
      next.requestedUnits += windowRequested;
      next.observedUnits += windowObserved;
      next.projectedUnits += windowProjected;
      next.lateralUnits += std::abs(windowLateral);
      next.lastWindowStep = windowStart;
      next.classification = MovementClassification::InsufficientEvidence;
      next.classificationStartStep = -1;
      next.lastWindow = MovementWindowOutcome::None;

      const bool enoughInput = windowRequested >= kMinimumRequest;
      const double minimumTravel = kLowTravelRatio * windowRequested;
      const bool lowTravel = enoughInput && windowObserved < minimumTravel;
      const bool directedProgress = enoughInput && windowProjected >= minimumTravel;
      const bool lateralDeflection = enoughInput && !lowTravel && !directedProgress
        && std::abs(windowLateral) >= minimumTravel
        && std::abs(windowLateral) > std::abs(windowProjected);

      if (lowTravel) {
        next.lastWindow = MovementWindowOutcome::LowTravel;
        ++next.lowTravelWindows;

        if (consecutiveLow == 0) {
          lowStreakStart = windowStart;
        }

        ++consecutiveLow;
        next.longestLowTravelStreak = std::max(next.longestLowTravelStreak,
          consecutiveLow);

        if (consecutiveLow >= 2) {
          if (next.firstBlockedStep < 0) {
            next.firstBlockedStep = lowStreakStart;
          }

          next.classification = MovementClassification::BlockedCandidate;
          next.classificationStartStep = lowStreakStart;
        }
      }
      else {
        consecutiveLow = 0;
        lowStreakStart = -1;
      }

      if (lateralDeflection) {
        next.lastWindow = MovementWindowOutcome::Deflected;
        ++next.deflectedWindows;

        if (next.firstDeflectedStep < 0) {
          next.firstDeflectedStep = windowStart;
        }

        const int sign = windowLateral > 0.0 ? 1 : -1;

        if (consecutiveDeflected == 0 || sign != previousLateralSign) {
          consecutiveDeflected = 0;
          deflectionStreakStart = windowStart;
        }

        ++consecutiveDeflected;
        previousLateralSign = sign;
        next.longestDeflectionStreak = std::max(next.longestDeflectionStreak,
          consecutiveDeflected);

        if (consecutiveDeflected >= 2) {
          next.classification = MovementClassification::DeflectedCandidate;
          next.classificationStartStep = deflectionStreakStart;
        }
      }
      else {
        consecutiveDeflected = 0;
        deflectionStreakStart = -1;
        previousLateralSign = 0;
      }

      if (directedProgress) {
        next.lastWindow = MovementWindowOutcome::Progress;
        ++next.progressWindows;
        next.classification = MovementClassification::Progress;
        next.classificationStartStep = windowStart;
      }
      else if (enoughInput && !lowTravel && !lateralDeflection) {
        next.lastWindow = MovementWindowOutcome::DirectionMismatch;
        ++next.directionMismatchWindows;
      }

      if (observer && next.classification != MovementClassification::InsufficientEvidence) {
        TraceWindowEvidence evidence {};
        evidence.firstStep = windowStart;
        evidence.lastStep = i;
        evidence.durationMs = windowMs;
        evidence.stateStartStep = next.classificationStartStep;
        evidence.classification = next.classification;
        evidence.start = trace.samples[windowStart].before;
        evidence.end = sample.after;
        evidence.headingDegrees = headingDegrees;
        evidence.requestedUnits = windowRequested;
        evidence.observedUnits = windowObserved;
        evidence.projectedUnits = windowProjected;
        evidence.lateralUnits = windowLateral;
        observer(observerContext, evidence);
      }

      windowStart = -1;
      windowMs = 0;
      windowRequested = 0.0;
      windowObserved = 0.0;
      windowProjected = 0.0;
      windowLateral = 0.0;
    }
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
  case MovementClassification::DeflectedCandidate:
    return "DeflectedCandidate";
  case MovementClassification::InsufficientEvidence:
    return "InsufficientEvidence";
  }

  return "InsufficientEvidence";
}

inline const char *movementWindowOutcomeName(MovementWindowOutcome outcome) {
  switch (outcome) {
  case MovementWindowOutcome::None:
    return "None";
  case MovementWindowOutcome::Progress:
    return "Progress";
  case MovementWindowOutcome::LowTravel:
    return "LowTravel";
  case MovementWindowOutcome::Deflected:
    return "Deflected";
  case MovementWindowOutcome::DirectionMismatch:
    return "DirectionMismatch";
  }

  return "None";
}

} // namespace aipb::explorer::bridge
