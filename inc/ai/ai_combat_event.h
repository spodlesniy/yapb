//
// AiPB - lightweight combat telemetry (not model features).
// SPDX-License-Identifier: MIT
//
#pragma once

#include <cstdint>
#include <ai/ai_observation.h>
#include <ai/ai_action.h>

namespace ai {

enum class CombatEventType : uint8_t {
  FlashStart, FlashEnd, WeaponFire, Damage, Kill,
};

// Missing or unknowable values are -1, never fabricated. All events are
// timestamped at capture time; an ammo decrement is only evidence of fire,
// not an authoritative engine shot callback.
struct CombatEvent {
  CombatEventType type { CombatEventType::WeaponFire };
  float gameTime {};
  float roundStartTime {};
  uint32_t roundId {};
  uint64_t episodeId {};
  int32_t botId { -1 }, team { -1 }, task { -1 }, aiAction { -1 };
  Vec3 position {}, aimDirection {};
  int32_t weaponId { -1 }, targetId { -1 }, attackerId { -1 }, victimId { -1 };
  int32_t sourceEntityId { -1 }, ammoBefore { -1 }, ammoAfter { -1 };
  int32_t healthDamage { -1 }, armorDamage { -1 }, flashAlpha { -1 };
  float blindTimeRemaining {};
  bool attackPressed {}, enemyVisible {}, enemyHeard {};
};

constexpr const char *combatEventName(CombatEventType type) {
  switch (type) {
  case CombatEventType::FlashStart: return "flash_blind_start";
  case CombatEventType::FlashEnd: return "flash_blind_end";
  case CombatEventType::WeaponFire: return "weapon_fire";
  case CombatEventType::Damage: return "damage";
  case CombatEventType::Kill: return "kill";
  }
  return "unknown";
}

constexpr const char *combatEventEvidence(CombatEventType type) {
  switch (type) {
  case CombatEventType::FlashStart: return "screen_fade";
  case CombatEventType::FlashEnd: return "flash_timer";
  case CombatEventType::WeaponFire: return "clip_decrease";
  case CombatEventType::Damage: return "damage_message";
  case CombatEventType::Kill: return "death_message";
  }
  return "unknown";
}

} // namespace ai
