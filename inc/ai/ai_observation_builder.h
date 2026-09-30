//
// AiPB - AI observation builder.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_observation.h>

#include <cstdint>

namespace ai {

// Game-facing snapshot. This type intentionally contains no GoldSrc types so
// it can be produced by a thin runtime adapter and fully unit-tested in isolation.
struct BotInput {
  Vec3 origin {};
  Vec3 velocity {};
  Vec3 destination {};
  Vec3 desiredVelocity {};

  float health {};
  float armor {};
  float maxSpeed {};

  int32_t team { -1 };
  int32_t difficulty { -1 };
  int32_t currentWeapon { -1 };
  int32_t currentNode { -1 };
  int32_t currentGoalNode { -1 };
  TaskType currentTask { TaskType::Unknown };

  uint32_t objectiveFlags {};
  uint32_t navigationFlags {};

  bool alive {};
  bool movingToGoal {};
  bool stuck {};
  bool hasC4 {};
  bool hasHostage {};
  bool inBombZone {};
  bool inBuyZone {};
  bool inRescueZone {};
};

struct CombatInput {
  int32_t enemyEntity { -1 };
  int32_t lastEnemyEntity { -1 };
  Vec3 enemyOrigin {};
  Vec3 lastEnemyOrigin {};
  uint32_t perceptionFlags {};
};

struct PlayerInput {
  int32_t entityIndex { -1 };
  Vec3 origin {};

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

struct WaypointInput {
  int32_t index { -1 };
  Vec3 origin {};

  uint32_t nodeFlags {};
  uint16_t connectionFlags {};

  bool visible {};
};

struct ObservationInput {
  float gameTime {};
  float roundTimeRemaining {};

  BotInput bot {};
  CombatInput combat {};
  Personality personality {};

  PlayerInput players[kMaxObservedPlayers] {};
  WaypointInput waypoints[kMaxObservedWaypoints] {};

  uint8_t playerCount {};
  uint8_t waypointCount {};
};

// Converts a game-facing snapshot into the stable, engine-independent AI
// observation. Player and waypoint positions become relative to the bot.
Observation buildObservation(const ObservationInput &input);

} // namespace ai
