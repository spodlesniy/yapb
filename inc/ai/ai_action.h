//
// AiPB - AI action abstraction.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_observation.h>

#include <cstdint>

namespace ai {

enum class GrenadeType : uint8_t {
  None,
  HE,
  Flashbang,
  Smoke,
};

enum class ActionType : uint8_t {
  None,

  // Navigation.
  MoveToNode,
  MoveToPosition,
  FollowPlayer,
  SeekCover,
  Retreat,
  HoldPosition,
  Explore,

  // Combat.
  AttackTarget,
  HuntTarget,
  AimAtTarget,
  Fire,
  Reload,
  ChangeWeapon,

  // Objectives.
  PlantBomb,
  DefuseBomb,
  PickupItem,
  EscapeFromBomb,
  RescueHostage,
  ProtectObjective,

  // Tactical / utility.
  Wait,
  Camp,
  ThrowGrenade,
  ThrowFlashbang,
  ThrowSmoke,

  Count,
};

// High-level intent returned by an AI policy.
//
// The action deliberately does not contain GoldSrc-specific input such as
// IN_ATTACK or player angles. A later execution layer will translate this
// intent into the existing YaPB navigation and movement systems.
struct Action {
  ActionType type { ActionType::None };

  // Candidate target data. Target semantics are defined separately from the
  // action payload and will be validated by the execution layer.
  int32_t targetNode { -1 };
  int32_t targetPlayer { -1 };
  Vec3 targetPosition {};

  // Optional action-specific parameters.
  WeaponType weaponType { WeaponType::Unknown };
  GrenadeType grenadeType { GrenadeType::None };
  float duration {};
  float confidence {};
};

} // namespace ai
