//
// AiPB - inference feature encoder.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <ai/ai_inference_features.h>

namespace ai {
namespace {

constexpr float kRoundTimeScale = 600.0f;
constexpr float kTaskTimeScale = 60.0f;
constexpr float kPositionScale = 4096.0f;
constexpr float kHealthScale = 100.0f;
constexpr float kSpeedScale = 320.0f;
constexpr float kAmmoScale = 100.0f;
constexpr float kBlindTimeScale = 10.0f;
constexpr float kFirePauseScale = 1.0f;
constexpr float kDistanceScale = kPositionScale;

float clampUnit(float value) {
  if (!std::isfinite(value)) {
    return 0.0f;
  }

  return std::clamp(value, 0.0f, 1.0f);
}

float normalizeSigned(float value, float scale) {
  if (!std::isfinite(value) || !std::isfinite(scale) || scale <= 0.0f) {
    return 0.0f;
  }

  return std::clamp(value / scale, -1.0f, 1.0f);
}

float normalizeNonNegative(float value, float scale) {
  if (!std::isfinite(value) || !std::isfinite(scale) || scale <= 0.0f) {
    return 0.0f;
  }

  return std::clamp(value / scale, 0.0f, 1.0f);
}

float booleanFeature(bool value) {
  return value ? 1.0f : 0.0f;
}

float enumOneHot(uint8_t value, uint8_t slot) {
  return value == slot ? 1.0f : 0.0f;
}

bool bitSet(uint32_t value, uint32_t bit) {
  return (value & (1u << bit)) != 0;
}

template <typename Enum> size_t enumValue(Enum value) {
  return static_cast<size_t>(value);
}

void writeOneHot(float *values, size_t base, uint8_t selected, size_t count) {

  for (size_t i = 0; i < count; ++i) {
    values[base + i] = enumOneHot(selected, static_cast<uint8_t>(i));
  }
}

Vec3 relative(const Vec3 &position, const Vec3 &origin) {
  return {
    position.x - origin.x,
    position.y - origin.y,
    position.z - origin.z,
  };
}

} // namespace

InferenceFeatures encodeInferenceFeatures(const Observation &observation) {
  InferenceFeatures result {};
  auto &values = result.values;
  const auto origin = observation.bot.origin;

  const auto set = [&](InferenceFeature::Core feature, float value) { values[enumValue(feature)] = value; };

  set(InferenceFeature::Core::RoundTimeRemaining, normalizeNonNegative(observation.roundTimeRemaining, kRoundTimeScale));
  set(InferenceFeature::Core::TaskTimeRemaining, normalizeNonNegative(observation.bot.taskTimeRemaining, kTaskTimeScale));
  set(InferenceFeature::Core::Health, normalizeNonNegative(observation.bot.health, kHealthScale));
  set(InferenceFeature::Core::Armor, normalizeNonNegative(observation.bot.armor, kHealthScale));
  set(InferenceFeature::Core::MaxSpeed, normalizeNonNegative(observation.bot.maxSpeed, kSpeedScale));
  set(InferenceFeature::Core::VelocityX, normalizeSigned(observation.bot.velocity.x, kSpeedScale));
  set(InferenceFeature::Core::VelocityY, normalizeSigned(observation.bot.velocity.y, kSpeedScale));
  set(InferenceFeature::Core::VelocityZ, normalizeSigned(observation.bot.velocity.z, kSpeedScale));
  set(InferenceFeature::Core::DesiredVelocityX, normalizeSigned(observation.bot.desiredVelocity.x, kSpeedScale));
  set(InferenceFeature::Core::DesiredVelocityY, normalizeSigned(observation.bot.desiredVelocity.y, kSpeedScale));
  set(InferenceFeature::Core::DesiredVelocityZ, normalizeSigned(observation.bot.desiredVelocity.z, kSpeedScale));

  const auto destination = relative(observation.bot.destination, origin);
  set(InferenceFeature::Core::DestinationRelativeX, normalizeSigned(destination.x, kPositionScale));
  set(InferenceFeature::Core::DestinationRelativeY, normalizeSigned(destination.y, kPositionScale));
  set(InferenceFeature::Core::DestinationRelativeZ, normalizeSigned(destination.z, kPositionScale));

  set(InferenceFeature::Core::Alive, booleanFeature(observation.bot.alive));
  set(InferenceFeature::Core::MovingToGoal, booleanFeature(observation.bot.movingToGoal));
  set(InferenceFeature::Core::Stuck, booleanFeature(observation.bot.stuck));
  set(InferenceFeature::Core::Difficulty, normalizeNonNegative(static_cast<float>(observation.bot.difficulty), 4.0f));
  set(InferenceFeature::Core::Skill, clampUnit(observation.personality.skill));
  set(InferenceFeature::Core::Aggression, clampUnit(observation.personality.aggression));
  set(InferenceFeature::Core::Risk, clampUnit(observation.personality.risk));
  set(InferenceFeature::Core::Teamwork, clampUnit(observation.personality.teamwork));
  set(InferenceFeature::Core::ObjectiveFocus, clampUnit(observation.personality.objectiveFocus));
  set(InferenceFeature::Core::Camping, clampUnit(observation.personality.camping));
  set(InferenceFeature::Core::Exploration, clampUnit(observation.personality.exploration));

  set(InferenceFeature::Core::AmmoInClip, normalizeNonNegative(static_cast<float>(observation.combat.ammoInClip), kAmmoScale));
  set(InferenceFeature::Core::Blind, booleanFeature(observation.combat.blind));
  set(InferenceFeature::Core::BlindTimeRemaining, normalizeNonNegative(observation.combat.blindTimeRemaining, kBlindTimeScale));
  set(InferenceFeature::Core::FirePauseRemaining, normalizeNonNegative(observation.combat.firePauseRemaining, kFirePauseScale));
  set(InferenceFeature::Core::EnemyDistance, normalizeNonNegative(observation.combat.enemyDistance, kDistanceScale));
  set(InferenceFeature::Core::LastEnemyDistance, normalizeNonNegative(observation.combat.lastEnemyDistance, kDistanceScale));

  const auto weaponBase = enumValue(InferenceFeature::Core::WeaponBase);
  writeOneHot(values, weaponBase, static_cast<uint8_t>(observation.combat.weaponType), 10);

  const auto reloadBase = enumValue(InferenceFeature::Core::ReloadBase);
  writeOneHot(values, reloadBase, static_cast<uint8_t>(observation.combat.reloadState), 3);

  const auto objectiveBase = enumValue(InferenceFeature::Core::ObjectiveBase);
  for (size_t i = 0; i < 7; ++i) {
    values[objectiveBase + i] = booleanFeature(bitSet(observation.bot.objectiveFlags, static_cast<uint32_t>(i)));
  }

  const auto navigationBase = enumValue(InferenceFeature::Core::NavigationBase);
  for (size_t i = 0; i < 4; ++i) {
    values[navigationBase + i] = booleanFeature(bitSet(observation.bot.navigationFlags, static_cast<uint32_t>(i)));
  }

  const auto perceptionBase = enumValue(InferenceFeature::Core::PerceptionBase);
  for (size_t i = 0; i < 4; ++i) {
    values[perceptionBase + i] = booleanFeature(bitSet(observation.combat.perceptionFlags, static_cast<uint32_t>(i)));
  }

  const auto taskBase = enumValue(InferenceFeature::Core::TaskBase);
  writeOneHot(values, taskBase, static_cast<uint8_t>(observation.bot.currentTask), 21);

  size_t playerIndices[kInferencePlayerSlots] {};
  for (size_t i = 0; i < kInferencePlayerSlots; ++i) {
    playerIndices[i] = i < observation.playerCount ? i : kMaxObservedPlayers;
  }

  std::sort(playerIndices, playerIndices + kInferencePlayerSlots, [&](size_t left, size_t right) {
    if (left == kMaxObservedPlayers || right == kMaxObservedPlayers) {
      return left != kMaxObservedPlayers;
    }

    const auto &a = observation.players[left];
    const auto &b = observation.players[right];

    if (a.valid != b.valid) {
      return a.valid > b.valid;
    }

    if (a.distance != b.distance) {
      return a.distance < b.distance;
    }

    return a.entityIndex < b.entityIndex;
  });

  for (size_t slot = 0; slot < kInferencePlayerSlots; ++slot) {
    const auto base = kCoreFeatureCount + slot * kPlayerFeatureCount;
    const auto sourceIndex = playerIndices[slot];

    if (sourceIndex == kMaxObservedPlayers) {
      continue;
    }

    const auto &player = observation.players[sourceIndex];
    values[base + enumValue(InferenceFeature::Player::Valid)] = booleanFeature(player.valid);
    values[base + enumValue(InferenceFeature::Player::Alive)] = booleanFeature(player.alive);
    values[base + enumValue(InferenceFeature::Player::Enemy)] = booleanFeature(player.enemy);
    values[base + enumValue(InferenceFeature::Player::Visible)] = booleanFeature(player.visible);
    values[base + enumValue(InferenceFeature::Player::Heard)] = booleanFeature(player.heard);
    values[base + enumValue(InferenceFeature::Player::RelativeX)] = normalizeSigned(player.relativeOrigin.x, kPositionScale);
    values[base + enumValue(InferenceFeature::Player::RelativeY)] = normalizeSigned(player.relativeOrigin.y, kPositionScale);
    values[base + enumValue(InferenceFeature::Player::RelativeZ)] = normalizeSigned(player.relativeOrigin.z, kPositionScale);
    values[base + enumValue(InferenceFeature::Player::Distance)] = normalizeNonNegative(player.distance, kDistanceScale);
    values[base + enumValue(InferenceFeature::Player::Health)] = normalizeNonNegative(player.health, kHealthScale);
    values[base + enumValue(InferenceFeature::Player::Armor)] = normalizeNonNegative(player.armor, kHealthScale);
  }

  const auto waypointBase = kCoreFeatureCount + kInferencePlayerSlots * kPlayerFeatureCount;
  size_t waypointIndices[kInferenceWaypointSlots] {};
  for (size_t i = 0; i < kInferenceWaypointSlots; ++i) {
    waypointIndices[i] = i < observation.waypointCount ? i : kMaxObservedWaypoints;
  }

  std::sort(waypointIndices, waypointIndices + kInferenceWaypointSlots, [&](size_t left, size_t right) {
    if (left == kMaxObservedWaypoints || right == kMaxObservedWaypoints) {
      return left != kMaxObservedWaypoints;
    }

    const auto &a = observation.waypoints[left];
    const auto &b = observation.waypoints[right];

    if (a.visible != b.visible) {
      return a.visible > b.visible;
    }

    if (a.distance != b.distance) {
      return a.distance < b.distance;
    }

    return a.index < b.index;
  });

  for (size_t slot = 0; slot < kInferenceWaypointSlots; ++slot) {
    const auto base = waypointBase + slot * kWaypointFeatureCount;
    const auto sourceIndex = waypointIndices[slot];

    if (sourceIndex == kMaxObservedWaypoints) {
      continue;
    }

    const auto &waypoint = observation.waypoints[sourceIndex];
    const auto present = waypoint.index >= 0;
    values[base + enumValue(InferenceFeature::Waypoint::Present)] = booleanFeature(present);
    values[base + enumValue(InferenceFeature::Waypoint::Visible)] = booleanFeature(waypoint.visible);
    values[base + enumValue(InferenceFeature::Waypoint::IsCurrent)] = booleanFeature(waypoint.index == observation.bot.currentNode);
    values[base + enumValue(InferenceFeature::Waypoint::IsGoal)] = booleanFeature(waypoint.index == observation.bot.currentGoalNode);
    values[base + enumValue(InferenceFeature::Waypoint::RelativeX)] = normalizeSigned(waypoint.relativeOrigin.x, kPositionScale);
    values[base + enumValue(InferenceFeature::Waypoint::RelativeY)] = normalizeSigned(waypoint.relativeOrigin.y, kPositionScale);
    values[base + enumValue(InferenceFeature::Waypoint::RelativeZ)] = normalizeSigned(waypoint.relativeOrigin.z, kPositionScale);
    values[base + enumValue(InferenceFeature::Waypoint::Distance)] = normalizeNonNegative(waypoint.distance, kDistanceScale);
  }

  return result;
}

} // namespace ai
