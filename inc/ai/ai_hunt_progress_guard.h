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
constexpr float kHuntUnreachableGraphDistance = 32767.0f;

// Only connected shortest-path distances can prove progress towards a remembered
// target. Never refresh the stall watchdog from a straight-line approximation.
constexpr bool isUsableHuntGraphDistance(float distance) {
  return distance >= 0.0f && distance < kHuntUnreachableGraphDistance;
}

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

// A hunt that already searched its remembered target must not be restarted
// by the SAME sight/sound snapshot. New target-specific sensory evidence may
// authorize another hunt immediately; each round starts with a clean budget.
constexpr bool suppressConsumedHuntWithoutNewEvidence(
    int targetPlayer, int consumedTargetPlayer,
    float roundStartTime, float consumedRoundStartTime,
    float seenTime, float noiseEndTime,
    float consumedSeenTime, float consumedNoiseEndTime) {
  return targetPlayer > 0 && targetPlayer == consumedTargetPlayer
      && roundStartTime == consumedRoundStartTime
      && !hasNewerHuntEvidence(seenTime, noiseEndTime, consumedSeenTime, consumedNoiseEndTime);
}

constexpr float kHuntTargetReanchorDistance = 64.0f;

// A new target position must be backed by strictly newer sensory evidence.
// Tiny shifts (e.g. hearing localization noise) must not reset the path or stall clock.
constexpr bool shouldReanchorHuntTarget(bool newerEvidence, float positionChangeSq) {
  return newerEvidence && positionChangeSq >= kHuntTargetReanchorDistance * kHuntTargetReanchorDistance;
}

} // namespace ai
