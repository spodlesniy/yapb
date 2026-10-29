#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include <aipb/explorer/engine_bridge_experiment_contract.h>
#include <aipb/explorer/engine_bridge_trace_report.h>

namespace aipb::explorer::bridge {

enum class ExperimentOutcome {
  Success,
  Failure,
  Inconclusive,
  Diverged
};

enum class ExperimentReason {
  TargetReached,
  BelowTarget,
  Incomplete,
  InvalidTrace,
  MapMismatch,
  StartMismatch,
  CommandMismatch,
  SpeedCapMismatch,
  TimeLimit,
  LeftGround
};

inline const char *experimentOutcomeName(ExperimentOutcome value) {
  switch (value) {
  case ExperimentOutcome::Success: return "Success";
  case ExperimentOutcome::Failure: return "Failure";
  case ExperimentOutcome::Inconclusive: return "Inconclusive";
  case ExperimentOutcome::Diverged: return "Diverged";
  }

  return "Inconclusive";
}

inline const char *experimentReasonName(ExperimentReason value) {
  switch (value) {
  case ExperimentReason::TargetReached: return "TargetReached";
  case ExperimentReason::BelowTarget: return "BelowTarget";
  case ExperimentReason::Incomplete: return "Incomplete";
  case ExperimentReason::InvalidTrace: return "InvalidTrace";
  case ExperimentReason::MapMismatch: return "MapMismatch";
  case ExperimentReason::StartMismatch: return "StartMismatch";
  case ExperimentReason::CommandMismatch: return "CommandMismatch";
  case ExperimentReason::SpeedCapMismatch: return "SpeedCapMismatch";
  case ExperimentReason::TimeLimit: return "TimeLimit";
  case ExperimentReason::LeftGround: return "LeftGround";
  }

  return "InvalidTrace";
}

struct GroundExperimentResult {
  std::string id {};
  std::string sourceId {};
  ExperimentOutcome outcome { ExperimentOutcome::Inconclusive };
  ExperimentReason reason { ExperimentReason::Incomplete };
  int frames {};
  int elapsedMs {};
  double projectedTravel {};
  AipbExplorerBridgeState observedStart {};
  AipbExplorerBridgeState observedEnd {};
};

inline bool matchesExperimentCommand(
  const AipbExplorerBridgeCommand &expected,
  const AipbExplorerBridgeCommand &actual
) {
  return std::abs(expected.forward - actual.forward) <= 0.0001f
    && std::abs(expected.strafe - actual.strafe) <= 0.0001f
    && std::abs(expected.yaw_delta - actual.yaw_delta) <= 0.0001f
    && expected.jump == actual.jump && expected.duck == actual.duck;
}

// Assess one observed GoldSrc trace against a predeclared, verified-start
// ground experiment. Do not infer collision or any safe replay authorization.
inline bool evaluateGroundExperiment(
  const GroundExperiment &experiment,
  const TraceMetadata &metadata,
  const TraceBuffer &trace,
  GroundExperimentResult &result
) {
  if (!validGroundExperiment(experiment)) {
    return false;
  }

  GroundExperimentResult next {};
  next.id = experiment.id;
  next.sourceId = experiment.sourceId;
  next.outcome = ExperimentOutcome::Inconclusive;
  next.reason = ExperimentReason::Incomplete;

  if (!validMetadata(metadata) || experiment.map != metadata.map) {
    next.outcome = ExperimentOutcome::Diverged;
    next.reason = ExperimentReason::MapMismatch;
    result = std::move(next);
    return true;
  }

  if (!trace.samples || trace.count <= 0
    || trace.count > ProbeBudget::kMaxSafetyFrames) {
    next.reason = ExperimentReason::InvalidTrace;
    result = std::move(next);
    return true;
  }

  next.observedStart = trace.samples[0].before;
  next.observedEnd = trace.samples[trace.count - 1].after;
  const ReplayDifference startDifference =
    compareStates(experiment.start, next.observedStart);
  const ReplayDifference metadataDifference =
    compareStates(metadata.initial, next.observedStart);

  if (!startDifference.within(0.05, 0.25, 0.25f)
    || !metadataDifference.within(0.05, 0.25, 0.25f)) {
    next.outcome = ExperimentOutcome::Diverged;
    next.reason = ExperimentReason::StartMismatch;
    result = std::move(next);
    return true;
  }

  int stepIndex = 0;
  int remaining = experiment.commands[0].frames;

  for (int index = 0; index < trace.count; ++index) {
    const TraceSample &sample = trace.samples[index];

    if (!valid(sample.command) || !validState(sample.before)
      || !validState(sample.after) || sample.msec == 0
      || sample.after.tick != sample.before.tick + 1
      || (index != 0 && !continuousTraceState(
        trace.samples[index - 1].after, sample.before))) {
      next.outcome = ExperimentOutcome::Diverged;
      next.reason = ExperimentReason::InvalidTrace;
      break;
    }

    if (index >= experiment.maxFrames
      || stepIndex >= static_cast<int>(experiment.commands.size())
      || !matchesExperimentCommand(experiment.commands[stepIndex].input, sample.command)) {
      next.outcome = ExperimentOutcome::Diverged;
      next.reason = ExperimentReason::CommandMismatch;
      break;
    }

    if (!std::isfinite(sample.maxSpeed)
      || std::abs(sample.maxSpeed - experiment.weaponSpeedCap) > 0.5f) {
      next.outcome = ExperimentOutcome::Diverged;
      next.reason = ExperimentReason::SpeedCapMismatch;
      break;
    }

    next.elapsedMs += sample.msec;
    ++next.frames;
    next.observedEnd = sample.after;

    if (next.elapsedMs > experiment.maxDurationMs) {
      next.outcome = ExperimentOutcome::Diverged;
      next.reason = ExperimentReason::TimeLimit;
      break;
    }

    if (!sample.before.on_ground || !sample.after.on_ground
      || sample.before.crouching || sample.after.crouching) {
      next.reason = ExperimentReason::LeftGround;
      break;
    }

    --remaining;

    if (remaining == 0) {
      ++stepIndex;

      if (stepIndex < static_cast<int>(experiment.commands.size())) {
        remaining = experiment.commands[stepIndex].frames;
      }
    }
  }

  constexpr double kRadiansPerDegree = 3.14159265358979323846 / 180.0;
  const double radians = experiment.targetHeading * kRadiansPerDegree;
  const double dx = next.observedEnd.x - next.observedStart.x;
  const double dy = next.observedEnd.y - next.observedStart.y;
  next.projectedTravel = dx * std::cos(radians) + dy * std::sin(radians);

  if (next.reason == ExperimentReason::Incomplete
    && next.frames == experiment.maxFrames && trace.completed) {
    next.outcome = next.projectedTravel >= experiment.minimumProjectedTravel
      ? ExperimentOutcome::Success : ExperimentOutcome::Failure;
    next.reason = next.outcome == ExperimentOutcome::Success
      ? ExperimentReason::TargetReached : ExperimentReason::BelowTarget;
  }

  result = std::move(next);
  return true;
}

inline bool formatGroundExperimentResult(
  const GroundExperimentResult &result,
  std::string &output
) {
  if (!safeName(result.id.c_str()) || !safeName(result.sourceId.c_str())
    || result.frames < 0 || result.elapsedMs < 0
    || !std::isfinite(result.projectedTravel)
    || !validState(result.observedStart) || !validState(result.observedEnd)) {
    return false;
  }

  char line[1024] {};
  const auto &a = result.observedStart;
  const auto &b = result.observedEnd;
  const int n = std::snprintf(line, sizeof(line),
    "%s,%s,%s,%s,%s,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",
    kExperimentResultVersion, result.id.c_str(), result.sourceId.c_str(),
    experimentOutcomeName(result.outcome), experimentReasonName(result.reason),
    result.frames, result.elapsedMs, result.projectedTravel,
    a.x, a.y, a.z, b.x, b.y, b.z);

  if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(line)) {
    return false;
  }

  output = line;
  return true;
}

} // namespace aipb::explorer::bridge
