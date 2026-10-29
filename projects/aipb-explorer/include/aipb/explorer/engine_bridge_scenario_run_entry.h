#pragma once

#include <string_view>

#include <aipb/explorer/engine_bridge_scenario_loader.h>

namespace aipb::explorer::scenario {

enum class RunEntryStatus {
  Validated,
  InvalidScenario,
  ExplorerBusy,
  MapMismatch,
  BotMissing,
  BotDead,
  TeamMismatch
};

inline const char *runEntryStatusName(RunEntryStatus status) {
  switch (status) {
  case RunEntryStatus::Validated: return "Validated";
  case RunEntryStatus::InvalidScenario: return "InvalidScenario";
  case RunEntryStatus::ExplorerBusy: return "ExplorerBusy";
  case RunEntryStatus::MapMismatch: return "MapMismatch";
  case RunEntryStatus::BotMissing: return "BotMissing";
  case RunEntryStatus::BotDead: return "BotDead";
  case RunEntryStatus::TeamMismatch: return "TeamMismatch";
  }

  return "InvalidScenario";
}

inline bool validScenarioRunArguments(int argc, std::string_view name) {
  return argc == 2 && safeIdentifier(name);
}

inline bool validScenarioControlArguments(int argc) {
  return argc == 1;
}

// Entry validation only. This never authorizes action execution; the complete
// start-state assessment and runtime scheduler are separate tasks.
inline RunEntryStatus checkScenarioRunEntry(
  const Scenario &scenario,
  std::string_view map,
  bool botPresent,
  bool botAlive,
  int team,
  bool explorerBusy
) {
  if (!validScenario(scenario)) {
    return RunEntryStatus::InvalidScenario;
  }

  if (explorerBusy) {
    return RunEntryStatus::ExplorerBusy;
  }

  switch (checkScenarioPreflight(scenario, map, botPresent, botAlive, team)) {
  case PreflightStatus::Ok: return RunEntryStatus::Validated;
  case PreflightStatus::MapMismatch: return RunEntryStatus::MapMismatch;
  case PreflightStatus::BotMissing: return RunEntryStatus::BotMissing;
  case PreflightStatus::BotDead: return RunEntryStatus::BotDead;
  case PreflightStatus::TeamMismatch: return RunEntryStatus::TeamMismatch;
  }

  return RunEntryStatus::InvalidScenario;
}

} // namespace aipb::explorer::scenario
