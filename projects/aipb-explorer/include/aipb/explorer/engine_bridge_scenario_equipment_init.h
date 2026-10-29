#pragma once

#include <string>

#include <aipb/explorer/engine_bridge_scenario_start.h>

namespace aipb::explorer::scenario {

enum class EquipmentInitStatus {
  Ready,
  InvalidScenario,
  Busy,
  UnsafeBot,
  UnsatisfiedConstraint,
  ArmorTypeChange
};

inline const char *equipmentInitStatusName(EquipmentInitStatus status) {
  switch (status) {
  case EquipmentInitStatus::Ready: return "Ready";
  case EquipmentInitStatus::InvalidScenario: return "InvalidScenario";
  case EquipmentInitStatus::Busy: return "Busy";
  case EquipmentInitStatus::UnsafeBot: return "UnsafeBot";
  case EquipmentInitStatus::UnsatisfiedConstraint: return "UnsatisfiedConstraint";
  case EquipmentInitStatus::ArmorTypeChange: return "ArmorTypeChange";
  }

  return "InvalidScenario";
}

struct EquipmentInitPlan {
  bool setHealth {};
  float health {};
  bool setArmor {};
  float armor {};
};

struct EquipmentInitDecision {
  EquipmentInitStatus status { EquipmentInitStatus::InvalidScenario };
  EquipmentInitPlan plan {};
  std::string detail {};

  bool ready() const {
    return status == EquipmentInitStatus::Ready;
  }
};

// Only numeric health and existing-vest armor amounts are writable by this
// adapter. All other explicit constraints must already match before mutation.
inline EquipmentInitDecision planScenarioEquipment(
  const Scenario &scenario,
  const ScenarioStartObservation &actual,
  bool explorerBusy,
  int expectedPrimaryId,
  int expectedSecondaryId,
  int expectedSelectedId
) {
  if (!validScenario(scenario)) {
    return { EquipmentInitStatus::InvalidScenario, {}, "scenario" };
  }

  if (explorerBusy) {
    return { EquipmentInitStatus::Busy, {}, "explorer" };
  }

  if (!actual.onGround || !std::isfinite(actual.maxSpeed)
    || actual.maxSpeed < 10.0f || !std::isfinite(actual.health)
    || actual.health <= 0.0f || !std::isfinite(actual.armor)
    || actual.armor < 0.0f || actual.armor > 100.0f) {
    return { EquipmentInitStatus::UnsafeBot, {}, "bot" };
  }

  Scenario readOnly = scenario;

  // Position, view and optional bot constraints belong to init-pose, not equipment.
  for (int axis = 0; axis < 3; ++axis) {
    readOnly.position[axis] = actual.position[axis];
  }

  readOnly.yaw = actual.yaw;
  readOnly.pitch = actual.pitch;
  readOnly.specifiedBot.clear();
  readOnly.equipment.specified.erase("health");
  readOnly.equipment.specified.erase("armor");

  const auto check = assessScenarioStart(
    readOnly, actual, expectedPrimaryId, expectedSecondaryId, expectedSelectedId);

  if (!check.ready()) {
    return { EquipmentInitStatus::UnsatisfiedConstraint, {},
      check.findings.empty() ? "equipment" : check.findings.front().field };
  }

  EquipmentInitPlan plan {};

  if (scenario.equipment.has("armor")) {
    const float desired = static_cast<float>(scenario.equipment.armor);

    // Changing a 0/nonzero vest state needs the CS game DLL's kevlar type.
    // A bare pev->armorvalue write cannot create or remove that game state.
    if ((desired > 0.0f) != (actual.armor > 0.0f)) {
      return { EquipmentInitStatus::ArmorTypeChange, {}, "armor" };
    }

    plan.setArmor = true;
    plan.armor = desired;
  }

  if (scenario.equipment.has("health")) {
    plan.setHealth = true;
    plan.health = static_cast<float>(scenario.equipment.health);
  }

  return { EquipmentInitStatus::Ready, plan, {} };
}

} // namespace aipb::explorer::scenario
