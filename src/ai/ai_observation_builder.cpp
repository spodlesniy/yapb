//
// YaPB - AI observation builder.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_observation_builder.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ai {
namespace {

Vec3 relativePosition (const Vec3 &origin, const Vec3 &reference) {
   return {
      origin.x - reference.x,
      origin.y - reference.y,
      origin.z - reference.z
   };
}

float length (const Vec3 &value) {
   return std::sqrt (value.x * value.x + value.y * value.y + value.z * value.z);
}

float finiteOrZero (float value) {
   return std::isfinite (value) ? value : 0.0f;
}

Vec3 finiteOrZero (const Vec3 &value) {
   return {
      finiteOrZero (value.x),
      finiteOrZero (value.y),
      finiteOrZero (value.z)
   };
}

} // namespace

Observation buildObservation (const ObservationInput &input) {
   Observation observation {};

   observation.gameTime = finiteOrZero (input.gameTime);
   observation.roundTimeRemaining = finiteOrZero (input.roundTimeRemaining);
   observation.personality = input.personality;

   observation.bot.origin = finiteOrZero (input.bot.origin);
   observation.bot.velocity = finiteOrZero (input.bot.velocity);
   observation.bot.health = finiteOrZero (input.bot.health);
   observation.bot.armor = finiteOrZero (input.bot.armor);
   observation.bot.maxSpeed = finiteOrZero (input.bot.maxSpeed);
   observation.bot.team = input.bot.team;
   observation.bot.difficulty = input.bot.difficulty;
   observation.bot.currentWeapon = input.bot.currentWeapon;
   observation.bot.currentNode = input.bot.currentNode;
   observation.bot.currentGoalNode = input.bot.currentGoalNode;
   observation.bot.objectiveFlags = input.bot.objectiveFlags;
   observation.bot.alive = input.bot.alive;
   observation.bot.hasC4 = input.bot.hasC4;
   observation.bot.hasHostage = input.bot.hasHostage;
   observation.bot.inBombZone = input.bot.inBombZone;
   observation.bot.inBuyZone = input.bot.inBuyZone;
   observation.bot.inRescueZone = input.bot.inRescueZone;

   const auto playerCount = std::min <size_t> (input.playerCount, kMaxObservedPlayers);
   observation.playerCount = static_cast <uint8_t> (playerCount);

   for (size_t i = 0; i < playerCount; ++i) {
      const auto &source = input.players[i];
      auto &target = observation.players[i];

      target.entityIndex = source.entityIndex;
      target.relativeOrigin = relativePosition (finiteOrZero (source.origin), finiteOrZero (input.bot.origin));
      target.distance = finiteOrZero (length (target.relativeOrigin));
      target.health = finiteOrZero (source.health);
      target.armor = finiteOrZero (source.armor);
      target.team = source.team;
      target.weapon = source.weapon;
      target.valid = source.valid;
      target.alive = source.alive;
      target.enemy = source.enemy;
      target.visible = source.visible;
      target.heard = source.heard;
   }

   const auto waypointCount = std::min <size_t> (input.waypointCount, kMaxObservedWaypoints);
   observation.waypointCount = static_cast <uint8_t> (waypointCount);

   for (size_t i = 0; i < waypointCount; ++i) {
      const auto &source = input.waypoints[i];
      auto &target = observation.waypoints[i];

      target.index = source.index;
      target.relativeOrigin = relativePosition (finiteOrZero (source.origin), finiteOrZero (input.bot.origin));
      target.distance = finiteOrZero (length (target.relativeOrigin));
      target.nodeFlags = source.nodeFlags;
      target.connectionFlags = source.connectionFlags;
      target.visible = source.visible;
   }

   return observation;
}

} // namespace ai
