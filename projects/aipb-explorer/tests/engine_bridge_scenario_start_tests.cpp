#include <cstdio>
#include <stdexcept>
#include <limits>

#include <aipb/explorer/engine_bridge_scenario_pose.h>

using namespace aipb::explorer::scenario;

static void check(bool value) {
  if (!value) {
    throw std::runtime_error("scenario start regression");
  }
}

int main() {
  try {
    Scenario s {};
    s.name = "test";
    s.map = "de_dust2";
    s.botIndex = 1;
    s.team = 2;
    s.timeoutMs = 1000;
    s.equipment.selected = "knife";
    for (const char *key : {"primary", "secondary", "selected", "he", "flash", "smoke", "helmet", "money", "health", "armor", "defuse_kit"}) {
      s.equipment.specified.insert(key);
    }
    for (const char *key : {"vx", "vy", "vz", "crouching"}) {
      s.specifiedBot.insert(key);
    }
    s.actions.push_back({});
    s.actions[0].durationMs = 100;
    s.actions[0].timeoutMs = 100;
    ScenarioStartObservation a {};
    a.onGround = true;
    a.health = 100;
    a.maxSpeed = 250;
    a.primaryId = 0;
    a.secondaryId = 0;
    a.selectedId = 29;
    a.he = 0;
    a.flash = 0;
    a.smoke = 0;
    a.helmetKnown = true;
    auto report = assessScenarioStart(s, a, 0, 0, 29);
    check(report.ready());
    a.position[0] = 5;
    a.flash = -1;
    a.helmetKnown = false;
    report = assessScenarioStart(s, a, 0, 0, 29);
    check(report.mismatches == 1 && report.unverified == 2);
    a.position[0] = 0;
    a.flash = 0;
    a.helmetKnown = true;
    a.yaw = 359;
    s.yaw = -1;
    check(assessScenarioStart(s, a, 0, 0, 29).ready());
    a.primaryId = -1;
    check(assessScenarioStart(s, a, 0, 0, 29).unverified == 1);
    s.equipment.specified.clear();
    s.specifiedBot.clear();
    a.flash = -1;
    a.helmetKnown = false;
    a.health = 13;
    a.money = 15000;
    a.defuseKit = true;
    check(assessScenarioStart(s, a, 0, 0, 29).ready());
    s.equipment.specified.insert("defuse_kit");
    check(assessScenarioStart(s, a, 0, 0, 29).mismatches == 1);
    s.equipment.defuseKit = true;
    check(assessScenarioStart(s, a, 0, 0, 29).ready());

    PoseInitPlan plan {};
    a.velocity[0] = 35.0f;
    a.velocity[1] = -10.0f;
    a.velocity[2] = 0.0f;
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::Ready);
    check(plan.velocity[0] == 35.0f && plan.velocity[1] == -10.0f);
    check(plan.position[0] == 0.0f && plan.yaw == -1.0f);
    check(!plan.crouching);

    s.specifiedBot.insert("vx");
    s.velocity[0] = 0.0f;
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::Ready);
    check(plan.velocity[0] == 0.0f && plan.velocity[1] == -10.0f);
    s.specifiedBot.insert("crouching");
    s.crouching = true;
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::StanceMismatch);
    check(plan.velocity[0] == 0.0f);
    s.crouching = false;

    check(planScenarioPose(s, a, true, plan) == PoseInitStatus::Busy);
    a.onGround = false;
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::BotNotGrounded);
    a.onGround = true;
    a.maxSpeed = 0.0f;
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::SpeedUnavailable);
    a.maxSpeed = 250.0f;

    s.specifiedBot.erase("vx");
    a.velocity[0] = std::numeric_limits<float>::quiet_NaN();
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::InvalidVelocity);
    s.specifiedBot.insert("vx");
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::Ready);
    s.equipment.selected = "invalid";
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::InvalidScenario);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario start: %s\n", e.what());
    return 1;
  }
}
