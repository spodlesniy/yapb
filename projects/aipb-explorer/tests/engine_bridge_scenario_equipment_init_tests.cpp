#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_scenario_equipment_init.h>
#include <aipb/explorer/engine_bridge_scenario_loader.h>

using namespace aipb::explorer::scenario;

static void check(bool value) {
  if (!value) {
    throw std::runtime_error("scenario equipment init regression");
  }
}

int main() {
  try {
    Scenario scenario {};
    scenario.name = "equipment_test";
    scenario.map = "de_dust2";
    scenario.botIndex = 1;
    scenario.team = 2;
    scenario.timeoutMs = 1000;
    scenario.actions.push_back({});
    scenario.actions[0].durationMs = 100;
    scenario.actions[0].timeoutMs = 100;

    ScenarioStartObservation actual {};
    actual.onGround = true;
    actual.maxSpeed = 250.0f;
    actual.health = 100.0f;
    actual.armor = 80.0f;
    actual.primaryId = 0;
    actual.secondaryId = 16;
    actual.selectedId = 16;
    actual.helmetKnown = false;

    scenario.equipment.specified.insert("health");
    scenario.equipment.health = 75;
    scenario.equipment.specified.insert("armor");
    scenario.equipment.armor = 50;
    scenario.equipment.specified.insert("secondary");
    scenario.equipment.secondary.type = "usp";

    auto decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.ready());
    check(decision.plan.setHealth && decision.plan.health == 75.0f);
    check(decision.plan.setArmor && decision.plan.armor == 50.0f);

    decision = planScenarioEquipment(scenario, actual, true, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::Busy);
    check(!decision.plan.setHealth && !decision.plan.setArmor);

    actual.onGround = false;
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::UnsafeBot);
    actual.onGround = true;

    actual.secondaryId = 26;
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::UnsatisfiedConstraint);
    check(decision.detail == "secondary_id");
    check(!decision.plan.setHealth);
    actual.secondaryId = 16;

    scenario.equipment.specified.insert("money");
    scenario.equipment.money = 200;
    actual.money = 200;
    check(planScenarioEquipment(scenario, actual, false, 0, 16, 29).ready());
    actual.money = 800;
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::UnsatisfiedConstraint);
    check(decision.detail == "money");
    scenario.equipment.specified.erase("money");

    scenario.equipment.specified.insert("helmet");
    scenario.equipment.helmet = false;
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::UnsatisfiedConstraint);
    check(decision.detail == "helmet");
    actual.helmetKnown = true;
    check(planScenarioEquipment(scenario, actual, false, 0, 16, 29).ready());
    scenario.equipment.specified.erase("helmet");

    actual.armor = 0.0f;
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::ArmorTypeChange);
    check(!decision.plan.setHealth);
    actual.armor = 80.0f;
    scenario.equipment.armor = 0;
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::ArmorTypeChange);
    scenario.equipment.armor = 50;

    scenario.equipment.specified.clear();
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.ready() && !decision.plan.setHealth && !decision.plan.setArmor);
    scenario.equipment.specified.insert("health");
    scenario.equipment.health = 101;
    decision = planScenarioEquipment(scenario, actual, false, 0, 16, 29);
    check(decision.status == EquipmentInitStatus::InvalidScenario);

    // The deployed HLDS example must keep all unconstrained equipment omitted.
    Scenario fixture {};
    std::string detail {};
    const auto status = loadScenario(
      AIPB_EXPLORER_SCENARIO_FIXTURE_DIR, "equipment_health_01", fixture, detail);
    if (status != LoadStatus::Ok) {
      throw std::runtime_error(std::string { "fixture load: " }
        + loadStatusName(status) + ": " + detail);
    }
    check(fixture.equipment.has("health") && fixture.equipment.health == 75);
    check(!fixture.equipment.has("armor") && !fixture.equipment.has("helmet"));
    check(!fixture.equipment.has("primary") && !fixture.equipment.has("money"));
    actual.armor = 0.0f;
    decision = planScenarioEquipment(fixture, actual, false, 0, 0, 29);
    check(decision.ready());
    check(decision.plan.setHealth && decision.plan.health == 75.0f);
    check(!decision.plan.setArmor);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario equipment init: %s\n", e.what());
    return 1;
  }
}
