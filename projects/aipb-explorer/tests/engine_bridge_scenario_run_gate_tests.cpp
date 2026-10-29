#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_scenario_run_gate.h>

using namespace aipb::explorer::scenario;

static void check(bool condition) {
  if (!condition) {
    throw std::runtime_error("scenario run gate regression");
  }
}

int main() {
  try {
    Scenario scenario {};
    scenario.name = "run_gate";
    scenario.map = "de_dust2";
    scenario.team = 2;
    scenario.botIndex = 1;
    scenario.timeoutMs = 2000;
    scenario.position[0] = 448.0;
    scenario.position[1] = 2368.0;
    scenario.position[2] = -92.0;
    scenario.yaw = -105.0f;
    scenario.actions.push_back({});
    scenario.actions[0].durationMs = 200;
    scenario.actions[0].timeoutMs = 250;
    scenario.actions.push_back({});
    scenario.actions[1].durationMs = 200;
    scenario.actions[1].timeoutMs = 250;

    ScenarioStartObservation observed {};
    observed.position[0] = 448.0;
    observed.position[1] = 2368.0;
    observed.position[2] = -92.0;
    observed.yaw = -105.0f;
    observed.onGround = true;
    observed.maxSpeed = 250.0f;
    observed.health = 100.0f;
    observed.primaryId = 0;
    observed.secondaryId = 16;

    auto decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.ready());
    check(decision.assessment.mismatches == 0 && decision.assessment.unverified == 0);

    // Omitted constraints never assert the default state.
    observed.velocity[0] = 60.0f;
    observed.money = 1234;
    observed.health = 42.0f;
    observed.secondaryId = 26;
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).ready());

    scenario.specifiedBot.insert("vx");
    scenario.velocity[0] = 0.0f;
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::StartMismatch);
    check(decision.assessment.findings[0].field == "vx");
    observed.velocity[0] = 0.0f;
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).ready());

    // A previously prepared start is invalidated by subsequent motion.
    observed.position[1] = 2400.0;
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::StartMismatch);
    observed.position[1] = 2368.0;

    scenario.equipment.specified.insert("secondary");
    scenario.equipment.secondary.type = "usp";
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::StartMismatch);
    observed.secondaryId = 16;
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).ready());

    scenario.equipment.specified.insert("secondary_clip");
    scenario.equipment.secondary.clip = 0;
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::StartUnverified);
    check(decision.assessment.unverified == 1);
    check(decision.assessment.findings[0].field == "secondary_clip");
    observed.secondaryClip = 0;
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).ready());
    scenario.equipment.specified.erase("secondary_clip");

    scenario.equipment.specified.insert("helmet");
    scenario.equipment.helmet = false;
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::StartUnverified);
    check(decision.assessment.findings[0].field == "helmet");
    observed.helmetKnown = true;
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).ready());
    scenario.equipment.specified.erase("helmet");

    observed.onGround = false;
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).status
      == RunStartStatus::StartMismatch);
    observed.onGround = true;
    observed.maxSpeed = 0.0f;
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).status
      == RunStartStatus::StartMismatch);
    observed.maxSpeed = 250.0f;

    // Reject non-default controls in every action, not just the first.
    scenario.actions[1].attack = true;
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::UnsupportedAction);
    check(decision.actionIndex == 1 && decision.unsupportedField != nullptr);
    check(std::string_view { decision.unsupportedField } == "attack");
    scenario.actions[1].attack = false;

    scenario.actions[1].attack2 = true;
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::UnsupportedAction);
    check(std::string_view { decision.unsupportedField } == "attack2");
    scenario.actions[1].attack2 = false;

    scenario.actions[1].use = true;
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::UnsupportedAction);
    check(std::string_view { decision.unsupportedField } == "use");
    scenario.actions[1].use = false;

    scenario.actions[1].select = "knife";
    decision = checkScenarioRunStart(scenario, observed, 0, 16, 29);
    check(decision.status == RunStartStatus::UnsupportedAction);
    check(std::string_view { decision.unsupportedField } == "select");
    scenario.actions[1].select = "none";

    scenario.actions.clear();
    check(checkScenarioRunStart(scenario, observed, 0, 16, 29).status
      == RunStartStatus::InvalidScenario);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario run gate: %s\n", e.what());
    return 1;
  }
}
