#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include <aipb/explorer/engine_bridge_scenario_action_scheduler.h>

namespace aipb::explorer::scenario {

inline constexpr const char *kScenarioResultVersion = "aipb-scenario-result-v1";

enum class ScenarioEndSource {
  Live,
  LastCommitted,
  Missing
};

inline const char *scenarioEndSourceName(ScenarioEndSource source) {
  switch (source) {
  case ScenarioEndSource::Live: return "live";
  case ScenarioEndSource::LastCommitted: return "last_committed";
  case ScenarioEndSource::Missing: return "missing";
  }
  return "missing";
}

struct ScenarioResultPose {
  double position[3] {};
  double velocity[3] {};
  double yaw {};
  double pitch {};
  bool onGround {};
  bool crouching {};
};

struct ScenarioResult {
  std::string name {};
  std::string map {};
  int botIndex {};
  int team {};
  std::uint32_t roundId {};
  ActionScheduleStatus status { ActionScheduleStatus::Idle };
  std::string reason {};
  int frames {};
  int commandMs {};
  std::int64_t startGameMs {};
  std::int64_t endGameMs { -1 };
  ScenarioEndSource endSource { ScenarioEndSource::Missing };
  ScenarioResultPose start {};
  ScenarioResultPose end {};
};

inline bool validScenarioResultPose(const ScenarioResultPose &pose) {
  for (int i = 0; i < 3; ++i) {
    if (!std::isfinite(pose.position[i]) || !std::isfinite(pose.velocity[i])) {
      return false;
    }
  }
  return std::isfinite(pose.yaw) && std::isfinite(pose.pitch);
}

inline bool safeScenarioResultReason(const std::string &reason) {
  if (reason.empty() || reason.size() > 63) {
    return false;
  }
  for (const char c : reason) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
      || c == ' ' || c == '-' || c == '_')) {
      return false;
    }
  }
  return true;
}

inline bool validScenarioResult(const ScenarioResult &result) {
  if (!safeIdentifier(result.name) || !safeIdentifier(result.map)
    || result.botIndex < 1 || result.botIndex > 32
    || (result.team != 1 && result.team != 2)
    || result.status == ActionScheduleStatus::Idle
    || result.status == ActionScheduleStatus::Running
    || !safeScenarioResultReason(result.reason)
    || result.frames < 0 || result.frames > ActionScheduler::kMaxFrames
    || result.commandMs < 0 || result.commandMs > kMaxScenarioMs
    || result.startGameMs < 0 || result.endGameMs < -1
    || (result.endGameMs >= 0 && result.endGameMs < result.startGameMs)
    || !validScenarioResultPose(result.start)) {
    return false;
  }
  return result.endSource == ScenarioEndSource::Missing
    || validScenarioResultPose(result.end);
}

// The terminal source identifies whether end coordinates are live, last
// observed after a committed frame, or unavailable (zero placeholders).
inline bool formatScenarioResult(
  char *buffer, std::size_t capacity, const ScenarioResult &result
) {
  if (!buffer || capacity == 0 || !validScenarioResult(result)) {
    return false;
  }
  const auto &a = result.start;
  const ScenarioResultPose missing {};
  const auto &b = result.endSource == ScenarioEndSource::Missing ? missing : result.end;
  const int written = std::snprintf(buffer, capacity,
    "%s,%s,%s,%d,%d,%u,%s,%s,%d,%d,%lld,%lld,%s,"
    "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,"
    "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d\n",
    kScenarioResultVersion, result.name.c_str(), result.map.c_str(),
    result.botIndex, result.team, static_cast<unsigned>(result.roundId),
    actionScheduleStatusName(result.status), result.reason.c_str(),
    result.frames, result.commandMs,
    static_cast<long long>(result.startGameMs),
    static_cast<long long>(result.endGameMs),
    scenarioEndSourceName(result.endSource),
    a.position[0], a.position[1], a.position[2],
    a.velocity[0], a.velocity[1], a.velocity[2], a.yaw, a.pitch,
    a.onGround ? 1 : 0, a.crouching ? 1 : 0,
    b.position[0], b.position[1], b.position[2],
    b.velocity[0], b.velocity[1], b.velocity[2], b.yaw, b.pitch,
    b.onGround ? 1 : 0, b.crouching ? 1 : 0);
  return written > 0 && static_cast<std::size_t>(written) < capacity;
}

} // namespace aipb::explorer::scenario
