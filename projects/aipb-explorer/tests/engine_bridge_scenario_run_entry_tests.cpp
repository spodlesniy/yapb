#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_scenario_run_entry.h>

using namespace aipb::explorer::scenario;

static void check(bool condition) {
  if (!condition) {
    throw std::runtime_error("scenario run entry regression");
  }
}

int main() {
  try {
    check(validScenarioRunArguments(2, "equipment_health_01"));
    check(!validScenarioRunArguments(1, "equipment_health_01"));
    check(!validScenarioRunArguments(3, "equipment_health_01"));
    check(!validScenarioRunArguments(2, "../unsafe"));
    check(!validScenarioRunArguments(2, ""));
    check(validScenarioControlArguments(1));
    check(!validScenarioControlArguments(2));
    check(!validScenarioControlArguments(0));

    Scenario s {};
    s.name = "entry_test";
    s.map = "de_dust2";
    s.botIndex = 1;
    s.team = 2;
    s.timeoutMs = 1000;
    s.actions.push_back({});
    s.actions[0].durationMs = 100;
    s.actions[0].timeoutMs = 100;

    check(checkScenarioRunEntry(s, "de_dust2", true, true, 2, false)
      == RunEntryStatus::Validated);
    check(checkScenarioRunEntry(s, "de_dust2", true, true, 2, true)
      == RunEntryStatus::ExplorerBusy);
    check(checkScenarioRunEntry(s, "de_inferno", true, true, 2, false)
      == RunEntryStatus::MapMismatch);
    check(checkScenarioRunEntry(s, "de_dust2", false, false, 0, false)
      == RunEntryStatus::BotMissing);
    check(checkScenarioRunEntry(s, "de_dust2", true, false, 2, false)
      == RunEntryStatus::BotDead);
    check(checkScenarioRunEntry(s, "de_dust2", true, true, 1, false)
      == RunEntryStatus::TeamMismatch);

    s.actions.clear();
    check(checkScenarioRunEntry(s, "de_dust2", true, true, 2, false)
      == RunEntryStatus::InvalidScenario);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario run entry: %s\n", e.what());
    return 1;
  }
}
