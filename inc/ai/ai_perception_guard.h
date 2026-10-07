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

} // namespace ai
