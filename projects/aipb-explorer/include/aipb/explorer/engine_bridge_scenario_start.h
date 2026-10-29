#pragma once

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_scenario.h>

namespace aipb::explorer::scenario {

// A read-only observed snapshot. Weapon identifiers use GoldSrc weapon ids:
// 0 is no weapon, -1 means the value is unavailable, -2 means multiple weapons
// occupy one slot. No snapshot is permission to change bot state.
struct ScenarioStartObservation {
  double position[3] {};
  float velocity[3] {};
  float yaw {};
  float pitch {};
  bool onGround {};
  bool crouching {};
  float health {};
  float armor {};
  float maxSpeed {};
  int money {};
  bool defuseKit {};
  int primaryId { -1 };
  int secondaryId { -1 };
  int selectedId { -1 };
  int primaryClip { -1 };
  int primaryReserve { -1 };
  int secondaryClip { -1 };
  int secondaryReserve { -1 };
  int he { -1 };
  int flash { -1 };
  int smoke { -1 };
  bool helmetKnown {};
  bool helmet {};
};

struct ScenarioStartFinding {
  std::string field {};
  double expected {};
  double observed {};
  bool unverified {};
};

struct ScenarioStartAssessment {
  int mismatches {};
  int unverified {};
  std::vector<ScenarioStartFinding> findings {};

  bool ready() const {
    return mismatches == 0 && unverified == 0;
  }
};

inline double wrappedScenarioAngle(double expected, double observed) {
  return std::remainder(expected - observed, 360.0);
}

// Compare the declared state to real observed data, not to a desired setup.
// Unreadable equipment is reported, never silently assumed to be present.
inline ScenarioStartAssessment assessScenarioStart(
  const Scenario &scenario,
  const ScenarioStartObservation &actual,
  int expectedPrimaryId,
  int expectedSecondaryId,
  int expectedSelectedId
) {
  ScenarioStartAssessment assessment {};

  const auto compare = [&](const char *field, double expected,
    double observed, double tolerance = 0.0) {
    if (!std::isfinite(observed) || std::abs(expected - observed) > tolerance) {
      ++assessment.mismatches;
      assessment.findings.push_back({field, expected, observed, false});
    }
  };
  const auto compareAmmo = [&](const char *field, int expected, int observed) {
    if (observed < 0) {
      ++assessment.unverified;
      assessment.findings.push_back({field, static_cast<double>(expected),
        static_cast<double>(observed), true});
    }
    else {
      compare(field, expected, observed);
    }
  };

  constexpr const char *kPosition[] { "x", "y", "z" };
  constexpr const char *kVelocity[] { "vx", "vy", "vz" };

  for (int i = 0; i < 3; ++i) {
    compare(kPosition[i], scenario.position[i], actual.position[i], 1.0);
    compare(kVelocity[i], scenario.velocity[i], actual.velocity[i], 1.0);
  }

  compare("yaw_delta", 0.0, wrappedScenarioAngle(scenario.yaw, actual.yaw), 1.0);
  compare("pitch", scenario.pitch, actual.pitch, 1.0);
  compare("grounded", 1.0, actual.onGround ? 1.0 : 0.0);
  compare("crouching", scenario.crouching ? 1.0 : 0.0,
    actual.crouching ? 1.0 : 0.0);
  compare("health", scenario.equipment.health, actual.health, 0.5);
  compare("armor", scenario.equipment.armor, actual.armor, 0.5);
  compare("money", scenario.equipment.money, actual.money);
  compare("defuse_kit", scenario.equipment.defuseKit ? 1.0 : 0.0,
    actual.defuseKit ? 1.0 : 0.0);
  compare("speed_valid", 1.0,
    std::isfinite(actual.maxSpeed) && actual.maxSpeed >= 10.0f ? 1.0 : 0.0);

  compareAmmo("primary_id", expectedPrimaryId, actual.primaryId);
  compareAmmo("secondary_id", expectedSecondaryId, actual.secondaryId);
  compareAmmo("selected_id", expectedSelectedId, actual.selectedId);

  if (expectedPrimaryId > 0 && actual.primaryId == expectedPrimaryId) {
    compareAmmo("primary_clip", scenario.equipment.primary.clip, actual.primaryClip);
    compareAmmo("primary_reserve", scenario.equipment.primary.reserve,
      actual.primaryReserve);
  }

  if (expectedSecondaryId > 0 && actual.secondaryId == expectedSecondaryId) {
    compareAmmo("secondary_clip", scenario.equipment.secondary.clip,
      actual.secondaryClip);
    compareAmmo("secondary_reserve", scenario.equipment.secondary.reserve,
      actual.secondaryReserve);
  }

  compareAmmo("he", scenario.equipment.he, actual.he);
  compareAmmo("flash", scenario.equipment.flash, actual.flash);
  compareAmmo("smoke", scenario.equipment.smoke, actual.smoke);

  if (!actual.helmetKnown) {
    ++assessment.unverified;
    assessment.findings.push_back({"helmet", scenario.equipment.helmet ? 1.0 : 0.0,
      -1.0, true});
  }
  else {
    compare("helmet", scenario.equipment.helmet ? 1.0 : 0.0,
      actual.helmet ? 1.0 : 0.0);
  }

  return assessment;
}

} // namespace aipb::explorer::scenario
