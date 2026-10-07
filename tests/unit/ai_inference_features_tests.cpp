//
// AiPB - inference feature encoder unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_inference_features.h>

#include <cmath>

using ai::test::expect;
using ai::test::expectNear;

AI_TEST(testInferenceFeatureSchema) {
  expect(ai::kInferenceFeatureSchemaVersion == 7, "inference feature schema uses the current version");
  expect(ai::kInferenceFeatureCount == 252, "inference feature vector uses the current width");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::LastEnemyDistance) == 31,
         "last enemy distance has its own core index");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::TeamBase) == 37, "team block starts at the expected index");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::WeaponBase) == 39,
         "weapon block no longer overlaps last enemy distance");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::ReloadBase) == 49, "reload block follows weapons");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::ObjectiveBase) == 52, "objective block follows reload");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::TaskBase) == 68, "task block starts at the expected index");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::ThrowTargetRelativeX) == 89, "throw target x follows task block");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::ThrowTargetRelativeY) == 90, "throw target y follows x");
  expect(static_cast<size_t>(ai::InferenceFeature::Core::ThrowTargetRelativeZ) == 91, "throw target z follows y");

  const ai::InferenceFeatures features {};
  expect(features.schemaVersion == ai::kInferenceFeatureSchemaVersion, "feature vector defaults to current schema");
  expect(features.hasSupportedSchema(), "feature vector reports supported schema");
}

