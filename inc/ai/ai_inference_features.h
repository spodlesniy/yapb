//
// AiPB - inference feature schema.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_observation.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace ai {

constexpr uint32_t kInferenceFeatureSchemaVersion = 1;
constexpr size_t kInferencePlayerSlots = 8;
constexpr size_t kInferenceWaypointSlots = 8;

namespace InferenceFeature {

enum class Core : size_t {
  RoundTimeRemaining = 0,
  Health,
  Armor,
  MaxSpeed,
  VelocityX,
  VelocityY,
  VelocityZ,
  DesiredVelocityX,
  DesiredVelocityY,
  DesiredVelocityZ,
  DestinationRelativeX,
  DestinationRelativeY,
  DestinationRelativeZ,
  Alive,
  MovingToGoal,
  Stuck,
  Difficulty,
  Skill,
  Aggression,
  Risk,
  Teamwork,
  ObjectiveFocus,
  Camping,
  Exploration,
  AmmoInClip,
  Blind,
  BlindTimeRemaining,
  FirePauseRemaining,
  EnemyDistance,
  LastEnemyDistance,
  WeaponBase = 30,
  ReloadBase = WeaponBase + 10,
  ObjectiveBase = ReloadBase + 3,
  NavigationBase = ObjectiveBase + 7,
  PerceptionBase = NavigationBase + 4,
  TaskBase = PerceptionBase + 4,
  Count = TaskBase + 20,
};

enum class Player : size_t {
  Valid = 0,
  Alive,
  Enemy,
  Visible,
  Heard,
  RelativeX,
  RelativeY,
  RelativeZ,
  Distance,
  Health,
  Armor,
  Count,
};

enum class Waypoint : size_t {
  Present = 0,
  Visible,
  IsCurrent,
  IsGoal,
  RelativeX,
  RelativeY,
  RelativeZ,
  Distance,
  Count,
};

} // namespace InferenceFeature

constexpr size_t kCoreFeatureCount = static_cast<size_t>(InferenceFeature::Core::Count);
constexpr size_t kPlayerFeatureCount = static_cast<size_t>(InferenceFeature::Player::Count);
constexpr size_t kWaypointFeatureCount = static_cast<size_t>(InferenceFeature::Waypoint::Count);

constexpr size_t kInferenceFeatureCount =
    kCoreFeatureCount + kInferencePlayerSlots * kPlayerFeatureCount + kInferenceWaypointSlots * kWaypointFeatureCount;

struct InferenceFeatures {
  uint32_t schemaVersion { kInferenceFeatureSchemaVersion };
  std::array<float, kInferenceFeatureCount> values {};

  bool hasSupportedSchema() const {
    return schemaVersion == kInferenceFeatureSchemaVersion;
  }

  float at(size_t index) const {
    return values[index];
  }
};

InferenceFeatures encodeInferenceFeatures(const Observation &observation);

} // namespace ai
