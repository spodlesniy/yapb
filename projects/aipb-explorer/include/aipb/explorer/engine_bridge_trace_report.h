#pragma once

#include <algorithm>
#include <cmath>

#include <aipb/explorer/engine_bridge_trace.h>

namespace aipb::explorer::bridge {

// Summarize measured GoldSrc frames, never intent-derived movement distance.
struct TraceReport {
  int frames {};
  int simulatedMs {};
  int airborneFrames {};
  int crouchedFrames {};
  int jumpHeldFrames {};
  int neutralInputFrames {};
  double traveledUnits {};
  double displacementUnits {};
  double averageSpeed {};
  double peakVelocity {};
  AipbExplorerBridgeState initial {};
  AipbExplorerBridgeState final {};
};

inline bool summarizeTrace(const TraceBuffer &trace, TraceReport &result) {
  if (!trace.completed || !trace.samples || trace.count <= 0
    || trace.count > ProbeBudget::kMaxSafetyFrames) {
    return false;
  }

  TraceReport next {};
  next.frames = trace.count;
  next.initial = trace.samples[0].before;

  for (int i = 0; i < trace.count; ++i) {
    const TraceSample &sample = trace.samples[i];

    if (!valid(sample.command) || !validState(sample.before) || !validState(sample.after)
      || sample.msec == 0 || !std::isfinite(sample.maxSpeed) || sample.maxSpeed < 0.0f
      || sample.after.tick != sample.before.tick + 1
      || (i != 0 && !continuousTraceState(trace.samples[i - 1].after, sample.before))) {
      return false;
    }

    const double dx = sample.after.x - sample.before.x;
    const double dy = sample.after.y - sample.before.y;
    const double dz = sample.after.z - sample.before.z;
    next.traveledUnits += std::hypot(dx, dy, dz);
    next.peakVelocity = std::max(next.peakVelocity,
      std::hypot(sample.after.vx, sample.after.vy, sample.after.vz));
    next.simulatedMs += sample.msec;
    next.airborneFrames += sample.after.on_ground == 0 ? 1 : 0;
    next.crouchedFrames += sample.after.crouching != 0 ? 1 : 0;
    next.jumpHeldFrames += sample.command.jump != 0 ? 1 : 0;
    next.neutralInputFrames += sample.command.forward == 0.0f
      && sample.command.strafe == 0.0f
      && sample.command.yaw_delta == 0.0f
      && !sample.command.jump && !sample.command.duck ? 1 : 0;
  }

  next.final = trace.samples[trace.count - 1].after;
  next.displacementUnits = std::hypot(
    next.final.x - next.initial.x,
    next.final.y - next.initial.y,
    next.final.z - next.initial.z);
  next.averageSpeed = next.traveledUnits * 1000.0 / next.simulatedMs;

  if (!std::isfinite(next.traveledUnits) || !std::isfinite(next.displacementUnits)
    || !std::isfinite(next.peakVelocity) || !std::isfinite(next.averageSpeed)) {
    return false;
  }

  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
