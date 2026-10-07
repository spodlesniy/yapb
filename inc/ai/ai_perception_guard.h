//
// AiPB - enemy perception state guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {

// Live entity state is valid only for the currently confirmed visible enemy.
// Suspected targets may retain an entity pointer while remaining hidden.
constexpr bool canUseLiveEnemyState(bool seeingEnemy, bool suspectEnemy, bool currentEnemyMatchesLastEnemy) {
  return seeingEnemy && !suspectEnemy && currentEnemyMatchesLastEnemy;
}

// Enemy slot state may expose exact live values only while that enemy is
// actually visible. Teammate state remains available through normal team data.
constexpr bool canExposePlayerState(bool enemy, bool visible) {
  return !enemy || visible;
}

// A newly heard enemy may replace older memory only when the sound event itself
// is nearer and recent visual contact does not still own that memory.
constexpr bool shouldReplaceRememberedEnemyWithHeard(float rememberedDistanceSq, float heardDistanceSq,
                                                      bool recentlySeen) {
  return !recentlySeen && heardDistanceSq < rememberedDistanceSq;
}

} // namespace ai
