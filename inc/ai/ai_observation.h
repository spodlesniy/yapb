//
// YaPB - AI observation abstraction.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ai {

constexpr size_t kMaxObservedPlayers = 16;
constexpr size_t kMaxObservedWaypoints = 8;

struct Vec3 {
   float x {};
   float y {};
   float z {};
};

struct PlayerState {
   int32_t entityIndex { -1 };
   Vec3 relativeOrigin {};
   float distance {};
   float health {};
   float armor {};

   int32_t team { -1 };
   int32_t weapon { -1 };

   bool valid {};
   bool alive {};
   bool enemy {};
   bool visible {};
   bool heard {};
};

struct WaypointState {
   int32_t index { -1 };
   Vec3 relativeOrigin {};
   float distance {};

   uint32_t nodeFlags {};
   uint16_t connectionFlags {};

   bool visible {};
};

struct Personality {
   // Values are normalized to [0, 1].
   float skill { 0.5f };
   float aggression { 0.5f };
   float risk { 0.5f };
   float teamwork { 0.5f };
   float objectiveFocus { 0.5f };
   float camping { 0.5f };
   float exploration { 0.5f };
};

struct BotState {
   Vec3 origin {};
   Vec3 velocity {};

   float health {};
   float armor {};
   float maxSpeed {};

   int32_t team { -1 };
   int32_t difficulty { -1 };
   int32_t currentWeapon { -1 };
   int32_t currentNode { -1 };
   int32_t currentGoalNode { -1 };

   uint32_t objectiveFlags {};

   bool alive {};
   bool hasC4 {};
   bool hasHostage {};
   bool inBombZone {};
   bool inBuyZone {};
   bool inRescueZone {};
};

struct Observation {
   // This is a semantic model of game state, not a serialized memory layout.
   // Training encoders should convert it explicitly into model features.
   float gameTime {};
   float roundTimeRemaining {};

   BotState bot {};
   Personality personality {};

   std::array <PlayerState, kMaxObservedPlayers> players {};
   std::array <WaypointState, kMaxObservedWaypoints> waypoints {};

   uint8_t playerCount {};
   uint8_t waypointCount {};
};

} // namespace ai
