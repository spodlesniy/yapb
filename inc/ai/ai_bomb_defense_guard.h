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

} // namespace ai
