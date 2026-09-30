//
// AiPB - AI observation abstraction.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <cstdint>

namespace ai {

constexpr size_t kMaxObservedPlayers = 16;
constexpr size_t kMaxObservedWaypoints = 8;

enum class TaskType : uint8_t {
  Unknown,
  Normal,
  Pause,
  MoveToPosition,
  FollowUser,
  PickupItem,
  Camp,
  PlantBomb,
  DefuseBomb,
  Attack,
  Hunt,
  SeekCover,
  ThrowExplosive,
  ThrowFlashbang,
  ThrowSmoke,
  DoubleJump,
  EscapeFromBomb,
  ShootBreakable,
  Hide,
  Blind,
  Spraypaint,
};

namespace ObjectiveFlag {
constexpr uint32_t BombPlanted = 1u << 0;
constexpr uint32_t BombCarrier = 1u << 1;
constexpr uint32_t HasHostage = 1u << 2;
constexpr uint32_t InBombZone = 1u << 3;
constexpr uint32_t InRescueZone = 1u << 4;
constexpr uint32_t InEscapeZone = 1u << 5;
constexpr uint32_t InVIPZone = 1u << 6;
}

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
  TaskType currentTask { TaskType::Unknown };

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

  PlayerState players[kMaxObservedPlayers] {};
  WaypointState waypoints[kMaxObservedWaypoints] {};

  uint8_t playerCount {};
  uint8_t waypointCount {};
};

} // namespace ai
