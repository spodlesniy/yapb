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
    for (const char *key : {"x", "y", "z", "yaw", "pitch",
      "vx", "vy", "vz", "crouching"}) {
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
    check(!plan.setPosition && plan.position[0] == 0.0f && plan.yaw == 359.0f);
    check(!plan.crouching);

    // Omitted start fields retain the live bot pose without preflight mismatches.
    a.position[0] = 108.75;
    a.position[1] = -54.5;
    a.position[2] = 13.25;
    a.yaw = 72.0f;
    a.pitch = -9.0f;
    check(assessScenarioStart(s, a, 0, 0, 29).ready());
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::Ready);
    check(!plan.setPosition && plan.position[0] == 108.75f);
    check(plan.position[1] == -54.5f && plan.position[2] == 13.25f);
    check(plan.yaw == 72.0f && plan.pitch == -9.0f);
    s.specifiedBot.insert("yaw");
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::Ready);
    check(!plan.setPosition && plan.yaw == -1.0f && plan.pitch == -9.0f);
    check(assessScenarioStart(s, a, 0, 0, 29).mismatches == 1);
    s.specifiedBot.erase("yaw");
    s.specifiedBot.insert("pitch");
    s.pitch = 10.0f;
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::Ready);
    check(plan.yaw == 72.0f && plan.pitch == 10.0f);
    check(assessScenarioStart(s, a, 0, 0, 29).mismatches == 1);
    s.specifiedBot.erase("pitch");
    s.specifiedBot.insert("x");
    s.specifiedBot.insert("y");
    s.specifiedBot.insert("z");
    check(planScenarioPose(s, a, false, plan) == PoseInitStatus::Ready);
    check(plan.setPosition && plan.position[0] == 0.0f);
    check(assessScenarioStart(s, a, 0, 0, 29).mismatches == 3);
    s.specifiedBot.erase("x");
    s.specifiedBot.erase("y");
    s.specifiedBot.erase("z");
    a.position[0] = 0.0;
    a.position[1] = 0.0;
    a.position[2] = 0.0;
    a.yaw = 359.0f;
    a.pitch = 0.0f;


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

    // Match the GoldSrc pistol ids: USP=16, Deagle=26, Glock18=17.
    const std::uint32_t secondaryMask = (std::uint32_t { 1 } << 16)
      | (std::uint32_t { 1 } << 17) | (std::uint32_t { 1 } << 26);
    const std::uint32_t primaryMask = (std::uint32_t { 1 } << 22)
      | (std::uint32_t { 1 } << 28);
    check(observedScenarioWeaponId(0, secondaryMask) == 0);
    check(observedScenarioWeaponId(std::uint32_t { 1 } << 26, secondaryMask) == 26);
    check(observedScenarioWeaponId(std::uint32_t { 1 } << 16, secondaryMask) == 16);
    check(observedScenarioWeaponId((std::uint32_t { 1 } << 16)
      | (std::uint32_t { 1 } << 26), secondaryMask) == -2);
    check(observedScenarioWeaponId(std::uint32_t { 1 } << 22, secondaryMask) == 0);
    check(observedScenarioWeaponId(std::uint32_t { 1 } << 22, primaryMask) == 22);
    check(observedScenarioWeaponId(std::uint32_t { 1 } << 28, primaryMask) == 28);
    check(observedScenarioWeaponId((std::uint32_t { 1 } << 22)
      | (std::uint32_t { 1 } << 28), primaryMask) == -2);
    check(observedScenarioWeaponId(0, 0) == -1);

    s.equipment.selected = "knife";
    s.equipment.secondary.type = "usp";
    s.equipment.specified.clear();
    s.equipment.specified.insert("secondary");
    a.position[0] = 0.0;
    a.yaw = -1.0f;
    a.velocity[0] = 0.0f;
    a.secondaryId = observedScenarioWeaponId(std::uint32_t { 1 } << 26, secondaryMask);
    auto wrongPistol = assessScenarioStart(s, a, 0, 16, 29);
    check(wrongPistol.mismatches == 1 && wrongPistol.unverified == 0);
    check(wrongPistol.findings.size() == 1
      && wrongPistol.findings[0].field == "secondary_id"
      && wrongPistol.findings[0].observed == 26.0);

    a.secondaryId = observedScenarioWeaponId((std::uint32_t { 1 } << 16)
      | (std::uint32_t { 1 } << 26), secondaryMask);
    check(assessScenarioStart(s, a, 0, 16, 29).unverified == 1);
    a.secondaryId = observedScenarioWeaponId(std::uint32_t { 1 } << 16, secondaryMask);
    check(assessScenarioStart(s, a, 0, 16, 29).ready());
    s.equipment.specified.insert("secondary_clip");
    s.equipment.secondary.clip = 0;
    a.secondaryClip = 12;
    check(assessScenarioStart(s, a, 0, 16, 29).mismatches == 1);
    s.equipment.specified.erase("secondary_clip");
    check(assessScenarioStart(s, a, 0, 16, 29).ready());

    check(observedScenarioGrenadeCount(false, 2, 2) == 0);
    check(observedScenarioGrenadeCount(true, -1, 1) == 1);
    check(observedScenarioGrenadeCount(true, 0, 1) == 1);
    check(observedScenarioGrenadeCount(true, 1, 2) == 1);
    check(observedScenarioGrenadeCount(true, 2, 2) == 2);
    check(observedScenarioGrenadeCount(true, 0, 2) == -2);
    check(observedScenarioGrenadeCount(true, 3, 2) == -2);

    // A known grenade presence must fail an explicit zero constraint.
    s.equipment.specified.insert("he");
    s.equipment.he = 0;
    a.he = observedScenarioGrenadeCount(true, -1, 1);
    auto grenadeReport = assessScenarioStart(s, a, 0, 16, 29);
    check(grenadeReport.mismatches == 1 && grenadeReport.unverified == 0);
    check(grenadeReport.findings.size() == 1
      && grenadeReport.findings[0].field == "he");

    s.equipment.specified.erase("he");
    s.equipment.specified.insert("flash");
    s.equipment.flash = 0;
    a.flash = observedScenarioGrenadeCount(true, 0, 2);
    grenadeReport = assessScenarioStart(s, a, 0, 16, 29);
    check(grenadeReport.mismatches == 1 && grenadeReport.unverified == 0);
    check(grenadeReport.findings[0].field == "flash_present");

    s.equipment.flash = 1;
    grenadeReport = assessScenarioStart(s, a, 0, 16, 29);
    check(grenadeReport.mismatches == 0 && grenadeReport.unverified == 1);
    a.flash = observedScenarioGrenadeCount(true, 2, 2);
    grenadeReport = assessScenarioStart(s, a, 0, 16, 29);
    check(grenadeReport.mismatches == 1 && grenadeReport.unverified == 0);
    s.equipment.flash = 2;
    check(assessScenarioStart(s, a, 0, 16, 29).ready());
    s.equipment.specified.erase("flash");
    check(assessScenarioStart(s, a, 0, 16, 29).ready());
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario start: %s\n", e.what());
    return 1;
  }
}
