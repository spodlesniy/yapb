#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_scenario_loader.h>
#include <aipb/explorer/engine_bridge_scenario_start.h>

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

    // Keep the two real HLDS examples loadable and verify their presence semantics.
    const std::string fixtureRoot { AIPB_EXPLORER_SCENARIO_FIXTURE_DIR };
    Scenario omitted {};
    Scenario explicitValues {};
    check(loadScenario(fixtureRoot, "optional_omitted_01", omitted, detail) == LoadStatus::Ok);
    check(loadScenario(fixtureRoot, "optional_explicit_01", explicitValues, detail) == LoadStatus::Ok);
    check(!omitted.hasBot("vx") && !omitted.hasBot("crouching"));
    check(omitted.equipment.has("secondary") && !omitted.equipment.has("secondary_clip"));
    check(!omitted.equipment.has("secondary_reserve") && !omitted.equipment.has("health"));
    check(!omitted.equipment.has("helmet") && !omitted.equipment.has("defuse_kit"));
    check(explicitValues.hasBot("vx") && explicitValues.hasBot("crouching"));
    check(explicitValues.equipment.has("primary") && explicitValues.equipment.primary.type == "none");
    check(explicitValues.equipment.has("secondary_clip") && explicitValues.equipment.secondary.clip == 0);
    check(!explicitValues.equipment.has("secondary_reserve"));
    check(explicitValues.equipment.has("helmet") && !explicitValues.equipment.helmet);
    check(explicitValues.equipment.has("defuse_kit") && !explicitValues.equipment.defuseKit);

    ScenarioStartObservation observed {};
    observed.position[0] = 448;
    observed.position[1] = 2368;
    observed.position[2] = -92;
    observed.yaw = -105;
    observed.onGround = true;
    observed.maxSpeed = 250;
    observed.primaryId = 0;
    observed.secondaryId = 16;
    observed.selectedId = 16;
    observed.secondaryClip = 12;
    observed.secondaryReserve = -1;
    observed.he = 0;
    observed.flash = 0;
    observed.smoke = 0;
    observed.money = 800;
    observed.health = 37;
    check(assessScenarioStart(omitted, observed, 0, 16, 16).ready());
    observed.health = 100;
    const auto strictReport = assessScenarioStart(explicitValues, observed, 0, 16, 16);
    check(strictReport.mismatches == 2 && strictReport.unverified == 1);
    observed.secondaryClip = 0;
    observed.money = 0;
    observed.helmetKnown = true;
    check(assessScenarioStart(explicitValues, observed, 0, 16, 16).ready());

    std::filesystem::remove_all(root);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "scenario loader: %s\n", error.what());
    return 1;
  }
}
