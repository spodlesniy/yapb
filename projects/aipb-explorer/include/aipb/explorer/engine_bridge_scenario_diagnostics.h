#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include <aipb/explorer/engine_bridge_scenario_result.h>

namespace aipb::explorer::scenario {

inline constexpr const char *kScenarioDiagnosticsVersion = "aipb-scenario-diagnostics-v1";
inline constexpr const char *kScenarioDiagnosticsColumns =
  "kind,frame,action,command_ms,game_time,round_start_time,round_id,round_over,"
  "mp_freezetime,freeze_bots,flags,movetype,solid,deadflag,maxspeed,friction,"
  "basevx,basevy,basevz,forward_speed,strafe_speed,x,y,z,vx,vy,vz,"
  "obstacle_fraction,obstacle_startsolid,obstacle_allsolid\n";

enum class ScenarioDiagnosticKind { Start, Frame, Stall, Recovered, End };

inline const char *scenarioDiagnosticKindName(ScenarioDiagnosticKind kind) {
  switch (kind) {
  case ScenarioDiagnosticKind::Start: return "start";
  case ScenarioDiagnosticKind::Frame: return "frame";
  case ScenarioDiagnosticKind::Stall: return "stall";
  case ScenarioDiagnosticKind::Recovered: return "recovered";
  case ScenarioDiagnosticKind::End: return "end";
  }
  return "unknown";
}

struct ScenarioDiagnosticSample {
  ScenarioDiagnosticKind kind { ScenarioDiagnosticKind::Start };
  int frame { -1 };
  int action { -1 };
  int commandMs {};
  double gameTime { -1.0 };
  double roundStartTime { -1.0 };
  unsigned roundId {};
  bool roundOver {};
  double freezeTime { -1.0 };
  bool freezeBots {};
  int flags {};
  int moveType {};
  int solid {};
  int deadFlag {};
  float maxSpeed {};
  float friction {};
  float baseVelocity[3] {};
  float forwardSpeed {};
  float strafeSpeed {};
  ScenarioResultPose pose {};
  double obstacleFraction { -1.0 };
  bool obstacleStartSolid {};
  bool obstacleAllSolid {};
};

inline bool validScenarioDiagnosticSample(const ScenarioDiagnosticSample &sample) {
  if (sample.frame < -1 || sample.frame > ActionScheduler::kMaxFrames
    || sample.action < -1 || sample.action >= kMaxActions
    || sample.commandMs < 0 || sample.commandMs > kMaxScenarioMs
    || !std::isfinite(sample.gameTime) || !std::isfinite(sample.roundStartTime)
    || !std::isfinite(sample.freezeTime) || !std::isfinite(sample.maxSpeed)
    || !std::isfinite(sample.friction) || !std::isfinite(sample.forwardSpeed)
    || !std::isfinite(sample.strafeSpeed) || !std::isfinite(sample.obstacleFraction)
    || !validScenarioResultPose(sample.pose)) {
    return false;
  }

  for (float value : sample.baseVelocity) {
    if (!std::isfinite(value)) {
      return false;
    }
  }

  return true;
}

class ScenarioDiagnostics {
public:
  static constexpr std::size_t kMaxSamples = 16;

  void clear() {
    samples_.clear();
  }

  bool append(const ScenarioDiagnosticSample &sample) {
    if (!validScenarioDiagnosticSample(sample) || samples_.size() >= kMaxSamples) {
      return false;
    }
    samples_.push_back(sample);
    return true;
  }

  const std::vector<ScenarioDiagnosticSample> &samples() const {
    return samples_;
  }

private:
  std::vector<ScenarioDiagnosticSample> samples_ {};
};

inline bool formatScenarioDiagnosticSample(
  char *buffer,
  std::size_t capacity,
  const ScenarioDiagnosticSample &sample
) {
  if (!buffer || capacity == 0 || !validScenarioDiagnosticSample(sample)) {
    return false;
  }

  const auto &p = sample.pose;
  const int n = std::snprintf(buffer, capacity,
    "%s,%d,%d,%d,%.9g,%.9g,%u,%d,%.9g,%d,"
    "%d,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,"
    "%.9g,%.9g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
    "%.9g,%d,%d\n",
    scenarioDiagnosticKindName(sample.kind),
    sample.frame, sample.action, sample.commandMs,
    sample.gameTime, sample.roundStartTime, sample.roundId,
    sample.roundOver ? 1 : 0, sample.freezeTime, sample.freezeBots ? 1 : 0,
    sample.flags, sample.moveType, sample.solid, sample.deadFlag,
    sample.maxSpeed, sample.friction,
    sample.baseVelocity[0], sample.baseVelocity[1], sample.baseVelocity[2],
    sample.forwardSpeed, sample.strafeSpeed,
    p.position[0], p.position[1], p.position[2],
    p.velocity[0], p.velocity[1], p.velocity[2],
    sample.obstacleFraction, sample.obstacleStartSolid ? 1 : 0,
    sample.obstacleAllSolid ? 1 : 0);

  return n > 0 && static_cast<std::size_t>(n) < capacity;
}

enum class ScenarioProgressEvent { None, Stall, Recovered };

// Detect a sustained zero-response interval only while translation is commanded.
// This is diagnostic evidence, never a reason to end or modify a scenario.
class ScenarioStallDetector {
public:
  void clear() {
    stationaryMs_ = 0;
    stalled_ = false;
  }

  ScenarioProgressEvent observe(
    int milliseconds,
    float forwardSpeed,
    float strafeSpeed,
    const ScenarioResultPose &before,
    const ScenarioResultPose &after
  ) {
    const double command = std::hypot(forwardSpeed, strafeSpeed);
    if (milliseconds < 1 || milliseconds > ActionScheduler::kMaxFrameMs
      || !std::isfinite(command) || command < 1.0) {
      clear();
      return ScenarioProgressEvent::None;
    }

    const double dx = after.position[0] - before.position[0];
    const double dy = after.position[1] - before.position[1];
    const double velocity = std::hypot(after.velocity[0], after.velocity[1]);
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(velocity)) {
      clear();
      return ScenarioProgressEvent::None;
    }

    if (std::hypot(dx, dy) > 0.05 || velocity > 0.1) {
      const bool recovered = stalled_;
      clear();
      return recovered ? ScenarioProgressEvent::Recovered : ScenarioProgressEvent::None;
    }

    if (!stalled_) {
      stationaryMs_ += milliseconds;
      if (stationaryMs_ >= 200) {
        stalled_ = true;
        return ScenarioProgressEvent::Stall;
      }
    }

    return ScenarioProgressEvent::None;
  }

private:
  int stationaryMs_ {};
  bool stalled_ {};
};

} // namespace aipb::explorer::scenario
