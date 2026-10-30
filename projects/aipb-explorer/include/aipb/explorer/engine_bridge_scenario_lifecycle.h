#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace aipb::explorer::scenario {

// The recorded round start is the end of Counter-Strike mp_freezetime.
// Reject experiments while game movement is disabled by the round phase.
inline bool scenarioRoundIsFrozen(float gameTime, float activeRoundStart) {
  return std::isfinite(gameTime) && std::isfinite(activeRoundStart)
    && gameTime >= 0.0f && activeRoundStart > gameTime;
}


enum class RunLifecycleFailure {
  None,
  RoundOver,
  RoundChanged,
  MapChanged,
  BotMissing,
  BotReplaced,
  BotDead,
  TeamChanged,
  SpeedInvalid,
  ExplorerConflict
};

inline const char *runLifecycleFailureName(RunLifecycleFailure failure) {
  switch (failure) {
  case RunLifecycleFailure::None: return "None";
  case RunLifecycleFailure::RoundOver: return "RoundOver";
  case RunLifecycleFailure::RoundChanged: return "RoundChanged";
  case RunLifecycleFailure::MapChanged: return "MapChanged";
  case RunLifecycleFailure::BotMissing: return "BotMissing";
  case RunLifecycleFailure::BotReplaced: return "BotReplaced";
  case RunLifecycleFailure::BotDead: return "BotDead";
  case RunLifecycleFailure::TeamChanged: return "TeamChanged";
  case RunLifecycleFailure::SpeedInvalid: return "SpeedInvalid";
  case RunLifecycleFailure::ExplorerConflict: return "ExplorerConflict";
  }
  return "BotMissing";
}

struct RunLifecycleObservation {
  bool roundOver {};
  std::uint32_t roundId {};
  std::string_view map {};
  bool botPresent {};
  bool botSame {};
  bool botAlive {};
  int team {};
  bool speedValid {};
  bool explorerBusy {};
};

// The round number is checked even when the engine's round-over flag has
// already been cleared by the next round's initialization.
inline RunLifecycleFailure checkRunLifecycle(
  const RunLifecycleObservation &observation,
  std::string_view expectedMap,
  std::uint32_t expectedRound,
  int expectedTeam
) {
  if (observation.roundOver) {
    return RunLifecycleFailure::RoundOver;
  }
  if (observation.roundId != expectedRound) {
    return RunLifecycleFailure::RoundChanged;
  }
  if (observation.map != expectedMap) {
    return RunLifecycleFailure::MapChanged;
  }
  if (!observation.botPresent) {
    return RunLifecycleFailure::BotMissing;
  }
  if (!observation.botSame) {
    return RunLifecycleFailure::BotReplaced;
  }
  if (!observation.botAlive) {
    return RunLifecycleFailure::BotDead;
  }
  if (observation.team != expectedTeam) {
    return RunLifecycleFailure::TeamChanged;
  }
  if (!observation.speedValid) {
    return RunLifecycleFailure::SpeedInvalid;
  }
  if (observation.explorerBusy) {
    return RunLifecycleFailure::ExplorerConflict;
  }
  return RunLifecycleFailure::None;
}

} // namespace aipb::explorer::scenario
