//
// AiPB - AI action abstraction.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>

namespace ai {

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
  int32_t targetNode { -1 };
  int32_t targetPlayer { -1 };
  float confidence {};
};

} // namespace ai
