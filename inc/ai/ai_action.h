//
// YaPB - AI action abstraction.
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdint>

namespace ai {

CR_DECLARE_SCOPED_ENUM_TYPE (ActionType, uint8_t,
   None,
   SelectTargetNode,
   Attack,
   Retreat,
   HoldPosition,
   Push,
   Follow,
   SeekCover,
   PlantBomb,
   DefuseBomb,
   Hunt
)

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
