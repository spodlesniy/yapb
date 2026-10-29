#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_scenario_loader.h>

using namespace aipb::explorer::scenario;

static void check(bool value) {
  if (!value) {
    throw std::runtime_error("scenario loader regression");
  }
}

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "aipb_d254_scenario_test";
    std::filesystem::create_directories(root / "scenarios");
    const std::string input = R"([scenario]
version=aipb-scenario-v1
name=smoke
map=de_dust2
timeout_ms=1000
[bot]
index=1
team=2
x=448
y=2368
z=-92
yaw=0
pitch=0
[action]
duration_ms=100
)";
    { std::ofstream file { root / "scenarios" / "smoke.scenario", std::ios::binary }; file << input; }
    Scenario scenario {};
    std::string detail {};
    check(loadScenario(root.string(), "smoke", scenario, detail) == LoadStatus::Ok);
    check(scenario.name == "smoke" && scenario.actions.size() == 1);
    check(checkScenarioPreflight(scenario, "de_dust2", true, true, 2) == PreflightStatus::Ok);
    check(checkScenarioPreflight(scenario, "de_inferno", true, true, 2) == PreflightStatus::MapMismatch);
    check(checkScenarioPreflight(scenario, "de_dust2", false, false, 2) == PreflightStatus::BotMissing);
    check(checkScenarioPreflight(scenario, "de_dust2", true, false, 2) == PreflightStatus::BotDead);
    check(checkScenarioPreflight(scenario, "de_dust2", true, true, 1) == PreflightStatus::TeamMismatch);
    scenario.name = "unchanged";
    check(loadScenario(root.string(), "../smoke", scenario, detail) == LoadStatus::InvalidName);
    check(loadScenario(root.string(), "unknown", scenario, detail) == LoadStatus::NotFoundOrUnreadable);
    { std::ofstream file { root / "scenarios" / "broken.scenario", std::ios::binary }; file << "invalid"; }
    check(loadScenario(root.string(), "broken", scenario, detail) == LoadStatus::InvalidFormat);
    { std::ofstream file { root / "scenarios" / "oversized.scenario", std::ios::binary }; file << std::string(65537, 'a'); }
    check(loadScenario(root.string(), "oversized", scenario, detail) == LoadStatus::TooLarge);
    check(scenario.name == "unchanged");
    std::filesystem::remove_all(root);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "scenario loader: %s\n", error.what());
    return 1;
  }
}
