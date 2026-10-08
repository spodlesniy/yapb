//
// AiPB - bounded attack-on-move guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//
#pragma once

namespace ai {

// Continue closing distance in a gunfight only if the old medium-aggression
// branch would otherwise stop, and the target is visible at a safe distance.
// Retreat, reload, knife, sniper, VIP and narrow/duck states retain their
// existing combat movement handling.
constexpr bool shouldAdvanceWhileAttacking(int approach, bool seeingEnemy,
                                           bool reloading, bool usingSniper,
                                           bool vip, bool ducking, bool narrow,
                                           float enemyDistanceSq) {
  return approach >= 30 && approach < 50
      && seeingEnemy && !reloading && !usingSniper && !vip
      && !ducking && !narrow && enemyDistanceSq >= 384.0f * 384.0f;
}

} // namespace ai
