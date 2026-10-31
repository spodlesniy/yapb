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
    Scenario eitherTeam {};
    check(loadScenario(fixtureRoot, "ground_forward_02", eitherTeam, detail)
      == LoadStatus::Ok);
    check(eitherTeam.team == 0 && !eitherTeam.hasBot("team"));
    check(checkScenarioPreflight(eitherTeam, "de_dust2", true, true, 1) == PreflightStatus::Ok);
    check(checkScenarioPreflight(eitherTeam, "de_dust2", true, true, 2) == PreflightStatus::Ok);
    check(checkScenarioPreflight(eitherTeam, "de_dust2", true, true, 0)
      == PreflightStatus::TeamMismatch);
    check(eitherTeam.equipment.specified.empty());
    check(eitherTeam.equipment.primary.type == "none");
    check(eitherTeam.equipment.secondary.type == "none");
    check(eitherTeam.actions.size() == 2);
    check(eitherTeam.actions[0].durationMs == 500
      && eitherTeam.actions[0].forward == 1.0f);
    check(eitherTeam.actions[1].durationMs == 250
      && eitherTeam.actions[1].yawDelta == 45.0f);
    Scenario strafe {};
    check(loadScenario(fixtureRoot, "ground_strafe_coast_01", strafe, detail)
      == LoadStatus::Ok);
    check(strafe.map == "de_dust2" && strafe.team == 0);
    check(strafe.equipment.specified.empty());
    check(checkScenarioPreflight(strafe, "de_dust2", true, true, 2)
      == PreflightStatus::Ok);
    check(strafe.actions.size() == 2);
    check(strafe.actions[0].durationMs == 500 && strafe.actions[0].timeoutMs == 700
      && strafe.actions[0].forward == 0.0f && strafe.actions[0].strafe == 1.0f
      && strafe.actions[0].yawDelta == 0.0f);
    check(strafe.actions[1].durationMs == 1000 && strafe.actions[1].timeoutMs == 1300
      && strafe.actions[1].forward == 0.0f && strafe.actions[1].strafe == 0.0f);
    Scenario diagonal {};
    check(loadScenario(fixtureRoot, "ground_diagonal_coast_01", diagonal, detail)
      == LoadStatus::Ok);
    check(diagonal.name == "ground_diagonal_coast_01" && diagonal.map == "de_dust2");
    check(diagonal.team == 0 && diagonal.equipment.specified.empty());
    check(diagonal.hasBot("vx") && diagonal.hasBot("vy") && diagonal.hasBot("vz"));
    check(checkScenarioPreflight(diagonal, "de_dust2", true, true, 2)
      == PreflightStatus::Ok);
    check(diagonal.actions.size() == 2);
    check(diagonal.actions[0].durationMs == 500
      && diagonal.actions[0].timeoutMs == 700
      && diagonal.actions[0].forward == 1.0f
      && diagonal.actions[0].strafe == 1.0f
      && diagonal.actions[0].yawDelta == 0.0f);
    check(diagonal.actions[1].durationMs == 1000
      && diagonal.actions[1].timeoutMs == 1300
      && diagonal.actions[1].forward == 0.0f
      && diagonal.actions[1].strafe == 0.0f);
    Scenario turning {};
    check(loadScenario(fixtureRoot, "ground_turn_speed_01", turning, detail)
      == LoadStatus::Ok);
    check(turning.map == "de_dust2" && turning.team == 0);
    check(turning.equipment.specified.empty() && turning.actions.size() == 5);
    check(checkScenarioPreflight(turning, "de_dust2", true, true, 2)
      == PreflightStatus::Ok);
    check(turning.actions[0].durationMs == 50 && turning.actions[0].forward == 0.0f);
    check(turning.actions[1].durationMs == 200 && turning.actions[1].forward == 1.0f
      && turning.actions[1].yawDelta == 30.0f);
    check(turning.actions[2].durationMs == 300 && turning.actions[2].yawDelta == 0.0f);
    check(turning.actions[3].durationMs == 200 && turning.actions[3].forward == 1.0f
      && turning.actions[3].yawDelta == -30.0f);
    check(turning.actions[4].durationMs == 500 && turning.actions[4].forward == 0.0f);
    Scenario slope {};
    check(loadScenario(fixtureRoot, "ground_slope_forward_01", slope, detail)
      == LoadStatus::Ok);
    check(slope.map == "de_dust2" && slope.team == 0);
    check(slope.equipment.specified.empty() && slope.actions.size() == 4);
    check(checkScenarioPreflight(slope, "de_dust2", true, true, 2)
      == PreflightStatus::Ok);
    check(slope.position[0] == eitherTeam.position[0]
      && slope.position[1] == eitherTeam.position[1]
      && slope.position[2] == eitherTeam.position[2]
      && slope.yaw == eitherTeam.yaw);
    for (int index = 0; index < 2; ++index) {
      check(slope.actions[index].durationMs == eitherTeam.actions[index].durationMs
        && slope.actions[index].forward == eitherTeam.actions[index].forward
        && slope.actions[index].yawDelta == eitherTeam.actions[index].yawDelta);
    }
    check(slope.actions[2].durationMs == 1 && slope.actions[2].forward == 0.0f
      && slope.actions[2].yawDelta == 90.0f);
    check(slope.actions[3].durationMs == 2000
      && slope.actions[3].forward == 1.0f
      && slope.actions[3].strafe == 0.0f && slope.actions[3].yawDelta == 0.0f);
    Scenario braking {};
    check(loadScenario(fixtureRoot, "ground_active_brake_01", braking, detail)
      == LoadStatus::Ok);
    check(braking.name == "ground_active_brake_01" && braking.map == "de_dust2");
    check(braking.team == 0 && braking.equipment.specified.empty());
    check(braking.hasBot("vx") && braking.hasBot("vy") && braking.hasBot("vz"));
    check(checkScenarioPreflight(braking, "de_dust2", true, true, 2)
      == PreflightStatus::Ok);
    check(braking.actions.size() == 2);
    check(braking.actions[0].durationMs == 500
      && braking.actions[0].timeoutMs == 700
      && braking.actions[0].forward == 1.0f && braking.actions[0].strafe == 0.0f);
    check(braking.actions[1].durationMs == 500
      && braking.actions[1].timeoutMs == 700
      && braking.actions[1].forward == -1.0f && braking.actions[1].strafe == 0.0f
      && braking.actions[1].yawDelta == 0.0f);
    Scenario hold {};
    check(loadScenario(fixtureRoot, "scenario_lifecycle_01", hold, detail)
      == LoadStatus::Ok);
    check(hold.name == "scenario_lifecycle_01");
    check(hold.team == 0 && !hold.hasBot("team"));
    check(hold.equipment.specified.empty());
    check(checkScenarioPreflight(hold, "de_dust2", true, true, 1)
      == PreflightStatus::Ok);
    check(checkScenarioPreflight(hold, "de_dust2", true, true, 2)
      == PreflightStatus::Ok);
    check(hold.actions.size() == 1);
    check(hold.actions[0].durationMs == 8000
      && hold.actions[0].timeoutMs == 10500);
    check(hold.actions[0].forward == 0.0f && hold.actions[0].strafe == 0.0f);
    check(!hold.actions[0].jump && !hold.actions[0].duck);
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