AI_TEST(testInferenceFeatureEncoding) {
  ai::Observation observation {};
  observation.bot.origin = { 100.0f, 200.0f, 300.0f };
  observation.bot.destination = { 1124.0f, 200.0f, 300.0f };
  observation.bot.throwTarget = { 1324.0f, 300.0f, 300.0f };
  observation.bot.health = 50.0f;
  observation.bot.armor = 25.0f;
  observation.bot.maxSpeed = 320.0f;
  observation.bot.team = 1;
  observation.bot.alive = true;
  observation.bot.hasDefuser = true;
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombDropped;
  observation.bot.droppedBombRelativeOrigin = { 512.0f, 0.0f, 0.0f };
  observation.bot.droppedBombDistance = 512.0f;
  observation.bombTimeRemaining = 20.0f;
  observation.bot.currentNode = 10;
  observation.bot.currentGoalNode = 20;
  observation.bot.currentTask = ai::TaskType::MoveToPosition;
  observation.bot.taskTimeRemaining = 30.0f;
  observation.personality.aggression = 0.8f;
  observation.combat.ammoInClip = 30;
  observation.combat.weaponType = ai::WeaponType::Rifle;
  observation.combat.lastEnemyDistance = 2048.0f;

  observation.playerCount = 2;
  observation.players[0].entityIndex = 8;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  observation.players[0].relativeOrigin = { 100.0f, 0.0f, 0.0f };
  observation.players[0].distance = 100.0f;
  observation.players[0].health = 100.0f;
  observation.players[0].armor = 50.0f;

  observation.players[1].entityIndex = 7;
  observation.players[1].valid = true;
  observation.players[1].alive = true;
  observation.players[1].enemy = false;
  observation.players[1].relativeOrigin = { 50.0f, 0.0f, 0.0f };
  observation.players[1].distance = 50.0f;
  observation.players[1].health = 75.0f;
  observation.players[1].armor = 25.0f;
  observation.players[1].isFollowTarget = true;

  observation.waypointCount = 2;
  observation.waypoints[0].index = 20;
  observation.waypoints[0].relativeOrigin = { 200.0f, 0.0f, 0.0f };
  observation.waypoints[0].distance = 200.0f;
  observation.waypoints[0].visible = false;
  observation.waypoints[1].index = 10;
  observation.waypoints[1].relativeOrigin = { 100.0f, 0.0f, 0.0f };
  observation.waypoints[1].distance = 100.0f;
  observation.waypoints[1].visible = true;

  const auto features = ai::encodeInferenceFeatures(observation);
  const auto core = [](ai::InferenceFeature::Core feature) { return static_cast<size_t>(feature); };
  const auto playerBase = [](size_t slot) { return ai::kCoreFeatureCount + slot * ai::kPlayerFeatureCount; };
  const auto waypointBase = [](size_t slot) {
    return ai::kCoreFeatureCount + ai::kInferencePlayerSlots * ai::kPlayerFeatureCount + slot * ai::kWaypointFeatureCount;
  };

  expectNear(features.at(core(ai::InferenceFeature::Core::BombTimeRemaining)), 20.0f / 60.0f, 0.0001f,
             "bomb time remaining is normalized");
  expectNear(features.at(core(ai::InferenceFeature::Core::TaskTimeRemaining)), 0.5f, 0.0001f, "task time remaining is normalized");
  expectNear(features.at(core(ai::InferenceFeature::Core::Health)), 0.5f, 0.0001f, "health is normalized");
  expectNear(features.at(core(ai::InferenceFeature::Core::ThrowTargetRelativeX)), 1224.0f / 4096.0f, 0.0001f,
             "throw target x is encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::ThrowTargetRelativeY)), 100.0f / 4096.0f, 0.0001f,
             "throw target y is encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::ThrowTargetRelativeZ)), 0.0f, 0.0001f,
             "throw target z is encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::DestinationRelativeX)), 0.25f, 0.0001f,
             "destination is encoded relative to bot origin");
  expectNear(features.at(core(ai::InferenceFeature::Core::Aggression)), 0.8f, 0.0001f, "personality is preserved");
  expectNear(features.at(core(ai::InferenceFeature::Core::Alive)), 1.0f, 0.0001f, "boolean state is encoded as one");
  expectNear(features.at(core(ai::InferenceFeature::Core::LastEnemyDistance)), 0.5f, 0.0001f,
             "last enemy distance is preserved independently from the weapon block");
  expectNear(features.at(core(ai::InferenceFeature::Core::HasDefuser)), 1.0f, 0.0001f, "defuser state is encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::DroppedBombRelativeX)), 512.0f / 4096.0f, 0.0001f,
             "dropped bomb position is encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::DroppedBombDistance)), 512.0f / 4096.0f, 0.0001f,
             "dropped bomb distance is encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::TeamBase)), 0.0f, 0.0001f,
             "terrorist team slot is clear for a CT");
  expectNear(features.at(core(ai::InferenceFeature::Core::TeamBase) + 1), 1.0f, 0.0001f,
             "counter-terrorist team slot is encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::WeaponBase) + static_cast<size_t>(ai::WeaponType::Rifle)), 1.0f, 0.0001f,
             "weapon type is one-hot encoded");
  expectNear(features.at(core(ai::InferenceFeature::Core::ObjectiveBase) + 7), 1.0f, 0.0001f,
             "dropped bomb objective flag is encoded");

  expectNear(features.at(playerBase(0) + static_cast<size_t>(ai::InferenceFeature::Player::IsFollowTarget)), 1.0f, 0.0001f, "follow target is encoded");
  expectNear(features.at(playerBase(0) + static_cast<size_t>(ai::InferenceFeature::Player::Distance)), 50.0f / 4096.0f, 0.0001f,
             "nearest player occupies the first player slot");
  expectNear(features.at(playerBase(0) + static_cast<size_t>(ai::InferenceFeature::Player::Enemy)), 0.0f, 0.0001f,
             "nearest friendly player is encoded correctly");

  expectNear(features.at(waypointBase(0) + static_cast<size_t>(ai::InferenceFeature::Waypoint::Distance)), 100.0f / 4096.0f, 0.0001f,
             "nearest visible waypoint occupies the first waypoint slot");
  expectNear(features.at(waypointBase(0) + static_cast<size_t>(ai::InferenceFeature::Waypoint::IsCurrent)), 1.0f, 0.0001f,
             "current waypoint is encoded explicitly");
  expectNear(features.at(waypointBase(0) + static_cast<size_t>(ai::InferenceFeature::Waypoint::IsGoal)), 0.0f, 0.0001f,
             "goal waypoint flag is encoded explicitly");

  observation.bot.currentTask = ai::TaskType::Spraypaint;
  const auto spraypaintFeatures = ai::encodeInferenceFeatures(observation);
  expectNear(spraypaintFeatures.at(core(ai::InferenceFeature::Core::TaskBase) + 20), 1.0f, 0.0001f,
             "spraypaint task is encoded in the new task slot");
}

