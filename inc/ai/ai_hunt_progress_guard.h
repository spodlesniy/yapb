//
// AiPB - remembered-enemy hunt progress guard.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

namespace ai {

constexpr float kHuntMeaningfulProgressDistance = 64.0f;
constexpr float kHuntNoProgressTimeout = 8.0f;

constexpr bool hasMeaningfulHuntProgress(float bestDistance, float currentDistance) {
  return bestDistance < 0.0f || currentDistance + kHuntMeaningfulProgressDistance <= bestDistance;
}

constexpr bool isHuntProgressStalled(float now, float lastProgressTime) {
  return lastProgressTime >= 0.0f && now >= lastProgressTime + kHuntNoProgressTimeout;
}

constexpr bool hasNewerHuntEvidence(float seenTime, float noiseEndTime,
                                    float huntSeenTime, float huntNoiseEndTime) {
  return seenTime > huntSeenTime || noiseEndTime > huntNoiseEndTime;
}

} // namespace ai
