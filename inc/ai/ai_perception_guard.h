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

// Switch a still-visible target only for a meaningful distance advantage.
// Squared distances avoid square roots in the combat scan.
constexpr float kVisibleEnemySwitchDistanceRatio = 0.75f;
constexpr float kVisibleEnemySwitchDistanceRatioSq =
    kVisibleEnemySwitchDistanceRatio * kVisibleEnemySwitchDistanceRatio;

constexpr bool shouldKeepCurrentVisibleEnemy(bool currentVisible,
                                             bool currentIsVip, bool candidateIsVip,
                                             float currentDistanceSq, float candidateDistanceSq) {
  if (!currentVisible || currentDistanceSq < 0.0f || candidateDistanceSq < 0.0f) {
    return false;
  }
  // A newly visible priority objective must not be delayed by the threshold.
  if (currentIsVip != candidateIsVip) {
    return currentIsVip;
  }
  return candidateDistanceSq >= currentDistanceSq * kVisibleEnemySwitchDistanceRatioSq;
}

// A radio hold does not disable reacting to a nearby, recently heard enemy.
// The heard position is perception memory, not an unseen live entity query.
constexpr float kRadioHoldThreatFreshness = 3.0f;
constexpr float kRadioHoldThreatDistanceSq = 768.0f * 768.0f;

constexpr bool shouldReleaseRadioHoldForHeardThreat(
    bool hearingEnemy, float now, float heardAt, float heardDistanceSq) {
  return hearingEnemy && heardAt > 0.0f && now >= heardAt
      && now - heardAt <= kRadioHoldThreatFreshness
      && heardDistanceSq >= 0.0f
      && heardDistanceSq <= kRadioHoldThreatDistanceSq;
}

constexpr float kEnemyHearingExpiration = 10.0f;

// Expire sensory evidence regardless of the scan cadence. A sound heard on
// this tick refreshes lastHeardTime before the guard is evaluated.
constexpr bool hasExpiredEnemyHearing(float now, float lastHeardTime) {
  return lastHeardTime + kEnemyHearingExpiration < now;
}

constexpr float kGrenadeTargetFreshness = 3.0f;

constexpr bool suppressBlindFire(float blindTimeRemaining) {
  return blindTimeRemaining > 0.0f;
}

// Precise aiming must be disabled for every action and legacy task while the
// ScreenFade blind timer is active, not only while Task::Blind owns execution.
constexpr bool suppressPreciseBlindAim(float blindTimeRemaining) {
  return blindTimeRemaining > 0.0f;
}

// Grenades may use remembered enemy state only while that exact target still has
// fresh sensory support. Sticky global perception flags alone are insufficient.
constexpr bool hasFreshGrenadeTarget(bool visibleTarget, bool heardTargetMatches,
                                     float now, float lastSeenTime, float lastHeardTime) {
  const bool recentlySeen = lastSeenTime > 0.0f
      && lastSeenTime + kGrenadeTargetFreshness > now;
  const bool recentlyHeard = heardTargetMatches
      && lastHeardTime > 0.0f
      && lastHeardTime + kGrenadeTargetFreshness > now;

  return visibleTarget || recentlySeen || recentlyHeard;
}

} // namespace ai