AI_TEST(testInferenceFeatureSanitizesInvalidValues) {
  ai::Observation observation {};
  observation.bot.health = std::numeric_limits<float>::quiet_NaN();
  observation.bot.velocity.x = std::numeric_limits<float>::infinity();
  observation.bot.destination.y = std::numeric_limits<float>::infinity();
  observation.personality.skill = 4.0f;
  observation.combat.enemyDistance = -10.0f;

  const auto features = ai::encodeInferenceFeatures(observation);

  for (const auto value : features.values) {
    expect(std::isfinite(value), "feature vector contains only finite values");
    expect(value >= -1.0f && value <= 1.0f, "feature vector values stay within model bounds");
  }

  expectNear(features.at(static_cast<size_t>(ai::InferenceFeature::Core::Skill)), 1.0f, 0.0001f, "personality value is clamped");
  expectNear(features.at(static_cast<size_t>(ai::InferenceFeature::Core::EnemyDistance)), 0.0f, 0.0001f, "negative distance is clamped");
}

AI_TEST(testInferenceFeatureOrdering) {
  ai::Observation observation {};
  observation.playerCount = 2;
  observation.players[0].entityIndex = 9;
  observation.players[0].valid = true;
  observation.players[0].distance = 200.0f;
  observation.players[0].relativeOrigin.x = 200.0f;
  observation.players[1].entityIndex = 3;
  observation.players[1].valid = true;
  observation.players[1].distance = 100.0f;
  observation.players[1].relativeOrigin.x = 100.0f;

  const auto features = ai::encodeInferenceFeatures(observation);
  const auto base = ai::kCoreFeatureCount;

  expectNear(features.at(base + static_cast<size_t>(ai::InferenceFeature::Player::Distance)), 100.0f / 4096.0f, 0.0001f,
             "player slots are ordered by distance");
  expectNear(features.at(base + ai::kPlayerFeatureCount + static_cast<size_t>(ai::InferenceFeature::Player::Distance)), 200.0f / 4096.0f,
             0.0001f, "second player slot follows deterministic ordering");
}

AI_TEST(testInferenceFeatureMasksAndDeprioritizesHiddenEnemyState) {
  ai::Observation observation {};
  observation.playerCount = 2;

  observation.players[0].entityIndex = 3;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  observation.players[0].heard = true;
  observation.players[0].relativeOrigin = { 10.0f, 20.0f, 30.0f };
  observation.players[0].distance = 40.0f;
  observation.players[0].health = 100.0f;
  observation.players[0].armor = 100.0f;

  observation.players[1].entityIndex = 9;
  observation.players[1].valid = true;
  observation.players[1].alive = true;
  observation.players[1].enemy = true;
  observation.players[1].visible = true;
  observation.players[1].relativeOrigin = { 800.0f, 0.0f, 0.0f };
  observation.players[1].distance = 800.0f;
  observation.players[1].health = 75.0f;
  observation.players[1].armor = 25.0f;

  const auto features = ai::encodeInferenceFeatures(observation);
  const auto first = ai::kCoreFeatureCount;
  const auto second = first + ai::kPlayerFeatureCount;

  expectNear(features.at(first + static_cast<size_t>(ai::InferenceFeature::Player::Visible)), 1.0f, 0.0001f,
             "visible enemy is ordered before a hidden enemy even when farther away");
  expectNear(features.at(first + static_cast<size_t>(ai::InferenceFeature::Player::RelativeX)), 800.0f / 4096.0f, 0.0001f,
             "visible enemy position remains encoded");

  expectNear(features.at(second + static_cast<size_t>(ai::InferenceFeature::Player::Heard)), 1.0f, 0.0001f,
             "hidden enemy hearing flag remains encoded");
  expectNear(features.at(second + static_cast<size_t>(ai::InferenceFeature::Player::RelativeX)), 0.0f, 0.0001f,
             "hidden enemy position is masked at the feature boundary");
  expectNear(features.at(second + static_cast<size_t>(ai::InferenceFeature::Player::Distance)), 0.0f, 0.0001f,
             "hidden enemy distance is masked at the feature boundary");
  expectNear(features.at(second + static_cast<size_t>(ai::InferenceFeature::Player::Health)), 0.0f, 0.0001f,
             "hidden enemy health is masked at the feature boundary");
  expectNear(features.at(second + static_cast<size_t>(ai::InferenceFeature::Player::Armor)), 0.0f, 0.0001f,
             "hidden enemy armor is masked at the feature boundary");
}
