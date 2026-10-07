//
// AiPB - planted-bomb defense node guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {

constexpr bool isBombDefenseNodeEligibleForPass(bool requireCamp, bool isCamp, bool isLadder) {
  return (!requireCamp || isCamp) && !isLadder;
}

constexpr float kDefuseSafetyMargin = 2.0f;
constexpr float kDefuseTimeWithKit = 7.0f;
constexpr float kDefuseTimeWithoutKit = 12.0f;

constexpr bool shouldPreemptActiveDefuseForVisibleEnemy(bool visibleEnemy, bool hasDefuser,
                                                        float bombTimeRemaining) {
  const float fullDefuseTime = hasDefuser ? kDefuseTimeWithKit : kDefuseTimeWithoutKit;
  return visibleEnemy && bombTimeRemaining > fullDefuseTime + kDefuseSafetyMargin;
}

} // namespace ai
