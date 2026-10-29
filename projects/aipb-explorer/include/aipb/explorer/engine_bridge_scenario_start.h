#pragma once

#include <cmath>
#include <cstdint>
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

// The caller supplies the established CS primary or secondary weapon mask.
// Report one concrete GoldSrc weapon id; multiple owned weapons are ambiguous.
inline int observedScenarioWeaponId(
  std::uint32_t inventory,
  std::uint32_t slotMask
) {
  if (slotMask == 0) {
    return -1;
  }

  const std::uint32_t owned = inventory & slotMask;

  if (owned == 0) {
    return 0;
  }

  if ((owned & (owned - 1)) != 0) {
    return -2;
  }

  for (int id = 1; id < 32; ++id) {
    if ((owned & (std::uint32_t { 1 } << id)) != 0) {
      return id;
    }
  }

  return -1;
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
    if (scenario.hasBot(kVelocity[i])) {
      compare(kVelocity[i], scenario.velocity[i], actual.velocity[i], 1.0);
    }
  }

  compare("yaw_delta", 0.0, wrappedScenarioAngle(scenario.yaw, actual.yaw), 1.0);
  compare("pitch", scenario.pitch, actual.pitch, 1.0);
  compare("grounded", 1.0, actual.onGround ? 1.0 : 0.0);
  if (scenario.hasBot("crouching")) {
    compare("crouching", scenario.crouching ? 1.0 : 0.0,
      actual.crouching ? 1.0 : 0.0);
  }
  if (scenario.equipment.has("health")) {
  compare("health", scenario.equipment.health, actual.health, 0.5);
  }
  if (scenario.equipment.has("armor")) {
  compare("armor", scenario.equipment.armor, actual.armor, 0.5);
  }
  if (scenario.equipment.has("money")) {
  compare("money", scenario.equipment.money, actual.money);
  }
  if (scenario.equipment.has("defuse_kit")) {
    compare("defuse_kit", scenario.equipment.defuseKit ? 1.0 : 0.0,
      actual.defuseKit ? 1.0 : 0.0);
  }
  compare("speed_valid", 1.0,
    std::isfinite(actual.maxSpeed) && actual.maxSpeed >= 10.0f ? 1.0 : 0.0);

  if (scenario.equipment.has("primary")) {
    compareAmmo("primary_id", expectedPrimaryId, actual.primaryId);
  }
  if (scenario.equipment.has("secondary")) {
    compareAmmo("secondary_id", expectedSecondaryId, actual.secondaryId);
  }
  if (scenario.equipment.has("selected")) {
    compareAmmo("selected_id", expectedSelectedId, actual.selectedId);
  }

  if (expectedPrimaryId > 0 && actual.primaryId == expectedPrimaryId) {
    if (scenario.equipment.has("primary_clip")) {
      compareAmmo("primary_clip", scenario.equipment.primary.clip, actual.primaryClip);
    }
    if (scenario.equipment.has("primary_reserve")) {
      compareAmmo("primary_reserve", scenario.equipment.primary.reserve,
      actual.primaryReserve);
    }
  }

  if (expectedSecondaryId > 0 && actual.secondaryId == expectedSecondaryId) {
    if (scenario.equipment.has("secondary_clip")) {
      compareAmmo("secondary_clip", scenario.equipment.secondary.clip,
      actual.secondaryClip);
    }
    if (scenario.equipment.has("secondary_reserve")) {
      compareAmmo("secondary_reserve", scenario.equipment.secondary.reserve,
      actual.secondaryReserve);
    }
  }

  if (scenario.equipment.has("he")) {
    compareAmmo("he", scenario.equipment.he, actual.he);
  }
  if (scenario.equipment.has("flash")) {
    compareAmmo("flash", scenario.equipment.flash, actual.flash);
  }
  if (scenario.equipment.has("smoke")) {
    compareAmmo("smoke", scenario.equipment.smoke, actual.smoke);
  }

  if (scenario.equipment.has("helmet") && !actual.helmetKnown) {
    ++assessment.unverified;
    assessment.findings.push_back({"helmet", scenario.equipment.helmet ? 1.0 : 0.0,
      -1.0, true});
  }
  else if (scenario.equipment.has("helmet")) {
    compare("helmet", scenario.equipment.helmet ? 1.0 : 0.0,
      actual.helmet ? 1.0 : 0.0);
  }

  return assessment;
}

} // namespace aipb::explorer::scenario
